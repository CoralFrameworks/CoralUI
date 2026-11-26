/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ui/Accessibility.h>
#include <ui/Layer.h>
#include <ui/Motion.h>
#include <ui/Window.h>

#include <ui/Theme.h>

#include "WidgetDrawing.h"
#include <ui/CanvasBackend.h>

#include <graphics/Bitmap.h>
#include <graphics/Path.h>
#include <graphics/Paint.h>
#include <gpu/DrawList.h>
#include <geometry/RoundRect.h>

#include <algorithm>
#include <atomic>
#include <list>
#include <mutex>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace coral::ui {

namespace detail {

struct LayerRecording {
  /**
   * The recording, split into segments so a change below one child does not throw the whole list away.
   *
   * Segment 0 is the layer own paint (background, shadow, self); segments 1..N are the N children, in
   * order. A layer whose draw needs a composite of the whole group (opacity, blend mode, a rounded or
   * shaped clip mask) keeps everything in segment 0 and is invalidated as a whole, because splitting a
   * `saveLayer`/`maskLayer` pair across segments would change what is composited.
   */
  /**
   * The segments, each an immutable list that can be lent to a frame without copying.
   *
   * Immutability is the point: the frame assembly hands a boundary's lists to the surface by reference,
   * so a later change has to replace the pointer rather than mutate a list an assembled frame may hold.
   */
  std::vector<std::shared_ptr<const graphics::gpu::DrawList>> segments;
  /** Estimated bytes this recording occupies in the budget; filled in when it is cached. */
  std::size_t bytes = 0;
  /** Child count at record time: a structural change drops the recording instead of patching it. */
  std::size_t childCount = 0;
  /** One flag per child segment; a set flag means that segment must be re-recorded before replay. */
  std::vector<uint8_t> dirtySegments;
  /**
   * The child each segment belongs to, in *paint* order (z-index sorted, stable).
   *
   * Storing the owner rather than an index into `mChildren` is what lets the cached path paint in the
   * same order as the uncached one: recording children in insertion order made a cached container
   * ignore `zIndex`, so a popup inside a small card could end up behind its sibling. Any structural
   * change still drops the recording, so these pointers cannot outlive their children.
   */
  std::vector<Layer*> owners;
};

} // namespace detail

namespace {

/** Repaint boundaries that redid drawing work in the current frame; reset by the window. */
thread_local uint32_t gRepaintedBoundaries = 0;
/** Bytes of recorded lists copied into the frame this frame; the R3 payoff metric. */
thread_local std::size_t gFrameListBytes = 0;
/** Bytes of recorded lists lent to the frame this frame; the complement of the copied bytes. */
thread_local std::size_t gLentListBytes = 0;
/** Children skipped because their paint rectangle was outside the clip; reset by the window. */
thread_local uint32_t gCulledLayers = 0;
/** The draw-culling switch; tests flip it to compare captures with and without culling. */
std::atomic<bool> gDrawCullingEnabled{ true };
/** Draw pass id for the `hasRunningAnimation` memo; starts at 1 so a fresh layer (0) always recomputes. */
thread_local uint32_t gAnimationFrame = 1;

/** The children in paint order: `zIndex` ascending, ties broken by insertion order. */
std::vector<Layer*> paintOrderOf(const std::vector<std::shared_ptr<Layer>>& children) {
  std::vector<Layer*> ordered;
  ordered.reserve(children.size());
  for (const auto& child: children) ordered.push_back(child.get());
  std::stable_sort(ordered.begin(), ordered.end(),
    [](const Layer* lhs, const Layer* rhs) { return lhs->zIndex() < rhs->zIndex(); });
  return ordered;
}

namespace {
/** Recomputes `subtreeHasBoundary` along an ancestor chain after a structural change. */
void refreshBoundaryAncestry(Layer* layer) {
  for (Layer* ancestor = layer; ancestor != nullptr; ancestor = ancestor->parent()) {
    bool descendant = false;
    for (const std::shared_ptr<Layer>& child: ancestor->children()) {
      if (child != nullptr && (child->isRepaintBoundary() || child->subtreeHasBoundary())) {
        descendant = true;
        break;
      }
    }
    const bool subtree = descendant || ancestor->isRepaintBoundary();
    if (subtree == ancestor->subtreeHasBoundary() &&
        descendant == ancestor->hasBoundaryDescendant()) {
      break;
    }
    ancestor->noteSubtreeHasBoundary(subtree);
    ancestor->noteHasBoundaryDescendant(descendant);
  }
}
} // namespace anonymous

constexpr float kPi = 3.14159265358979323846f;


/** True when @p target is @p root or lives inside it. */
bool subtreeContains(const Layer* root, const Layer* target) {
  if (root == target) return true;
  for (const auto& child: root->children()) {
    if (subtreeContains(child.get(), target)) return true;
  }
  return false;
}

/**
 * Per-subtree area cap for recordings.
 *
 * 512^2 is the measured value: replaying a much larger list costs more per frame than re-recording its
 * children on the editor scene (p50 0.96 -> 1.09 ms at 2048^2). A value of 0 removes the cap, which is
 * what the budget experiments use; a negative value disables recording altogether.
 */
std::atomic<float> gRecordingCacheAreaLimit{ 512.0f * 512.0f };

/** Aggregate memory budget; see `Layer::setRecordingCacheBudgetBytes`. */
constexpr std::size_t kDefaultRecordingBudgetBytes = 64u * 1024u * 1024u;
std::atomic<std::size_t> gRecordingCacheBudget{ kDefaultRecordingBudgetBytes };
std::atomic<std::size_t> gRecordingCacheBytes{ 0 };
std::atomic<std::size_t> gRecordingCacheEntries{ 0 };
/** Child segments re-recorded in place; see `Layer::recordingSegmentRebuildCount`. */
std::atomic<std::size_t> gRecordingSegmentRebuilds{ 0 };

/**
 * Least-recently-used order of the layers that currently hold a recording.
 *
 * The UI thread is the only writer (recording, invalidation and destruction happen there), but the
 * counters are read from tests and benchmarks, so the list is guarded by a mutex rather than being
 * lock-free. Entries are removed on release and in the destructor, so a pointer cannot outlive its
 * layer.
 */
std::mutex gRecordingCacheMutex;
std::list<Layer*> gRecordingCacheLru;

/** True when a subtree of @p areaPixels may be recorded at all. */
bool recordingAllowedForArea(const float areaPixels) {
  const float limit = gRecordingCacheAreaLimit.load(std::memory_order_acquire);
  if (limit < 0.0f) return false;
  return limit == 0.0f || areaPixels <= limit;
}

/** Approximate bytes a recorded segment occupies: geometry grows, bitmaps are shared images. */
std::size_t estimatedSegmentBytes(const graphics::gpu::DrawList& list) {
  std::size_t bytes = 0;
  bytes += list.commands.capacity() * sizeof(decltype(list.commands)::value_type);
  bytes += list.contours.capacity() * sizeof(decltype(list.contours)::value_type);
  bytes += list.points.capacity() * sizeof(decltype(list.points)::value_type);
  bytes += list.maskPixels.capacity();
  bytes += list.clipMasks.capacity() * sizeof(decltype(list.clipMasks)::value_type);
  bytes += list.layers.capacity() * sizeof(decltype(list.layers)::value_type);
  bytes += list.images.capacity() * sizeof(decltype(list.images)::value_type);
  bytes += list.imageHashes.capacity() * sizeof(decltype(list.imageHashes)::value_type);
  bytes += list.externalImages.capacity() * sizeof(decltype(list.externalImages)::value_type);
  return bytes;
}/**
 * Appends a recorded list to @p target and charges its bytes to the frame.
 *
 * Every appended list is a copy into the frame; that copy is exactly what R3 would remove by handing
 * the frame a reference, so the bytes are counted here.
 */
template <typename Target>
bool appendListCounted(Target& target, const graphics::gpu::DrawList& list) {
  if (!target.appendRecording(list)) return false;
  gFrameListBytes += estimatedSegmentBytes(list);
  return true;
}

std::size_t estimatedRecordingBytes(
    const std::vector<std::shared_ptr<const graphics::gpu::DrawList>>& segments) {
  std::size_t bytes = 0;
  for (const std::shared_ptr<const graphics::gpu::DrawList>& segment : segments) {
    if (segment != nullptr) bytes += estimatedSegmentBytes(*segment);
  }
  return bytes;
}

} // namespace anonymous

void Layer::setRecordingCacheAreaLimit(const float areaPixelsSquared) noexcept {
  gRecordingCacheAreaLimit.store(areaPixelsSquared, std::memory_order_release);
}

float Layer::recordingCacheAreaLimit() noexcept {
  return gRecordingCacheAreaLimit.load(std::memory_order_acquire);
}

void Layer::setRecordingCacheBudgetBytes(const std::size_t bytes) noexcept {
  gRecordingCacheBudget.store(bytes, std::memory_order_release);
}

std::size_t Layer::recordingCacheBudgetBytes() noexcept {
  return gRecordingCacheBudget.load(std::memory_order_acquire);
}

std::size_t Layer::recordingCacheBytesInUse() noexcept {
  return gRecordingCacheBytes.load(std::memory_order_acquire);
}

std::size_t Layer::recordingCacheEntryCount() noexcept {
  return gRecordingCacheEntries.load(std::memory_order_acquire);
}

std::size_t Layer::recordingSegmentRebuildCount() noexcept {
  return gRecordingSegmentRebuilds.load(std::memory_order_acquire);
}

void Layer::releaseRecording() {
  mRecordingValid = false;
  if (mRecording == nullptr) {
    return;
  }
  const std::lock_guard<std::mutex> lock(gRecordingCacheMutex);
  const std::size_t bytes = mRecording->bytes;
  if (bytes != 0) {
    const std::size_t inUse = gRecordingCacheBytes.load(std::memory_order_relaxed);
    gRecordingCacheBytes.store(inUse > bytes ? inUse - bytes : 0, std::memory_order_release);
  }
  gRecordingCacheLru.remove(this);
  if (gRecordingCacheEntries.load(std::memory_order_relaxed) > 0) {
    gRecordingCacheEntries.fetch_sub(1, std::memory_order_acq_rel);
  }
  mRecording.reset();
}

uint32_t Layer::repaintedBoundaries() noexcept { return gRepaintedBoundaries; }
void Layer::resetRepaintedBoundaries() noexcept { gRepaintedBoundaries = 0; }
std::size_t Layer::lentListBytes() noexcept {
  return gLentListBytes;
}

void Layer::resetLentListBytes() noexcept {
  gLentListBytes = 0;
}

std::size_t Layer::frameListBytes() noexcept { return gFrameListBytes; }
void Layer::resetFrameListBytes() noexcept { gFrameListBytes = 0; }

void Layer::setDrawCullingEnabled(const bool enabled) noexcept {
  gDrawCullingEnabled.store(enabled, std::memory_order_release);
}

bool Layer::drawCullingEnabled() noexcept {
  return gDrawCullingEnabled.load(std::memory_order_acquire);
}

uint32_t Layer::culledLayers() noexcept { return gCulledLayers; }
void Layer::resetCulledLayers() noexcept { gCulledLayers = 0; }

void Layer::deferPaint() {
  // A clean layer keeps its recording: it can replay unchanged when it comes back into the clip.
  if (!mNeedsPaint && !mSubtreeNeedsPaint) return;
  releaseRecording();
  mNeedsPaint = false;
  mSubtreeNeedsPaint = false;
  mPaintDeferred = true;
}

bool Layer::markSegmentDirty(const Layer* child) {
  if (mRecording == nullptr || !mRecordingValid) return false;
  const std::size_t childCount = mChildren.size();
  if (mRecording->childCount != childCount || mRecording->segments.size() != childCount + 1 ||
      mRecording->owners.size() != childCount) {
    return false;
  }
  for (std::size_t index = 0; index < childCount; ++index) {
    if (mRecording->owners[index] == child) {
      if (mRecording->dirtySegments.size() != childCount) {
        mRecording->dirtySegments.assign(childCount, 0);
      }
      mRecording->dirtySegments[index] = 1;
      return true;
    }
  }
  return false;
}

void Layer::adjustRecordingBytes(const std::ptrdiff_t delta) {
  if (delta == 0 || mRecording == nullptr) return;
  const std::lock_guard<std::mutex> lock(gRecordingCacheMutex);
  const std::size_t current = mRecording->bytes;
  const std::size_t next = delta < 0
    ? (current > static_cast<std::size_t>(-delta) ? current - static_cast<std::size_t>(-delta) : 0)
    : current + static_cast<std::size_t>(delta);
  const std::size_t inUse = gRecordingCacheBytes.load(std::memory_order_relaxed);
  if (next >= current) {
    gRecordingCacheBytes.store(inUse + (next - current), std::memory_order_release);
  } else {
    gRecordingCacheBytes.store(inUse > (current - next) ? inUse - (current - next) : 0,
      std::memory_order_release);
  }
  mRecording->bytes = next;
}


bool Layer::cacheRecording(std::shared_ptr<detail::LayerRecording> recording, const std::size_t bytes) {
  const std::size_t budget = gRecordingCacheBudget.load(std::memory_order_acquire);
  if (recording == nullptr || bytes == 0 || budget == 0 || bytes > budget) {

    return false;
  }
  if (isRepaintBoundary()) ++gRepaintedBoundaries;
  releaseRecording();
  const std::lock_guard<std::mutex> lock(gRecordingCacheMutex);
  recording->bytes = bytes;
  mRecording = std::move(recording);
  gRecordingCacheLru.push_back(this);
  gRecordingCacheEntries.fetch_add(1, std::memory_order_acq_rel);
  gRecordingCacheBytes.fetch_add(bytes, std::memory_order_acq_rel);
  // Evict least recently used entries until the budget holds. The layer just added cannot be the only
  // victim: `bytes > budget` was rejected above, so if it alone fits the loop always has another.
  while (gRecordingCacheBytes.load(std::memory_order_relaxed) > budget &&
         !gRecordingCacheLru.empty()) {
    Layer* victim = gRecordingCacheLru.front();
    gRecordingCacheLru.pop_front();
    if (victim->mRecording != nullptr) {
      const std::size_t victimBytes = victim->mRecording->bytes;
      if (victimBytes != 0) {
        const std::size_t inUse = gRecordingCacheBytes.load(std::memory_order_relaxed);
        gRecordingCacheBytes.store(inUse > victimBytes ? inUse - victimBytes : 0,
          std::memory_order_release);
      }
      gRecordingCacheEntries.fetch_sub(1, std::memory_order_acq_rel);
    }
    victim->mRecording.reset();
    victim->mRecordingValid = false;
    // It has to be re-recorded, and its area repainted, on its next draw.
    victim->mNeedsPaint = true;
    victim->addDamage(victim->mBounds);
  }
  return true;
}

namespace {

bool hasTransform(const Point& translation, const float scaleX, const float scaleY,
    const float rotation) {
  return translation.x != 0.0f || translation.y != 0.0f || scaleX != 1.0f ||
    scaleY != 1.0f || rotation != 0.0f;
}

void applyTransform(graphics::Canvas& canvas, const Rect& bounds, const Point& translation,
    const float scaleX, const float scaleY, const float rotation, const Point& origin) {
  const Point center{ bounds.getLeft() + bounds.getWidth() * origin.x,
    bounds.getTop() + bounds.getHeight() * origin.y };
  canvas.translate(center.x + translation.x, center.y + translation.y);
  if (rotation != 0.0f) {
    canvas.rotate(rotation);
  }
  canvas.scale(scaleX, scaleY);
  canvas.translate(-center.x, -center.y);
}

Rect transformedBounds(const Rect& bounds, const Point& translation, const float scaleX,
    const float scaleY, const float rotation, const Point& origin) {
  const Point center{ bounds.getLeft() + bounds.getWidth() * origin.x,
    bounds.getTop() + bounds.getHeight() * origin.y };
  const float radians = rotation * kPi / 180.0f;
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  const std::array<Point, 4> corners = {
    Point{ bounds.getLeft(), bounds.getTop() },
    Point{ bounds.getRight(), bounds.getTop() },
    Point{ bounds.getRight(), bounds.getBottom() },
    Point{ bounds.getLeft(), bounds.getBottom() },
  };
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;
  for (size_t index = 0; index < corners.size(); ++index) {
    const Point relative{ (corners[index].x - center.x) * scaleX,
      (corners[index].y - center.y) * scaleY };
    const Point transformed{ center.x + translation.x + relative.x * cosine - relative.y * sine,
      center.y + translation.y + relative.x * sine + relative.y * cosine };
    if (index == 0) {
      left = right = transformed.x;
      top = bottom = transformed.y;
    } else {
      left = std::min(left, transformed.x);
      top = std::min(top, transformed.y);
      right = std::max(right, transformed.x);
      bottom = std::max(bottom, transformed.y);
    }
  }
  return Rect::MakeLTRB(left, top, right, bottom);
}

Point inverseTransform(const Point& point, const Rect& bounds, const Point& translation,
    const float scaleX, const float scaleY, const float rotation, const Point& origin) {
  const Point center{ bounds.getLeft() + bounds.getWidth() * origin.x,
    bounds.getTop() + bounds.getHeight() * origin.y };
  Point local{ point.x - (center.x + translation.x), point.y - (center.y + translation.y) };
  if (rotation != 0.0f) {
    const float radians = -rotation * kPi / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    local = { local.x * cosine - local.y * sine, local.x * sine + local.y * cosine };
  }
  if (scaleX != 0.0f) local.x /= scaleX;
  if (scaleY != 0.0f) local.y /= scaleY;
  return { local.x + center.x, local.y + center.y };
}


graphics::Path roundedRectPath(const Rect& bounds, const float radius) {
  const float r = std::clamp(radius, 0.0f, std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f);
  if (r <= 0.0f) {
    graphics::Path path;
    path.moveTo({ bounds.getLeft(), bounds.getTop() });
    path.lineTo({ bounds.getRight(), bounds.getTop() });
    path.lineTo({ bounds.getRight(), bounds.getBottom() });
    path.lineTo({ bounds.getLeft(), bounds.getBottom() });
    path.close();
    return path;
  }
  const float k = r * 0.5522848f;
  graphics::Path path;
  path.moveTo({ bounds.getLeft() + r, bounds.getTop() });
  path.lineTo({ bounds.getRight() - r, bounds.getTop() });
  path.cubicTo({ bounds.getRight() - r + k, bounds.getTop() },
    { bounds.getRight(), bounds.getTop() + r - k }, { bounds.getRight(), bounds.getTop() + r });
  path.lineTo({ bounds.getRight(), bounds.getBottom() - r });
  path.cubicTo({ bounds.getRight(), bounds.getBottom() - r + k },
    { bounds.getRight() - r + k, bounds.getBottom() },
    { bounds.getRight() - r, bounds.getBottom() });
  path.lineTo({ bounds.getLeft() + r, bounds.getBottom() });
  path.cubicTo({ bounds.getLeft() + r - k, bounds.getBottom() },
    { bounds.getLeft(), bounds.getBottom() - r + k },
    { bounds.getLeft(), bounds.getBottom() - r });
  path.lineTo({ bounds.getLeft(), bounds.getTop() + r });
  path.cubicTo({ bounds.getLeft(), bounds.getTop() + r - k },
    { bounds.getLeft() + r - k, bounds.getTop() }, { bounds.getLeft() + r, bounds.getTop() });
  path.close();
  return path;
}

} // namespace anonymous

Rect Layer::paintBounds() const {
  Rect bounds = mBounds;
  if (mShadow.has_value()) {
    // `drawShadow` grows the rectangle by `blurRadius` and shifts it by the offset; the halo has to
    // cover the largest ring or a popup whose shadow reaches into the clip would be culled away.
    const float grow = mShadowRadius + mShadow->blurRadius;
    bounds = Rect::MakeLTRB(bounds.getLeft() - grow + mShadow->offsetX,
      bounds.getTop() - grow + mShadow->offsetY,
      bounds.getRight() + grow + mShadow->offsetX,
      bounds.getBottom() + grow + mShadow->offsetY);
  }
  if (mPaintBoundsInflation > 0.0f) {
    bounds = Rect::MakeLTRB(bounds.getLeft() - mPaintBoundsInflation,
      bounds.getTop() - mPaintBoundsInflation,
      bounds.getRight() + mPaintBoundsInflation,
      bounds.getBottom() + mPaintBoundsInflation);
  }
  if (hasTransform(mTranslation, mScaleX, mScaleY, mRotation)) {
    bounds = transformedBounds(bounds, mTranslation, mScaleX, mScaleY, mRotation, mTransformOrigin);
  }
  return bounds;
}

bool Layer::intersectsClip(const float left, const float top, const float right, const float bottom) const {
  if (hasTransform(mTranslation, mScaleX, mScaleY, mRotation) || mShadow.has_value()) {
    // Rare cases: fall back to the full paint rectangle, which already accounts for both.
    const Rect paint = paintBounds();
    return paint.getRight() >= left && paint.getLeft() <= right &&
      paint.getBottom() >= top && paint.getTop() <= bottom;
  }
  const float grow = mPaintBoundsInflation;
  return mBounds.getRight() + grow >= left && mBounds.getLeft() - grow <= right &&
    mBounds.getBottom() + grow >= top && mBounds.getTop() - grow <= bottom;
}

Layer::~Layer() {
  releaseRecording();
}

namespace {
float ease(const float t, const Easing easing) {
  switch (easing) {
    case Easing::Linear: return t;
    case Easing::EaseIn: return t * t;
    case Easing::EaseOut: return 1.0f - (1.0f - t) * (1.0f - t);
    case Easing::EaseInOut:
      return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
  }
  return t;
}
} // namespace anonymous

void Layer::setOpacity(const float opacity) {
  if (mTransition.active()) {
    animateOpacity(opacity, mTransition.duration, mTransition.easing);
    return;
  }
  cancelAnimation(AnimationTarget::Opacity);
  const float next = std::clamp(opacity, 0.0f, 1.0f);
  if (mOpacity == next) {
    return;
  }
  mOpacity = next;
  setNeedsPaint();
}

void Layer::setOpacity(const float opacity, const std::chrono::milliseconds duration,
    const Easing easing) {
  animateOpacity(opacity, duration, easing);
}

void Layer::setBackgroundColor(const Color color) {
  if (mTransition.active() && !reduceMotion()) {
    if (mBackground == color && mBackgroundRole == ThemeColorRole::None) return;
    animateBackgroundColor(color, mTransition.duration, mTransition.easing);
    return;
  }
  mColorAnimation.active = false;
  if (mBackground == color && mBackgroundRole == ThemeColorRole::None) return;
  mBackgroundRole = ThemeColorRole::None;
  mBackground = color;
  setNeedsPaint();
}

void Layer::animateBackgroundColor(const Color target, const std::chrono::milliseconds duration,
    const Easing easing) {
  if (duration.count() <= 0 || reduceMotion()) {
    mColorAnimation.active = false;
    mBackgroundRole = ThemeColorRole::None;
    mBackground = target;
    setNeedsPaint();
    return;
  }
  mColorAnimation = ColorAnimation{ mBackground, target, std::chrono::steady_clock::now(), duration,
    easing, true };
  mBackgroundRole = ThemeColorRole::None;
  invalidateAnimationCache();
  setNeedsPaint();
}

void Layer::setBackgroundColorRole(const ThemeColorRole role) {
  if (mBackgroundRole == role) return;
  mColorAnimation.active = false;
  mBackgroundRole = role;
  if (role != ThemeColorRole::None && mTheme != nullptr) {
    mBackground = themeColor(*mTheme, role);
  }
  setNeedsPaint();
}

void Layer::setTranslation(const Point& translation) {
  invalidateClipMask();
  if (mTransition.active()) {
    animateTranslation(translation, mTransition.duration, mTransition.easing);
    return;
  }
  cancelAnimation(AnimationTarget::TranslationX);
  cancelAnimation(AnimationTarget::TranslationY);
  if (mTranslation == translation) {
    return;
  }
  addDamage(mBounds); // old position, with the old translation
  mTranslation = translation;
  setNeedsPaint();
}

void Layer::setTranslation(const Point& translation, const std::chrono::milliseconds duration,
    const Easing easing) {
  animate(AnimationTarget::TranslationX, mTranslation.x, translation.x, duration, easing);
  animate(AnimationTarget::TranslationY, mTranslation.y, translation.y, duration, easing);
}

void Layer::setScale(const float scale) {
  setScale(scale, scale);
}

void Layer::setScale(const float scale, const std::chrono::milliseconds duration,
    const Easing easing) {
  setScale(scale, scale, duration, easing);
}

void Layer::setScale(const float scaleX, const float scaleY) {
  invalidateClipMask();
  if (mTransition.active()) {
    animate(AnimationTarget::ScaleX, mScaleX, scaleX, mTransition.duration, mTransition.easing);
    animate(AnimationTarget::ScaleY, mScaleY, scaleY, mTransition.duration, mTransition.easing);
    return;
  }
  cancelAnimation(AnimationTarget::ScaleX);
  cancelAnimation(AnimationTarget::ScaleY);
  if (mScaleX == scaleX && mScaleY == scaleY) {
    return;
  }
  mScaleX = scaleX;
  mScaleY = scaleY;
  mDamageUnbounded = true;
  setNeedsPaint();
}

void Layer::setScale(const float scaleX, const float scaleY,
    const std::chrono::milliseconds duration, const Easing easing) {
  animate(AnimationTarget::ScaleX, mScaleX, scaleX, duration, easing);
  animate(AnimationTarget::ScaleY, mScaleY, scaleY, duration, easing);
}

void Layer::setRotation(const float degrees) {
  invalidateClipMask();
  if (mTransition.active()) {
    animateRotation(degrees, mTransition.duration, mTransition.easing);
    return;
  }
  cancelAnimation(AnimationTarget::Rotation);
  if (mRotation == degrees) {
    return;
  }
  mRotation = degrees;
  mDamageUnbounded = true;
  setNeedsPaint();
}

void Layer::setRotation(const float degrees, const std::chrono::milliseconds duration,
    const Easing easing) {
  animateRotation(degrees, duration, easing);
}

void Layer::animateOpacity(const float target, const std::chrono::milliseconds duration,
    const Easing easing) {
  animate(AnimationTarget::Opacity, mOpacity, std::clamp(target, 0.0f, 1.0f), duration, easing);
}

void Layer::animateTranslation(const Point& target, const std::chrono::milliseconds duration,
    const Easing easing) {
  animate(AnimationTarget::TranslationX, mTranslation.x, target.x, duration, easing);
  animate(AnimationTarget::TranslationY, mTranslation.y, target.y, duration, easing);
}

void Layer::animateScale(const float target, const std::chrono::milliseconds duration,
    const Easing easing) {
  animate(AnimationTarget::ScaleX, mScaleX, target, duration, easing);
  animate(AnimationTarget::ScaleY, mScaleY, target, duration, easing);
}

void Layer::animateRotation(const float target, const std::chrono::milliseconds duration,
    const Easing easing) {
  animate(AnimationTarget::Rotation, mRotation, target, duration, easing);
}

void Layer::animate(const AnimationTarget target, const float from, const float to,
    const std::chrono::milliseconds duration, const Easing easing) {
  const auto now = std::chrono::steady_clock::now();
  // A declarative state setter can be called every frame by a binding; do not restart an animation
  // that is already travelling to the same destination.
  for (const Animation& animation: mAnimations) {
    if (animation.target == target && animation.to == to && now < animation.start + animation.duration) {
      return;
    }
  }
  cancelAnimation(target);
  if (duration.count() <= 0) {
    switch (target) {
      case AnimationTarget::Opacity: mOpacity = to; break;
      case AnimationTarget::TranslationX: mTranslation.x = to; break;
      case AnimationTarget::TranslationY: mTranslation.y = to; break;
      case AnimationTarget::ScaleX: mScaleX = to; break;
      case AnimationTarget::ScaleY: mScaleY = to; break;
      case AnimationTarget::Rotation: mRotation = to; break;
    }
    if (target != AnimationTarget::Opacity) {
      invalidateClipMask();
    }
    setNeedsPaint();
    return;
  }
  mAnimations.push_back(Animation{ target, from, to, now, duration, easing });
  invalidateAnimationCache();
  setNeedsPaint();
}

void Layer::cancelAnimation(const AnimationTarget target) {
  std::erase_if(mAnimations, [target](const Animation& animation) {
    return animation.target == target;
  });
}

void Layer::animateBounds(const Rect& target, const std::chrono::milliseconds duration,
    std::shared_ptr<const Curve> curve) {
  if (duration.count() <= 0 || reduceMotion()) {
    mBoundsAnimation.active = false;
    setBounds(target);
    return;
  }
  mBoundsAnimation.from = mBounds;
  mBoundsAnimation.to = target;
  mBoundsAnimation.start = std::chrono::steady_clock::now();
  mBoundsAnimation.duration = duration;
  mBoundsAnimation.curve = curve != nullptr ? std::move(curve) : Curve::easeInOut();
  mBoundsAnimation.active = true;
  invalidateAnimationCache();
  addDamage(mBounds);
  addDamage(target);
  setNeedsLayout();
}

void Layer::updateAnimations(const std::chrono::steady_clock::time_point now) {
  if (mBoundsAnimation.active) {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - mBoundsAnimation.start).count();
    const float t = mBoundsAnimation.duration.count() > 0
      ? std::clamp(static_cast<float>(elapsed) /
            static_cast<float>(mBoundsAnimation.duration.count()), 0.0f, 1.0f)
      : 1.0f;
    const float eased = mBoundsAnimation.curve->transform(t);
    const Rect& from = mBoundsAnimation.from;
    const Rect& to = mBoundsAnimation.to;
    const Rect next = Rect::MakeLTRB(from.getLeft() + (to.getLeft() - from.getLeft()) * eased,
      from.getTop() + (to.getTop() - from.getTop()) * eased,
      from.getRight() + (to.getRight() - from.getRight()) * eased,
      from.getBottom() + (to.getBottom() - from.getBottom()) * eased);
    setBounds(next);
    if (t >= 1.0f) mBoundsAnimation.active = false;
  }
  if (mColorAnimation.active) {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - mColorAnimation.start).count();
    const float t = std::clamp(static_cast<float>(elapsed) /
        static_cast<float>(std::max<int64_t>(1, mColorAnimation.duration.count())), 0.0f, 1.0f);
    const float eased = ease(t, mColorAnimation.easing);
    const Color& from = mColorAnimation.from;
    const Color& to = mColorAnimation.to;
    const auto mix = [eased](const float a, const float b) { return a + (b - a) * eased; };
    mBackground = Color{ mix(from.red, to.red), mix(from.green, to.green), mix(from.blue, to.blue),
      mix(from.alpha, to.alpha) };
    if (t >= 1.0f) {
      mBackground = to;
      mColorAnimation.active = false;
    }
    setNeedsPaint();
  }
  if (mAnimations.empty()) {
    return;
  }
  bool changed = false;
  bool transformChanged = false;
  bool translationChanged = false;
  for (const Animation& animation: mAnimations) {
    if (animation.target == AnimationTarget::TranslationX ||
        animation.target == AnimationTarget::TranslationY) {
      translationChanged = true;
    }
  }
  if (translationChanged) addDamage(mBounds); // old position, with the old translation
  for (Animation& animation: mAnimations) {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - animation.start).count();
    const float t = animation.duration.count() > 0
      ? std::clamp(static_cast<float>(elapsed) / static_cast<float>(animation.duration.count()),
          0.0f, 1.0f)
      : 1.0f;
    const float value = animation.from + (animation.to - animation.from) * ease(t, animation.easing);
    switch (animation.target) {
      case AnimationTarget::Opacity: mOpacity = value; break;
      case AnimationTarget::TranslationX: mTranslation.x = value; transformChanged = true; break;
      case AnimationTarget::TranslationY: mTranslation.y = value; transformChanged = true; break;
      case AnimationTarget::ScaleX: mScaleX = value; transformChanged = true; mDamageUnbounded = true; break;
      case AnimationTarget::ScaleY: mScaleY = value; transformChanged = true; mDamageUnbounded = true; break;
      case AnimationTarget::Rotation: mRotation = value; transformChanged = true; mDamageUnbounded = true; break;
    }
    changed = true;
  }
  std::erase_if(mAnimations, [now](const Animation& animation) {
    return now - animation.start >= animation.duration;
  });
  if (transformChanged) {
    invalidateClipMask();
  }
  if (changed) {
    setNeedsPaint();
  }
}

void Layer::setClipRadius(const float radius) {
  const float next = std::max(0.0f, radius);
  if (mClipRadius == next) {
    return;
  }
  mClipRadius = next;
  invalidateClipMask();
  setNeedsPaint();
}

void Layer::setBlendMode(const graphics::BlendMode mode) {
  if (mBlendMode == mode) {
    return;
  }
  mBlendMode = mode;
  setNeedsPaint();
}

void Layer::setShadow(const Shadow shadow, const float cornerRadius) {
  mShadow = shadow;
  mShadowRadius = std::max(0.0f, cornerRadius);
  setNeedsPaint();
}

void Layer::clearShadow() {
  if (!mShadow.has_value()) return;
  mShadow.reset();
  setNeedsPaint();
}

void Layer::setTransformOrigin(const Point& origin) {
  const Point next{ std::clamp(origin.x, 0.0f, 1.0f), std::clamp(origin.y, 0.0f, 1.0f) };
  if (mTransformOrigin == next) {
    return;
  }
  mTransformOrigin = next;
  invalidateClipMask();
  setNeedsPaint();
}

void Layer::setClipPath(graphics::Path path) {
  mClipPath = std::move(path);
  ++mClipPathRevision;
  invalidateClipMask();
  setNeedsPaint();
}

void Layer::clearClipPath() {
  if (mClipPath.isEmpty()) {
    return;
  }
  mClipPath = graphics::Path{};
  ++mClipPathRevision;
  invalidateClipMask();
  setNeedsPaint();
}

void Layer::invalidateClipMask() {
  mClipMask.reset();
  mClipMaskRadius = -1.0f;
}

Rect Layer::boundsInWindow() const {
  Rect result = mBounds;
  for (const Layer* layer = this; layer != nullptr; layer = layer->parent()) {
    const Point offset = layer->translation();
    result = Rect::MakeXYWH(result.getLeft() + offset.x, result.getTop() + offset.y,
      result.getWidth(), result.getHeight());
  }
  return result;
}

void Layer::setBounds(const Rect& bounds) {
  if (mBounds == bounds) {
    return;
  }
  invalidateClipMask();
  addDamage(mBounds);
  const bool sizeChanged = mBounds.getWidth() != bounds.getWidth() ||
                           mBounds.getHeight() != bounds.getHeight();
  mBounds = bounds;
  if (sizeChanged) onBoundsSizeChanged();
  setNeedsPaint();
  if (sizeChanged) {
    setNeedsLayout();
  }
}

void Layer::setVisible(const bool visible) {
  if (mVisible == visible) {
    return;
  }
  mVisible = visible;
  setNeedsLayout();
  setNeedsPaint();
  onVisibilityChanged(visible);
}

void Layer::attachToWindow(Window* window) {
  if (mWindow == window) {
    for (const std::shared_ptr<Layer>& child: mChildren) child->attachToWindow(window);
    return;
  }
  Window* previous = mWindow;
  mWindow = window;
  onWindowChanged(previous, window);
  for (const std::shared_ptr<Layer>& child: mChildren) child->attachToWindow(window);
}

void Layer::addChild(std::shared_ptr<Layer> child) {
  if (child == nullptr) {
    return;
  }
  child->mParent = this;
  ++child->mReparents;
  child->setTheme(mTheme);
  if (!child->mStyleSheetExplicit) child->inheritStyleSheet(mStyleSheet);
  child->attachToWindow(mWindow);
  child->mParentAssignedSize = false;
  mChildren.push_back(std::move(child));
  if (mChildren.back()->isRepaintBoundary() || mChildren.back()->subtreeHasBoundary()) {
    refreshBoundaryAncestry(this);
  }
  setNeedsLayout();
  setNeedsPaint();
  notifyAccessibilityEvent(*this, AccessibilityEvent::ChildrenAdded);
}

void Layer::setTheme(std::shared_ptr<const Theme> theme) {
  mTheme = std::move(theme);
  if (mTheme != nullptr) {
    mFocusRingWidth = mTheme->focusRingWidth;
    applyTheme(*mTheme);
  }
  const auto inherited = mTheme;
  for (const auto& child: mChildren) {
    child->setTheme(inherited);
  }
}

bool Layer::performAccessibilityAction(const AccessibilityAction action, const std::string_view value) {
  if (action == AccessibilityAction::Confirm) {
    return performAccessibilityAction(AccessibilityAction::Invoke, value);
  }
  if (action == AccessibilityAction::Cancel) return false;
  (void)value;
  if (action == AccessibilityAction::Select && mAccessibleOnSelect) {
    mAccessibleOnSelect();
    return true;
  }
  return false;
}

void Layer::applyTheme(const Theme& theme) {
  if (mBackgroundRole != ThemeColorRole::None) {
    mBackground = themeColor(theme, mBackgroundRole);
    setNeedsPaint();
  }
}

void Layer::setLayoutParams(const LayoutParams& params) {
  if (sameLayoutParams(mLayoutParams, params)) return;
  mLayoutParams = params;
  // The next layout pass decides this again; until then the layer must not claim its size is fixed.
  mParentAssignedSize = false;
  setNeedsLayout();
}


void Layer::setRepaintBoundary(const bool boundary) {
  if (mRepaintBoundary == boundary) return;
  mRepaintBoundary = boundary;
  refreshBoundaryAncestry(mParent != nullptr ? mParent : this);
  // The ancestors' recordings were allowed to flatten this subtree; now they must not, so drop them
  // instead of leaving a list that still contains a copy of this layer's content.
  for (Layer* ancestor = mParent; ancestor != nullptr; ancestor = ancestor->mParent) {
    ancestor->releaseRecording();
    ancestor->mSubtreeNeedsPaint = true;
  }
  setNeedsPaint();
}

bool Layer::hasRepaintBoundaryDescendant() const {
  for (const std::shared_ptr<Layer>& child: mChildren) {
    if (child->mRepaintBoundary || child->hasRepaintBoundaryDescendant()) return true;
  }
  return false;
}

void Layer::setClip(const bool clip) {
  if (mClip == clip) return;
  mClip = clip;
  setNeedsPaint();
}

void Layer::insertChild(const size_t index, std::shared_ptr<Layer> child) {
  if (child == nullptr || child.get() == this) {
    return;
  }
  child->mParent = this;
  ++child->mReparents;
  child->setTheme(mTheme);
  if (!child->mStyleSheetExplicit) child->inheritStyleSheet(mStyleSheet);
  // A keyed reorder removes the child (which clears the window back-pointer) and inserts it again;
  // without this the moved subtree would keep window() == null and lose window services (post,
  // clipboard routing, drag sessions, IME anchoring).
  child->attachToWindow(mWindow);
  child->mParentAssignedSize = false;
  const size_t clamped = std::min(index, mChildren.size());
  mChildren.insert(mChildren.begin() + static_cast<std::ptrdiff_t>(clamped), std::move(child));
  if (mChildren[clamped]->isRepaintBoundary() || mChildren[clamped]->subtreeHasBoundary()) {
    refreshBoundaryAncestry(this);
  }
  setNeedsLayout();
  setNeedsPaint();
  notifyAccessibilityEvent(*this, AccessibilityEvent::ChildrenAdded);
}

void Layer::removeChild(const std::shared_ptr<Layer>& child) {
  const auto found = std::find(mChildren.begin(), mChildren.end(), child);
  if (found == mChildren.end()) {
    return;
  }
  // A focused node must not outlive its place in the tree: the window keeps a bare pointer, and the
  // reconciler removes the old subtree before mounting the new one. Clear focus first so the layer can
  // still reach window services inside `onFocusChanged(false)`.
  if (mWindow != nullptr) {
    Layer* focused = mWindow->focusedLayer();
    if (focused != nullptr && subtreeContains(found->get(), focused)) mWindow->focusLayer(nullptr);
    // A hovered/pressed row can be recycled during a mouse drag; those are also bare pointers.
    mWindow->notifyLayerRemoved(found->get());
  }
  (*found)->mParent = nullptr;
  ++(*found)->mReparents;
  const bool hadBoundary = (*found)->isRepaintBoundary() || (*found)->subtreeHasBoundary();
  (*found)->attachToWindow(nullptr);
  mChildren.erase(found);
  if (hadBoundary) refreshBoundaryAncestry(this);
  setNeedsLayout();
  setNeedsPaint();
  notifyAccessibilityEvent(*this, AccessibilityEvent::ChildrenRemoved);
}


Size Layer::measure(const Size& available) {
  (void)available;
  float width = mBounds.getWidth();
  float height = mBounds.getHeight();
  for (const std::shared_ptr<Layer>& child: mChildren) {
    if (!child->visible()) {
      continue;
    }
    const Size childSize = child->measure(available);
    width = std::max(width, childSize.width);
    height = std::max(height, childSize.height);
  }
  setMeasuredSize({ width, height });
  return mMeasured;
}

void Layer::layout() {
  for (const std::shared_ptr<Layer>& child: mChildren) {
    if (child->needsLayout() || child->subtreeNeedsLayout()) {
      child->layout();
    }
  }
  clearNeedsLayout();
}

void Layer::draw(RenderContext& context) {
  if (!mVisible || mOpacity <= 0.0f) return;
  if (mPaintDeferred) {
    // A parent skipped this layer while it was outside the clip. The layer is visible again, so the
    // recording that the skip left behind is stale by construction: drop it and record this frame.
    mPaintDeferred = false;
    releaseRecording();
    mNeedsPaint = true;
    mSubtreeNeedsPaint = true;
  }
  auto& canvas = context.canvas();

  // A boundary owns its own list, so an ancestor must not flatten it: the ancestor stays live and
  // appends the boundary's current list every frame. That is what makes a change inside one boundary
  // stop at that boundary instead of forcing (or silently missing) an ancestor re-record.
  // A plan-mode frame records content into the frame list, but a boundary still records its own list --
  // that list is what the frame borrows on the next frame. Without this the plan could never bootstrap:
  // the first frame would find no lists and nothing would ever be recorded for a boundary to lend.
  const bool planAllowsCaching = !context.assemblingPlan() || isRepaintBoundary();
  const bool cacheWhole = planAllowsCaching && !mChildren.empty() && !hasRunningAnimation() &&
    !hasBoundaryDescendant() &&
    mBounds.getWidth() > 0.0f && mBounds.getHeight() > 0.0f &&
    recordingAllowedForArea(mBounds.getWidth() * mBounds.getHeight()) &&
    canvas.isRecording() && !detail::nativeCanvas(canvas).hasClipPath() && !detail::nativeCanvas(canvas).inRecordingScope();

  if (!cacheWhole) {

    releaseRecording();
    drawUncached(context);
    return;
  }

  const bool transformed = hasTransform(mTranslation, mScaleX, mScaleY, mRotation);
  const bool masked = mClip && (mClipRadius > 0.0f || !mClipPath.isEmpty()) &&
    context.deviceSize().width > 0.0f && context.deviceSize().height > 0.0f &&
    context.capabilities().layerMasks.supported;
  const bool blended = mBlendMode != graphics::BlendMode::kSrcOver &&
    context.capabilities().blendModes.supported;
  // A group that composites as a whole (opacity, blend mode, shaped or rounded mask) cannot be split:
  // the saveLayer/maskLayer pair would end up in one segment and the paint it composites in another.
  const bool segmentable = mOpacity >= 1.0f && !blended && !masked;

  auto& native = detail::nativeCanvas(canvas);
  const graphics::Matrix transform = canvas.transform();
  const Rect clip = canvas.deviceClipBounds();

  const std::size_t childCount = mChildren.size();

  // ## The clip in this layer's logical space, for the segment replay
  //
  // A cached container records one segment per child and appends all of them on every replay. That is
  // fine while the clip shows the whole container, but a container clipped by its parent (a viewport,
  // a card scrolled half out of view) would replay every child's list for pixels the clip throws away.
  // The same rectangle the draw pass culls children with is what the replay skips on.
  float cullLeft = 0.0f;
  float cullTop = 0.0f;
  float cullRight = 0.0f;
  float cullBottom = 0.0f;
  bool haveCullClip = false;
  if (drawCullingEnabled() && transform.isTranslationScaleOnly()) {
    const float scaleX = transform.m[0];
    const float scaleY = transform.m[5];
    if (scaleX != 0.0f && scaleY != 0.0f) {
      cullLeft = (clip.getLeft() - transform.m[12]) / scaleX;
      cullTop = (clip.getTop() - transform.m[13]) / scaleY;
      cullRight = (clip.getRight() - transform.m[12]) / scaleX;
      cullBottom = (clip.getBottom() - transform.m[13]) / scaleY;
      haveCullClip = true;
    }
  }

  const auto beginSegment = [&]() {
    native.beginRecordingScope();
    canvas.save();
    if (transformed) {
      applyTransform(native, mBounds, mTranslation, mScaleX, mScaleY, mRotation, mTransformOrigin);
    }
    if (mClip) canvas.clipRect(mBounds);
  };
  const auto endSegment = [&](graphics::gpu::DrawList& out) {
    canvas.restore();
    native.endRecordingScope(out);
  };
  const auto recordOwn = [&]() {
    if (mShadow.has_value()) {
      detail::drawShadow(canvas, mBounds, mShadowRadius, *mShadow);
    }
    if (mBackground.alpha > 0.0f) {
      graphics::Paint paint;
      paint.color = mBackground;
      paint.style = graphics::Paint::Style::kFill;
      canvas.drawRect(mBounds, paint);
    }
    drawSelfCached(context);
  };

  const auto appendAll = [&]() {
    for (std::size_t index = 0; index < mRecording->segments.size(); ++index) {
      const std::shared_ptr<const graphics::gpu::DrawList>& segment = mRecording->segments[index];
      if (segment == nullptr) continue;
      // Segment 0 is the layer's own paint; the rest are its children in paint order, and a child the
      // clip cannot see contributes nothing -- the same rule `drawChildren` applies when it draws them.
      // A child that paints outside its bounds is never skipped, which is what `clipsToBounds()` asks.
      if (index > 0 && haveCullClip && index - 1 < mRecording->owners.size()) {
        const Layer* owner = mRecording->owners[index - 1];
        if (owner != nullptr && owner->clipsToBounds() &&
            !owner->intersectsClip(cullLeft, cullTop, cullRight, cullBottom)) {
          ++gCulledLayers;
          continue;
        }
      }
      if (!appendListCounted(native, *segment)) return false;
    }
    return true;
  };

  if (segmentable) {
    // Replay: patch only the child segments that changed, then append everything else as recorded.
    if (mRecordingValid && mRecording != nullptr && transform == mRecordingTransform &&
        clip == mRecordingClip && mBounds == mRecordingBounds &&
        mRecording->childCount == childCount &&
        mRecording->segments.size() == childCount + 1 &&
        mRecording->owners.size() == childCount) {
      if (mRecording->dirtySegments.size() != childCount) {
        mRecording->dirtySegments.assign(childCount, 0);
      }
      bool patched = false;
      for (std::size_t index = 0; index < childCount; ++index) {
        if (mRecording->dirtySegments[index] == 0) continue;
        patched = true;
        graphics::gpu::DrawList fresh;
        beginSegment();
        mRecording->owners[index]->draw(context);
        endSegment(fresh);
        const auto replacement =
          std::make_shared<const graphics::gpu::DrawList>(std::move(fresh));
        adjustRecordingBytes(static_cast<std::ptrdiff_t>(estimatedSegmentBytes(*replacement)) -
          static_cast<std::ptrdiff_t>(estimatedSegmentBytes(*mRecording->segments[index + 1])));
        mRecording->segments[index + 1] = replacement;
        mRecording->dirtySegments[index] = 0;
        gRecordingSegmentRebuilds.fetch_add(1, std::memory_order_acq_rel);
      }
      if (appendAll()) {
        // The counter answers "did this boundary redo drawing work in this frame": a patched segment
        // counts, and a plain replay does not. Counting at the setter would be wrong -- the reset
        // happens at frame start.
        if (patched && isRepaintBoundary()) ++gRepaintedBoundaries;
        mNeedsPaint = false;
        mSubtreeNeedsPaint = false;
        return;
      }
      releaseRecording();
    }

    auto recorded = std::make_shared<detail::LayerRecording>();
    recorded->childCount = childCount;
    recorded->segments.resize(childCount + 1);
    recorded->dirtySegments.assign(childCount, 0);
    beginSegment();
    recordOwn();
    {
      graphics::gpu::DrawList own;
      endSegment(own);
      recorded->segments[0] = std::make_shared<const graphics::gpu::DrawList>(std::move(own));
    }
    // Record in paint order, not insertion order: the cached path has to honour `zIndex` exactly like
    // `drawChildren` does, and the owner per segment is what a later `markSegmentDirty` patches.
    for (Layer* child: paintOrderOf(mChildren)) {
      const std::size_t index = recorded->owners.size();
      beginSegment();
      child->draw(context);
      {
        graphics::gpu::DrawList segment;
        endSegment(segment);
        recorded->segments[index + 1] =
          std::make_shared<const graphics::gpu::DrawList>(std::move(segment));
      }
      recorded->owners.push_back(child);
    }
    const std::size_t recordedBytes = estimatedRecordingBytes(recorded->segments);
    if (!cacheRecording(std::move(recorded), recordedBytes)) {
      drawUncached(context);
      return;
    }
    mRecordingTransform = transform;
    mRecordingClip = clip;
    mRecordingBounds = mBounds;
    mRecordingValid = true;
    if (!appendAll()) {
      releaseRecording();
      drawUncached(context);
      return;
    }
    // Recording the list is also painting it: without this the frame that first caches a container
    // stayed "dirty" until the next frame's replay cleared it, which kept the retained frame
    // short-circuit and the damage heuristic from ever seeing a clean tree.
    mNeedsPaint = false;
    mSubtreeNeedsPaint = false;
    return;
  }

  // Legacy path: the whole subtree is one list and is invalidated as one.
  if (mRecordingValid && mRecording != nullptr && transform == mRecordingTransform &&
      clip == mRecordingClip && mBounds == mRecordingBounds &&
      mRecording->segments.size() == 1 &&
      appendListCounted(native, *mRecording->segments[0])) {
    mNeedsPaint = false;
    mSubtreeNeedsPaint = false;
    return;
  }
  auto recorded = std::make_shared<detail::LayerRecording>();
  recorded->segments.resize(1);
  native.beginRecordingScope();
  drawUncached(context);
  {
    graphics::gpu::DrawList segment;
    native.endRecordingScope(segment);
    recorded->segments[0] = std::make_shared<const graphics::gpu::DrawList>(std::move(segment));
  }
  const std::size_t recordedBytes = estimatedRecordingBytes(recorded->segments);
  if (!cacheRecording(std::move(recorded), recordedBytes)) {
    drawUncached(context);
    return;
  }
  mRecordingTransform = transform;
  mRecordingClip = clip;
  mRecordingBounds = mBounds;
  mRecordingValid = true;
  if (!appendListCounted(native, *mRecording->segments[0])) {
    releaseRecording();
    drawUncached(context);
    return;
  }
  mNeedsPaint = false;
  mSubtreeNeedsPaint = false;
}
void Layer::drawUncached(RenderContext& context) {
  auto& canvas = context.canvas();
  canvas.save();
  const bool transformed = hasTransform(mTranslation, mScaleX, mScaleY, mRotation);
  const bool translucent = mOpacity < 1.0f;
  const bool maskedClip = mClip && (mClipRadius > 0.0f || !mClipPath.isEmpty()) &&
    context.deviceSize().width > 0.0f && context.deviceSize().height > 0.0f &&
    context.capabilities().layerMasks.supported;
  const bool blended = mBlendMode != graphics::BlendMode::kSrcOver &&
    context.capabilities().blendModes.supported;
  const bool layered = translucent || maskedClip || blended;
  if (layered) {
    // The layer bounds are in the parent's coordinate space: `Canvas::saveLayer` applies the current
    // transform to them, so the layer must be opened before the child transform is applied.
    const Rect layerBounds = transformed
      ? transformedBounds(mBounds, mTranslation, mScaleX, mScaleY, mRotation, mTransformOrigin)
      : mBounds;
    if (blended) {
      // A blend mode has to composite the *group* once, not each child, so the layer's paint carries
      // both the mode and the group alpha.
      graphics::Paint layerPaint;
      layerPaint.color = graphics::Color::White();
      layerPaint.color.alpha = mOpacity;
      layerPaint.blendMode = mBlendMode;
      canvas.saveLayer(layerBounds, layerPaint);
    } else {
      const auto alpha = static_cast<uint8_t>(std::lround(mOpacity * 255.0f));
      canvas.saveLayerAlpha(layerBounds, alpha);
    }
  }
  if (transformed) {
    applyTransform(detail::nativeCanvas(canvas), mBounds, mTranslation, mScaleX, mScaleY, mRotation, mTransformOrigin);
  }
  if (mShadow.has_value()) {
    detail::drawShadow(canvas, mBounds, mShadowRadius, *mShadow);
  }
  if (mClip) {
    // The rectangular bound is the fast reject for both clips. A rounded clip is applied after the
    // layer is drawn because the GPU renderer supports layer masks but not per-command shaped clips.
    canvas.clipRect(mBounds);
  }
  if (mBackground.alpha > 0.0f) {
    graphics::Paint paint;
    paint.color = mBackground;
    paint.style = graphics::Paint::Style::kFill;
    canvas.drawRect(mBounds, paint);
  }
  drawSelfCached(context);
  drawChildren(context);  if (maskedClip) {
    if (graphics::Bitmap* mask = clipMaskFor(context); mask != nullptr) {
      canvas.maskLayer(*mask, graphics::MaskMode::kAlpha, true);
    }
  }
  if (layered) {
    canvas.restore();
  }
  canvas.restore();
  mNeedsPaint = false;
  mSubtreeNeedsPaint = false;
}

void Layer::drawSelfCached(RenderContext& context) {
  // The recording is device-space, so it is valid only while the canvas transform and clip still
  // match. Both are cheap comparisons against the values captured when the scope closed. A shaped
  // clip is excluded because its mask index belongs to the parent list.
  auto& canvas = context.canvas();
  const bool cacheable = cachesOwnPaint() && mChildren.empty() && !hasRunningAnimation() &&
    mBounds.getWidth() > 0.0f && mBounds.getHeight() > 0.0f &&
    recordingAllowedForArea(mBounds.getWidth() * mBounds.getHeight()) &&
    canvas.isRecording() && !detail::nativeCanvas(canvas).hasClipPath() && !detail::nativeCanvas(canvas).inRecordingScope();
  if (!cacheable) {
    releaseRecording();
    drawSelf(context);
    return;
  }

  const graphics::Matrix transform = canvas.transform();
  const Rect clip = canvas.deviceClipBounds();

  if (mRecordingValid && mRecording != nullptr && transform == mRecordingTransform &&
      clip == mRecordingClip && mBounds == mRecordingBounds &&
      mRecording->segments.size() == 1 &&
      appendListCounted(detail::nativeCanvas(canvas), *mRecording->segments[0])) {
    return;
  }

  auto recorded = std::make_shared<detail::LayerRecording>();
  recorded->segments.resize(1);
  detail::nativeCanvas(canvas).beginRecordingScope();
  drawSelf(context);
  {
    graphics::gpu::DrawList segment;
    detail::nativeCanvas(canvas).endRecordingScope(segment);
    recorded->segments[0] = std::make_shared<const graphics::gpu::DrawList>(std::move(segment));
  }
  const std::size_t recordedBytes = estimatedRecordingBytes(recorded->segments);
  if (!cacheRecording(std::move(recorded), recordedBytes)) {
    drawSelf(context);
    return;
  }
  mRecordingTransform = transform;
  mRecordingClip = clip;
  mRecordingBounds = mBounds;
  mRecordingValid = true;
  if (!appendListCounted(detail::nativeCanvas(canvas), *mRecording->segments[0])) {
    releaseRecording();
    drawSelf(context);
  }
}

graphics::Bitmap* Layer::clipMaskFor(RenderContext& context) {
  const Size target = context.deviceSize();
  if (target.width <= 0.0f || target.height <= 0.0f) {
    return nullptr;
  }
  auto& canvas = context.canvas();
  const graphics::Matrix transform = canvas.transform();
  const bool pathClip = !mClipPath.isEmpty();
  graphics::Path devicePath;
  if (pathClip) {
    devicePath = mClipPath.transformed(transform);
  }
  // The mask covers the *layer's* region, not the clip shape's bounding box: `maskLayer` stretches its
  // bitmap over that region, so a path clip inset inside a larger layer has to keep the layer's full
  // extent (with the shape drawn where it belongs inside it).
  const Point layerCorners[4] = {
    transform * Point{ mBounds.getLeft(), mBounds.getTop() },
    transform * Point{ mBounds.getRight(), mBounds.getTop() },
    transform * Point{ mBounds.getRight(), mBounds.getBottom() },
    transform * Point{ mBounds.getLeft(), mBounds.getBottom() },
  };
  float layerLeft = layerCorners[0].x;
  float layerTop = layerCorners[0].y;
  float layerRight = layerCorners[0].x;
  float layerBottom = layerCorners[0].y;
  for (const Point& corner: layerCorners) {
    layerLeft = std::min(layerLeft, corner.x);
    layerTop = std::min(layerTop, corner.y);
    layerRight = std::max(layerRight, corner.x);
    layerBottom = std::max(layerBottom, corner.y);
  }
  const Rect layerBounds = Rect::MakeLTRB(layerLeft, layerTop, layerRight, layerBottom);
  const float deviceRadius = mClipRadius * std::max(0.001f, canvas.transformScale());
  if (mClipMask != nullptr && mClipMaskDeviceSize == target &&
      mClipMaskDeviceBounds == layerBounds && mClipMaskRadius == deviceRadius &&
      mClipMaskRevision == mClipPathRevision) {
    return mClipMask.get();
  }

  // The mask is the size of the *layer region*, not of the device.
  //
  // It used to be a target-sized bitmap with the shape drawn at device coordinates, which only lines up
  // when the layer covers the whole target. `maskLayer` stretches its bitmap over the layer's region, so
  // a card at (10, 52) got its mask squashed from the whole canvas into that card: the rounded corner
  // appeared near the card's left edge rather than at its corner. Drawing the shape in the mask's own
  // local space makes the bitmap a 1:1 description of the layer region, which is what the composite
  // samples; it also drops the mask's memory and rasterisation from target area to layer area.
  const int width = std::max(1, static_cast<int>(std::ceil(layerBounds.getWidth())));
  const int height = std::max(1, static_cast<int>(std::ceil(layerBounds.getHeight())));
  auto mask = std::make_unique<graphics::Bitmap>(graphics::Bitmap::allocate(width, height));
  if (mask->isEmpty()) {
    return nullptr;
  }
  auto maskCanvas = graphics::Canvas::create(*mask);
  if (maskCanvas == nullptr) {
    return nullptr;
  }
  // A mask is coverage: every pixel the shape does not cover must be transparent. The bitmap is a
  // recycled allocation, so leaving it uninitialised leaks whatever a previous surface drew there
  // into the clip, which showed up as stale glyph and tile fragments inside rounded clips.
  maskCanvas->clear(graphics::Color::MakeRGBA8(0, 0, 0, 0));
  maskCanvas->translate(-layerBounds.getLeft(), -layerBounds.getTop());
  graphics::Paint paint;
  paint.color = graphics::Color::White();
  paint.style = graphics::Paint::Style::kFill;
  if (pathClip) {
    maskCanvas->drawPath(devicePath, paint);
  } else {
    maskCanvas->drawRoundRect(graphics::RoundRect::MakeRectRadius(layerBounds, deviceRadius),
      paint);
  }
  mClipMask = std::move(mask);
  mClipMaskDeviceSize = target;
  mClipMaskDeviceBounds = layerBounds;
  mClipMaskRadius = deviceRadius;
  mClipMaskRevision = mClipPathRevision;
  return mClipMask.get();
}

void Layer::drawSelf(RenderContext& context) {
  (void)context;
}

void Layer::onFrame(const std::chrono::steady_clock::time_point now) {
  const bool wasRunning = mColorAnimation.active || mBoundsAnimation.active || !mAnimations.empty();
  updateAnimations(now);
  const bool stillRunning = mColorAnimation.active || mBoundsAnimation.active || !mAnimations.empty();
  // The pass that cached "animating" ended with the draw; an animation that finished since then must
  // not leave the memo answering true forever.
  if (wasRunning && !stillRunning) invalidateAnimationCache();
  for (const auto& child: mChildren) {
    if (child->visible()) {
      child->onFrame(now);
    }
  }
}

void Layer::onDeviceRecovered() {
  invalidateClipMask();
  releaseRecording();
  setNeedsPaint();
  for (const std::shared_ptr<Layer>& child: mChildren) child->onDeviceRecovered();
}

void Layer::onVisibilityChanged(const bool visible) {
  (void)visible;
}

void Layer::invalidateAnimationCache() {
  // A new animation makes "is anything below animating" stale for this layer and every ancestor that
  // may have cached it; pass id 0 never matches a real pass, so the next query recomputes.
  for (Layer* layer = this; layer != nullptr; layer = layer->mParent) {
    layer->mAnimationFrameId = 0;
  }
}

void Layer::beginAnimationFrame() noexcept {
  ++gAnimationFrame;
}

bool Layer::hasRunningAnimation() const {
  // `cacheWhole` asks this for every layer it considers, so an unmemoised walk repeats the whole
  // subtree once per nesting level. One pass per frame is enough: animations start from event handlers
  // and finish in `onFrame`, both of which run before the draw pass this id covers.
  if (mAnimationFrameId == gAnimationFrame) return mAnimationRunning;
  mAnimationFrameId = gAnimationFrame;
  bool running = mColorAnimation.active || mBoundsAnimation.active || !mAnimations.empty();
  if (!running) {
    for (const std::shared_ptr<Layer>& child: mChildren) {
      if (child->visible() && child->hasRunningAnimation()) {
        running = true;
        break;
      }
    }
  }
  mAnimationRunning = running;
  return running;
}


void Layer::drawChildren(RenderContext& context) {
  std::vector<RenderContext::PlannedItem>* plan = context.planSink();
  auto& canvas = context.canvas();
  // Culling is the draw pass's O(visible) guarantee: a child whose paint rectangle misses the clip is
  // not recorded at all, so a scrolled page costs what its viewport shows rather than what its content
  // holds. A non-uniform/rotated transform makes the rectangle a poor proxy, so those frames draw
  // everything; so does a child that declares it paints outside its bounds.
  const bool canCull = drawCullingEnabled();
  const graphics::Matrix transform = canCull ? canvas.transform() : graphics::Matrix{};
  // Children live in this layer's coordinate space and the clip arrives in device pixels. The transform
  // is scale+translate (checked below), so inverting it is exact and happens once per container rather
  // than once per child: `logical = (device - translation) / scale`.
  float clipLeft = 0.0f;
  float clipTop = 0.0f;
  float clipRight = 0.0f;
  float clipBottom = 0.0f;
  bool haveClip = false;
  if (canCull && transform.isTranslationScaleOnly()) {
    const float scaleX = transform.m[0];
    const float scaleY = transform.m[5];
    if (scaleX != 0.0f && scaleY != 0.0f) {
      const Rect device = canvas.deviceClipBounds();
      clipLeft = (device.getLeft() - transform.m[12]) / scaleX;
      clipTop = (device.getTop() - transform.m[13]) / scaleY;
      clipRight = (device.getRight() - transform.m[12]) / scaleX;
      clipBottom = (device.getBottom() - transform.m[13]) / scaleY;
      haveClip = true;
    }
  }

  const std::vector<Layer*> ordered = paintOrderOf(mChildren);
  for (Layer* child: ordered) {
    if (haveClip && child->clipsToBounds() &&
        !child->intersectsClip(clipLeft, clipTop, clipRight, clipBottom)) {
      child->deferPaint();
      ++gCulledLayers;
      continue;
    }
    if (plan != nullptr && child->isRepaintBoundary() && !child->plansItsOwnContent() &&
        !child->subtreeNeedsPaint() && !child->borrowRecording().empty()) {
      // The frame will draw this boundary from the list it already recorded; descending into it here
      // would record the same content a second time. A *dirty* boundary is not lent: its list is stale,
      // and drawing it is what patches it -- so it draws normally and keeps its list current.
      const auto segments = child->borrowRecording();
      std::size_t lent = 0;
      for (const std::shared_ptr<const graphics::gpu::DrawList>& segment: segments) {
        if (segment != nullptr) lent += estimatedSegmentBytes(*segment);
      }
      gLentListBytes += lent;
      RenderContext::PlannedItem item;
      item.layer = child;
      plan->push_back(item);
      continue;
    }
    child->draw(context);
  }
}

Layer* Layer::hitTest(const Point& point) {
  if (!mVisible || mOpacity <= 0.0f) {
    return nullptr;
  }
  const Point local = hasTransform(mTranslation, mScaleX, mScaleY, mRotation)
    ? inverseTransform(point, mBounds, mTranslation, mScaleX, mScaleY, mRotation, mTransformOrigin)
    : point;
  if (!mBounds.contains(local)) {
    return nullptr;
  }
  if (mClip && mClipRadius > 0.0f && !roundedRectPath(mBounds, mClipRadius).contains(local)) {
    return nullptr;
  }
  if (mClip && !mClipPath.isEmpty() && !mClipPath.contains(local)) {
    return nullptr;
  }
  // The same order recording paints in, reversed: whatever is painted last is hit first. A second hand
  // -written sort here is how a container ends up painted in one order and hit-tested in another.
  const std::vector<Layer*> ordered = paintOrderedChildren();
  for (auto child = ordered.rbegin(); child != ordered.rend(); ++child) {
    if (Layer* const hit = (*child)->hitTest(local); hit != nullptr) {
      return hit;
    }
  }
  return mHitTestable ? this : nullptr;
}

bool Layer::onPointerDown(const PointerEvent& event) {
  (void)event;
  return false;
}

bool Layer::onPointerUp(const PointerEvent& event) {
  (void)event;
  return false;
}

bool Layer::onPointerMove(const PointerEvent& event) {
  (void)event;
  return false;
}

bool Layer::onPointerLeave() {
  return false;
}

bool Layer::onKeyDown(const KeyEvent& event) {
  (void)event;
  return false;
}

bool Layer::onKeyUp(const KeyEvent& event) {
  (void)event;
  return false;
}

bool Layer::onScroll(const ScrollEvent& event) {
  (void)event;
  return false;
}

bool Layer::onGesture(const GestureEvent& event) {
  (void)event;
  return false;
}

bool Layer::onDrop(const DropEvent& event) {
  (void)event;
  return false;
}

DragEffect Layer::onDragEnter(const DragEvent& event) {
  (void)event;
  return DragEffect::None;
}

DragEffect Layer::onDragOver(const DragEvent& event) {
  (void)event;
  return DragEffect::None;
}

void Layer::onDragLeave() {}

void Layer::setDragOutProvider(DragOutProvider provider) {
  mDragOutProvider = std::move(provider);
}

bool Layer::onTextInput(const char32_t character) {
  (void)character;
  return false;
}

bool Layer::onTextComposition(const std::string& text) {
  (void)text;
  return false;
}

bool Layer::onTextCommit(const std::string& text) {
  (void)text;
  return false;
}

bool Layer::onTextDelete(const int32_t index, const uint32_t length) {
  (void)index;
  (void)length;
  return false;
}

bool Layer::acceptsTextInput() const {
  return false;
}

void Layer::onFocusChanged(const bool focused) {
  (void)focused;
}

float Layer::baselineOffset() const {
  return -1.0f;
}

Size Layer::intrinsicSize() const {
  return Size{ -1.0f, -1.0f };
}

Size Layer::measureWithConstraints(const Constraints& constraints) {
  // Adapt to the legacy spelling: there `0` means unbounded, which is what an infinite maximum becomes.
  const Size measured = measure(constraints.availableSize());
  return Size{ std::max(measured.width, constraints.minWidth),
    std::max(measured.height, constraints.minHeight) };
}

Size Layer::measureCached(const Size& available) {
  if (mMeasureCacheValid && !mMeasureCacheFromConstraints &&
      mMeasureCacheAvailable.width == available.width &&
      mMeasureCacheAvailable.height == available.height) {
    return mMeasureCacheResult;
  }
  const Size result = measure(available);
  mMeasureCacheAvailable = available;
  mMeasureCacheFromConstraints = false;
  mMeasureCacheResult = result;
  mMeasureCacheValid = true;
  return result;
}

Size Layer::measureCachedWithConstraints(const Constraints& constraints) {
  if (mMeasureCacheValid && mMeasureCacheFromConstraints &&
      mMeasureCacheConstraints.minWidth == constraints.minWidth &&
      mMeasureCacheConstraints.minHeight == constraints.minHeight &&
      mMeasureCacheConstraints.maxWidth == constraints.maxWidth &&
      mMeasureCacheConstraints.maxHeight == constraints.maxHeight) {
    return mMeasureCacheResult;
  }
  const Size result = measureWithConstraints(constraints);
  mMeasureCacheConstraints = constraints;
  mMeasureCacheFromConstraints = true;
  mMeasureCacheResult = result;
  mMeasureCacheValid = true;
  return result;
}

void Layer::invalidateMeasureCache() {
  mMeasureCacheValid = false;
}

void Layer::setNeedsLayout() {
  mMeasureCacheValid = false;
  mNeedsLayout = true;
  setNeedsPaint();
  // Climb until a layout boundary. Every ancestor learns that *a* pass is needed (so the traversal can
  // reach the boundary), but only the ones at or below it are marked as needing their own arrangement:
  // inside a boundary nothing can change the parent's size or the position of anything outside it.
  const Layer* current = this;
  bool stopped = false;
  for (Layer* ancestor = mParent; ancestor != nullptr; ancestor = ancestor->mParent) {
    // Every ancestor up to the root learns that a pass is needed, otherwise the traversal that looks
    // for dirty children is never started and the boundary would stay dirty forever.
    ancestor->mSubtreeNeedsLayout = true;
    if (stopped) continue;
    if (current->isLayoutBoundary()) {
      stopped = true;
      continue;
    }
    ancestor->mNeedsLayout = true;
    current = ancestor;
  }
}

void Layer::addDamage(const Rect& localBounds) {
  if (localBounds.getWidth() <= 0.0f || localBounds.getHeight() <= 0.0f) return;
  const Rect parentBounds = Rect::MakeLTRB(localBounds.getLeft() + mTranslation.x,
    localBounds.getTop() + mTranslation.y, localBounds.getRight() + mTranslation.x,
    localBounds.getBottom() + mTranslation.y);
  if (!mHasDamage) {
    mDamage = parentBounds;
    mHasDamage = true;
    return;
  }
  mDamage = Rect::MakeLTRB(std::min(mDamage.getLeft(), parentBounds.getLeft()),
    std::min(mDamage.getTop(), parentBounds.getTop()),
    std::max(mDamage.getRight(), parentBounds.getRight()),
    std::max(mDamage.getBottom(), parentBounds.getBottom()));
}

void Layer::collectDirtyCounts(uint32_t& paintDirty, uint32_t& layoutDirty) const {
  // An invisible or fully transparent subtree is skipped by `draw`, so its flags never clear. Counting
  // it would make a permanent "1 dirty node" that no frame can ever pay off.
  if (!mVisible || mOpacity <= 0.0f) return;
  if (mNeedsPaint) ++paintDirty;
  if (mNeedsLayout) ++layoutDirty;
  for (const std::shared_ptr<Layer>& child: mChildren) {
    child->collectDirtyCounts(paintDirty, layoutDirty);
  }
}

namespace {

bool rectsIntersect(const Rect& left, const Rect& right) {
  if (left.isEmpty() || right.isEmpty()) return false;
  return left.getRight() >= right.getLeft() && left.getLeft() <= right.getRight() &&
    left.getBottom() >= right.getTop() && left.getTop() <= right.getBottom();
}

Rect intersectRects(const Rect& left, const Rect& right) {
  if (left.isEmpty()) return right;
  if (right.isEmpty()) return left;
  const float l = std::max(left.getLeft(), right.getLeft());
  const float t = std::max(left.getTop(), right.getTop());
  const float r = std::min(left.getRight(), right.getRight());
  const float b = std::min(left.getBottom(), right.getBottom());
  if (r <= l || b <= t) return Rect{};
  return Rect::MakeLTRB(l, t, r, b);
}

Rect offsetRect(const Rect& rect, const Point offset) {
  return Rect::MakeLTRB(rect.getLeft() + offset.x, rect.getTop() + offset.y,
    rect.getRight() + offset.x, rect.getBottom() + offset.y);
}

} // namespace anonymous

bool Layer::collectDamage(Rect& out, const Point parentOffset, const Rect clip) const {
  if (mDamageUnbounded) return false;
  // A scale or rotation can move pixels outside the new bounds, and the old bounds are no longer
  // known here; the window falls back to a full repaint for that subtree.
  if (mScaleX != 1.0f || mScaleY != 1.0f || mRotation != 0.0f) return false;
  const Rect globalBounds = offsetRect(mBounds, parentOffset);
  // An ancestor clip this subtree cannot reach means nothing below can join the damage. Without this
  // the walk is proportional to the tree, not to what changed: a scrolled page visited every row it
  // owns even though its viewport only shows a handful of them.
  if (!clip.isEmpty() && !rectsIntersect(globalBounds, clip)) return true;
  // A clipping container tightens the clip for everything below it, once.
  const Rect childClip = clipsChildren()
    ? (clip.isEmpty() ? globalBounds : intersectRects(clip, globalBounds))
    : clip;
  if (mHasDamage) {
    const Rect damaged = offsetRect(mDamage, parentOffset);
    if (out.getWidth() <= 0.0f || out.getHeight() <= 0.0f) {
      out = damaged;
    } else {
      out = Rect::MakeLTRB(std::min(out.getLeft(), damaged.getLeft()),
        std::min(out.getTop(), damaged.getTop()),
        std::max(out.getRight(), damaged.getRight()),
        std::max(out.getBottom(), damaged.getBottom()));
    }
  }
  const Point childOffset{ parentOffset.x + mTranslation.x, parentOffset.y + mTranslation.y };
  for (const std::shared_ptr<Layer>& child: mChildren) {
    // Nothing below a clean child changed, so its rectangle cannot contribute new damage.
    if (!child->subtreeNeedsPaint() && !child->needsPaint()) continue;
    // The draw pass will skip this child for the same reason; its damage cannot be visible either.
    if (!childClip.isEmpty() && child->clipsToBounds() &&
        !rectsIntersect(offsetRect(child->paintBounds(), childOffset), childClip)) {
      continue;
    }
    if (child->isRepaintBoundary() && child->subtreeNeedsPaint()) {
      // A boundary repaints as a unit: its whole rectangle is the damage and nothing inside it has to
      // be visited. That is the trade a boundary makes -- a coarser rectangle for a coherent region.
      Rect boundary = offsetRect(child->bounds(), childOffset);
      if (!childClip.isEmpty()) boundary = intersectRects(boundary, childClip);
      if (!boundary.isEmpty()) {
        if (out.getWidth() <= 0.0f || out.getHeight() <= 0.0f) {
          out = boundary;
        } else {
          out = Rect::MakeLTRB(std::min(out.getLeft(), boundary.getLeft()),
            std::min(out.getTop(), boundary.getTop()),
            std::max(out.getRight(), boundary.getRight()),
            std::max(out.getBottom(), boundary.getBottom()));
        }
      }
      continue;
    }
    Rect childDamage;
    if (!child->collectDamage(childDamage, childOffset, childClip)) return false;
    if (childDamage.isEmpty()) continue;
    if (!childClip.isEmpty()) childDamage = intersectRects(childDamage, childClip);
    if (childDamage.isEmpty()) continue;
    if (out.getWidth() <= 0.0f || out.getHeight() <= 0.0f) {
      out = childDamage;
    } else {
      out = Rect::MakeLTRB(std::min(out.getLeft(), childDamage.getLeft()),
        std::min(out.getTop(), childDamage.getTop()),
        std::max(out.getRight(), childDamage.getRight()),
        std::max(out.getBottom(), childDamage.getBottom()));
    }
  }
  return true;
}

std::vector<Layer*> Layer::paintOrderedChildren() const {
  return paintOrderOf(mChildren);
}

std::shared_ptr<const graphics::gpu::DrawList> Layer::recordInto(const Size size, const float scale,
    const Point origin, const Rect clip) {
  if (size.width <= 0.0f || size.height <= 0.0f || scale <= 0.0f) return nullptr;
  auto list = std::make_shared<graphics::gpu::DrawList>();
  std::unique_ptr<graphics::Canvas> recorded = graphics::Canvas::createRecordingBackend(
    static_cast<int>(size.width), static_cast<int>(size.height), nullptr, list);
  if (recorded == nullptr) return nullptr;
  Canvas canvas = detail::makeCanvas(*recorded);
  RenderContext context(canvas, scale, nullptr, size, nullptr);
  context.canvas().scale(scale);
  // The subtree's own origin becomes the list's origin, so a later translation is the only thing that
  // decides where it lands.
  if (origin.x != 0.0f || origin.y != 0.0f) {
    context.canvas().translate(-origin.x, -origin.y);
  } else {
    context.canvas().translate(-mBounds.getLeft(), -mBounds.getTop());
  }
  // The clip is in the same logical space as the origin. Applying it before the draw is what makes a
  // band list contain only the rows that band shows: the culling inside `draw` reads this canvas clip.
  if (!clip.isEmpty()) {
    context.canvas().clipRect(clip);
  }
  draw(context);
  return list;
}

void Layer::adoptRecording(std::shared_ptr<const graphics::gpu::DrawList> list) {
  if (list == nullptr) {
    mRecordingValid = false;
    return;
  }
  auto recording = std::make_shared<detail::LayerRecording>();
  recording->segments.push_back(std::move(list));
  recording->childCount = 0;
  recording->dirtySegments.assign(1, 0);
  recording->bytes = estimatedSegmentBytes(*recording->segments.front());
  mRecording = std::move(recording);
  mRecordingValid = true;
  mRecordingClip = Rect{};
  mRecordingTransform = Matrix{};
}

std::vector<std::shared_ptr<const graphics::gpu::DrawList>> Layer::borrowRecording() const {
  if (!mRecordingValid || mRecording == nullptr) return {};
  return mRecording->segments;
}

void Layer::clearDamage() {
  mDamage = Rect {};
  mHasDamage = false;
  mDamageUnbounded = false;
  for (const std::shared_ptr<Layer>& child: mChildren) child->clearDamage();
}

void Layer::setNeedsPaint() {
  mNeedsPaint = true;
  mSubtreeNeedsPaint = true;
  releaseRecording();
  addDamage(mBounds);
  // Walk the ancestor chain keeping, at each level, which child the change came through: a segmentable
  // ancestor patches that child segment instead of dropping its whole recording.
  const Layer* child = this;
  for (Layer* layer = mParent; layer != nullptr; layer = layer->mParent) {
    layer->mSubtreeNeedsPaint = true;
    if (!layer->markSegmentDirty(child)) {
      layer->releaseRecording();
    }
    child = layer;
  }
}


void Layer::setStyleSheet(std::shared_ptr<const StyleSheet> sheet) {
  mStyleSheetExplicit = true;
  inheritStyleSheet(std::move(sheet));
}

void Layer::inheritStyleSheet(std::shared_ptr<const StyleSheet> sheet) {
  if (mStyleSheet == sheet) return;
  mStyleSheet = std::move(sheet);
  // A sheet cascades like the theme: every control under the window has to be able to resolve its
  // style name, not only the root. A child with its own sheet keeps it.
  for (const std::shared_ptr<Layer>& child: mChildren) {
    if (!child->mStyleSheetExplicit) child->inheritStyleSheet(mStyleSheet);
  }
  if (!mStyleName.empty()) setNeedsPaint();
}

void Layer::setStyleName(std::string name) {
  if (mStyleName == name) return;
  mStyleName = std::move(name);
  setNeedsPaint();
}

void Layer::setEnabled(const bool enabled) {
  if (mEnabled == enabled) return;
  mEnabled = enabled;
  onEnabledChanged(enabled);
  setNeedsPaint();
  notifyAccessibilityEvent(*this, AccessibilityEvent::EnabledChanged);
}

bool Layer::inputEnabled() const noexcept {
  for (const Layer* layer = this; layer != nullptr; layer = layer->mParent) {
    if (!layer->mEnabled) return false;
  }
  return true;
}

StateMask Layer::styleStates() const {
  return mEnabled ? 0 : stateFlag(ControlState::Disabled);
}

std::optional<ResolvedStyle> Layer::resolvedStyle() const {
  if (mStyleSheet == nullptr) return std::nullopt;
  std::string_view name = mStyleName;
  if (name.empty()) {
    const char* typeName = defaultStyleName();
    if (typeName == nullptr || *typeName == '\0') return std::nullopt;
    name = typeName;
  }
  if (mStyleSheet->find(name) == nullptr) return std::nullopt;
  static const Theme fallback = Theme::dark();
  const Theme& theme = mTheme != nullptr ? *mTheme : fallback;
  return mStyleSheet->resolve(name, styleStates(), theme);
}


void Layer::setDirection(const Direction direction) {
  if (mDirectionSet && mDirection == direction) return;
  mDirection = direction;
  mDirectionSet = true;
  setNeedsLayout();
  setNeedsPaint();
}

Direction Layer::direction() const noexcept {
  if (mDirectionSet) return mDirection;
  return mParent != nullptr ? mParent->direction() : Direction::Ltr;
}

} // namespace coral::ui
