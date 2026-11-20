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

#include <ui/Motion.h>
#include <ui/RecyclingListView.h>

#include <ui/PlatformClock.h>
#include <ui/Theme.h>
#include <ui/Window.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <utility>

namespace coral::ui {

class RecyclingListView::Content final : public Layer {
 public:
  explicit Content(RecyclingListView& owner) : mOwner(owner) {
  }

  Size measure(const Size& available) override {
    const float width = available.width > 0.0f ? available.width : 200.0f;
    setMeasuredSize({ width, mOwner.contentHeight() });
    return measuredSize();
  }

  void layout() override {
    mOwner.syncRows();
  }

 private:
  RecyclingListView& mOwner;
};

RecyclingListView::RecyclingListView(RowBuilder builder, const float rowHeight)
  : mBuilder(std::move(builder)), mRowHeight(std::max(1.0f, rowHeight)) {
  mContent = std::make_shared<Content>(*this);
  setContent(mContent);
  setBackgroundColor(Color::MakeRGBA8(0, 0, 0, 0));
  setAccessibleRole(AccessibilityRole::List);
}

RecyclingListView::~RecyclingListView() = default;

void RecyclingListView::setRows(std::vector<std::string> keys) {
  if (mRows == keys) return;
  mRows = std::move(keys);
  rebuildRowMetrics();
  removeMissingRows();
  setNeedsLayout();
  setNeedsPaint();
  if (mContent != nullptr) {
    mContent->setNeedsLayout();
    mContent->setNeedsPaint();
  }
  syncRows();
}

void RecyclingListView::setRowBuilder(RowBuilder builder) {
  mBuilder = std::move(builder);
  mIntrinsicHeights.clear();
  for (auto& entry: mMounted) entry.second->measureDirty = true;
  for (auto& entry: mPool) entry.second->measureDirty = true;
  setNeedsLayout();
  syncRows();
}

void RecyclingListView::setSelectedKey(std::string key) {
  if (mSelectedKey == key) return;
  mSelectedKey = std::move(key);
  syncRows();
  setNeedsPaint();
  if (mOnSelectedChanged) mOnSelectedChanged(mSelectedKey);
}

void RecyclingListView::setOnSelectedChanged(std::function<void(const std::string&)> callback) {
  mOnSelectedChanged = std::move(callback);
}

void RecyclingListView::setFocusedKey(std::string key) {
  if (mFocusedKey == key) return;
  mFocusedKey = std::move(key);
  mFocusedDescendant = nullptr;
  mFocusDetached = false;
  syncRows();
  if (mFocusedKey.empty()) return;
  if (const auto it = mMounted.find(mFocusedKey); it != mMounted.end()) {
    Layer* const scope = mFocusOwner != nullptr ? mFocusOwner->activeFocusScope() : nullptr;
    if (mFocusOwner != nullptr && (scope == nullptr || isWithin(it->second->layer.get(), scope))) {
      mFocusOwner->focusLayer(it->second->layer.get());
    }
    if (mFocusRequester != nullptr) mFocusRequester(it->second->layer.get());
  }
}

void RecyclingListView::setFocusOwner(Window* window) {
  mFocusOwner = window;
  if (mFocusOwner != nullptr && mFocusDetached && !mFocusedKey.empty()) {
    if (const auto it = mMounted.find(mFocusedKey); it != mMounted.end()) {
      restoreFocus(mFocusedKey, it->second->layer);
    }
  }
}

bool RecyclingListView::isWithin(const Layer* layer, const Layer* ancestor) {
  for (const Layer* current = layer; current != nullptr; current = current->parent()) {
    if (current == ancestor) return true;
  }
  return false;
}

void RecyclingListView::detachFocus(const std::string& key, const std::shared_ptr<Layer>& row) {
  if (mFocusOwner == nullptr || row == nullptr) return;
  Layer* const focused = mFocusOwner->focusedLayer();
  if (focused == nullptr || !isWithin(focused, row.get())) return;
  mFocusedKey = key;
  mFocusedDescendant = focused;
  mFocusDetached = true;
  // The row is about to leave the tree; keeping a pointer to it in Window would be a dangling focus.
  mFocusOwner->focusLayer(nullptr);
}

void RecyclingListView::restoreFocus(const std::string& key, const std::shared_ptr<Layer>& row) {
  if (mFocusOwner == nullptr || row == nullptr || !mFocusDetached || key != mFocusedKey) return;
  // Never steal focus from a control the user moved to while the row was detached.
  if (mFocusOwner->focusedLayer() != nullptr) {
    mFocusDetached = false;
    mFocusedDescendant = nullptr;
    return;
  }
  Layer* target = row.get();
  if (mFocusedDescendant != nullptr && isWithin(mFocusedDescendant, row.get())) {
    target = mFocusedDescendant;
  } else {
    mFocusedDescendant = nullptr;
  }
  const Layer* const scope = mFocusOwner->activeFocusScope();
  if (scope != nullptr && !isWithin(target, scope)) {
    mFocusDetached = false;
    return;
  }
  mFocusOwner->focusLayer(target);
  mFocusDetached = false;
}

void RecyclingListView::setHoveredKey(std::string key) {
  if (mHoveredKey == key) return;
  mHoveredKey = std::move(key);
  syncRows();
  setNeedsPaint();
}

void RecyclingListView::setFocusRequester(std::function<void(Layer*)> requester) {
  mFocusRequester = std::move(requester);
}

void RecyclingListView::setPoolLimit(const size_t limit) {
  mPoolPolicy.limit = limit;
  trimPool();
}

void RecyclingListView::setPoolPolicy(PoolPolicy policy) {
  policy.distanceWeight = std::clamp(policy.distanceWeight, 0.0, 1.0);
  policy.minRetainAbove = std::min(policy.minRetainAbove, policy.limit);
  policy.minRetainBelow = std::min(policy.minRetainBelow, policy.limit - policy.minRetainAbove);
  policy.minRetainAbovePixels = std::max(0.0f, policy.minRetainAbovePixels);
  policy.minRetainBelowPixels = std::max(0.0f, policy.minRetainBelowPixels);
  mPoolPolicy = policy;
  trimPool();
}

RecyclingListView::RowState RecyclingListView::stateFor(const std::string& key) const {
  return RowState{ key == mSelectedKey, key == mFocusedKey, key == mHoveredKey };
}

void RecyclingListView::touchPoolEntry(const std::string& key, const size_t index) {
  mPoolMeta[key] = PoolMeta{ ++mAccessTick, index };
}

void RecyclingListView::trimPool() {
  if (mPoolPolicy.limit == 0) {
    mPool.clear();
    mPoolMeta.clear();
    mFocusedDescendant = nullptr;
    return;
  }
  const size_t retainAbove = std::min(mPoolPolicy.limit,
    std::max(mPoolPolicy.minRetainAbove,
      countForPixelBudget(true, mPoolPolicy.minRetainAbovePixels)));
  const size_t retainBelow = std::min(mPoolPolicy.limit - retainAbove,
    std::max(mPoolPolicy.minRetainBelow,
      countForPixelBudget(false, mPoolPolicy.minRetainBelowPixels)));

  struct Candidate {
    std::unordered_map<std::string, std::unique_ptr<Slot>>::iterator it;
    float distance = 0.0f;
    uint64_t age = 0;
    int side = 0; // -1 above, +1 below, 0 inside
  };

  while (mPool.size() > mPoolPolicy.limit) {
    std::vector<Candidate> candidates;
    candidates.reserve(mPool.size());
    float maxDistance = 0.0f;
    uint64_t maxAge = 0;
    for (auto it = mPool.begin(); it != mPool.end(); ++it) {
      const auto meta = mPoolMeta.find(it->first);
      const size_t index = meta != mPoolMeta.end() ? meta->second.lastIndex : 0;
      const uint64_t access = meta != mPoolMeta.end() ? meta->second.lastAccess : 0;
      float distance = 0.0f;
      int side = 0;
      if (mContent != nullptr && index < mRowOffsets.size()) {
        const float rowTop = mRowOffsets[index];
        const float rowBottom = rowTop + rowHeightAt(index);
        const float viewportTop = std::max(0.0f, bounds().getTop() - mContent->bounds().getTop());
        const float viewportBottom = std::max(0.0f, bounds().getBottom() - mContent->bounds().getTop());
        if (index < mVisibleFirst) {
          distance = std::max(0.0f, viewportTop - rowBottom);
          side = -1;
        } else if (index > mVisibleLast) {
          distance = std::max(0.0f, rowTop - viewportBottom);
          side = 1;
        }
      }
      const uint64_t age = mAccessTick >= access ? mAccessTick - access : 0;
      maxDistance = std::max(maxDistance, distance);
      maxAge = std::max(maxAge, age);
      candidates.push_back(Candidate{ it, distance, age, side });
    }
    if (candidates.empty()) break;

    std::vector<size_t> above;
    std::vector<size_t> below;
    for (size_t i = 0; i < candidates.size(); ++i) {
      if (candidates[i].side < 0) above.push_back(i);
      if (candidates[i].side > 0) below.push_back(i);
    }
    const auto byDistance = [&](const size_t left, const size_t right) {
      return candidates[left].distance < candidates[right].distance;
    };
    std::sort(above.begin(), above.end(), byDistance);
    std::sort(below.begin(), below.end(), byDistance);

    std::vector<bool> protectedRow(candidates.size(), false);
    for (size_t i = 0; i < retainAbove && i < above.size(); ++i) {
      protectedRow[above[i]] = true;
    }
    for (size_t i = 0; i < retainBelow && i < below.size(); ++i) {
      protectedRow[below[i]] = true;
    }

    size_t victim = candidates.size();
    double victimScore = -1.0;
    uint64_t victimAccess = std::numeric_limits<uint64_t>::max();
    for (size_t i = 0; i < candidates.size(); ++i) {
      if (protectedRow[i]) continue;
      const double distanceScore = maxDistance > 0.0f
        ? static_cast<double>(candidates[i].distance) / static_cast<double>(maxDistance) : 0.0;
      const double ageScore = maxAge > 0
        ? static_cast<double>(candidates[i].age) / static_cast<double>(maxAge) : 0.0;
      const double score = mPoolPolicy.distanceWeight * distanceScore +
        (1.0 - mPoolPolicy.distanceWeight) * ageScore;
      const uint64_t access = mPoolMeta[candidates[i].it->first].lastAccess;
      if (victim == candidates.size() || score > victimScore ||
          (score == victimScore && access < victimAccess)) {
        victim = i;
        victimScore = score;
        victimAccess = access;
      }
    }
    if (victim == candidates.size()) break;

    const std::string key = candidates[victim].it->first;
    if (key == mFocusedKey) mFocusedDescendant = nullptr;
    mPoolMeta.erase(key);
    mPool.erase(candidates[victim].it);
    ++mPoolEvictions;
  }
}

void RecyclingListView::setRowHeightProvider(RowHeightProvider provider) {
  mHeightProvider = std::move(provider);
  mIntrinsicHeights.clear();
  rebuildRowMetrics();
  setNeedsLayout();
  if (mContent != nullptr) mContent->setNeedsLayout();
  syncRows();
}

void RecyclingListView::setAutoRowHeight(const bool enabled) {
  if (mAutoRowHeight == enabled) return;
  mAutoRowHeight = enabled;
  if (!mAutoRowHeight) mIntrinsicHeights.clear();
  for (auto& entry: mMounted) entry.second->measureDirty = true;
  for (auto& entry: mPool) entry.second->measureDirty = true;
  mMeasuredWidth = 0.0f;
  rebuildRowMetrics();
  setNeedsLayout();
  if (mContent != nullptr) mContent->setNeedsLayout();
  syncRows();
}
void RecyclingListView::setMeasurePrefetchRows(const size_t rows) {
  mMeasurePrefetchRows = rows;
  syncRows();
}

void RecyclingListView::setClockRateCalibrationPpm(const double ppm) {
  mUsePlatformClock = false;
  mCalibratedPpm = ppm;
  mClockRate = 1.0 + ppm / 1000000.0;
  mHasTimestampBase = false;
  mHasClockSample = false;
}

void RecyclingListView::setClockRateProvider(ClockRateProvider provider) {
  mUsePlatformClock = false;
  mClockRateProvider = std::move(provider);
  if (mClockRateProvider) {
    mCalibratedPpm = mClockRateProvider();
    mClockRate = std::clamp(1.0 + mCalibratedPpm / 1000000.0, 0.95, 1.05);
  }
}

void RecyclingListView::setMaxMeasurePrefetchExtra(const size_t rows) {
  mMaxPrefetchExtra = rows;
  if (mPrefetchBoost > mMaxPrefetchExtra) mPrefetchBoost = mMaxPrefetchExtra;
  syncRows();
}

void RecyclingListView::invalidateRowHeights() {
  mIntrinsicHeights.clear();
  mMeasuredWidth = 0.0f;
  for (auto& entry: mMounted) entry.second->measureDirty = true;
  for (auto& entry: mPool) entry.second->measureDirty = true;
  rebuildRowMetrics();
  setNeedsLayout();
  if (mContent != nullptr) mContent->setNeedsLayout();
  syncRows();
}

float RecyclingListView::rowHeightAt(const size_t index) const {
  return index < mRowHeights.size() ? mRowHeights[index] : mRowHeight;
}

void RecyclingListView::rebuildRowMetrics() {
  // Scroll anchoring: remember which row sits at the viewport top before the metrics change, so a
  // row-height change above the viewport does not shift what the user is reading.
  std::string anchorKey;
  size_t anchorIndex = 0;
  float anchorTop = 0.0f;
  float anchorScroll = 0.0f;
  if (mScrollAnchoring && !mRows.empty() && !mRowOffsets.empty() && scrollY() > 0.0f) {
    const auto offset = std::upper_bound(mRowOffsets.begin(), mRowOffsets.end(), scrollY());
    const size_t index = offset == mRowOffsets.begin()
      ? 0
      : static_cast<size_t>(offset - mRowOffsets.begin()) - 1;
    if (index < mRows.size()) {
      anchorKey = mRows[index];
      anchorIndex = index;
      anchorTop = mRowOffsets[index];
      anchorScroll = scrollY();
    }
  }
  mRowHeights.clear();
  mRowOffsets.clear();
  mRowHeights.reserve(mRows.size());
  mRowOffsets.reserve(mRows.size() + 1);
  mRowOffsets.push_back(0.0f);
  for (size_t i = 0; i < mRows.size(); ++i) {
    float height = mRowHeight;
    if (mHeightProvider) {
      height = mHeightProvider(mRows[i], i);
    } else if (mAutoRowHeight) {
      if (const auto measured = mIntrinsicHeights.find(mRows[i]); measured != mIntrinsicHeights.end()) {
        height = measured->second;
      }
    }
    if (!std::isfinite(height) || height <= 0.0f) height = mRowHeight;
    mRowHeights.push_back(height);
    mRowOffsets.push_back(mRowOffsets.back() + height);
  }
  if (mRowOffsets.empty()) mRowOffsets.push_back(0.0f);
  if (!anchorKey.empty()) {
    const auto position = std::find(mRows.begin(), mRows.end(), anchorKey);
    // When the anchored row itself was removed, fall back to the row now occupying its index so the
    // viewport still does not jump.
    const size_t index = position != mRows.end()
      ? static_cast<size_t>(position - mRows.begin())
      : anchorIndex;
    if (index < mRowOffsets.size() && index < mRows.size()) {
      const float delta = mRowOffsets[index] - anchorTop;
      if (std::abs(delta) > 0.5f) setScrollOffset(scrollX(), anchorScroll + delta);
    }
  }
}

size_t RecyclingListView::countForPixelBudget(const bool above, const float budget) const {
  if (budget <= 0.0f || mRows.empty()) return 0;
  float accumulated = 0.0f;
  size_t count = 0;
  if (above) {
    for (size_t i = mVisibleFirst; i > 0; --i) {
      accumulated += rowHeightAt(i - 1);
      ++count;
      if (accumulated >= budget) break;
    }
  } else {
    for (size_t i = mVisibleLast + 1; i < mRows.size(); ++i) {
      accumulated += rowHeightAt(i);
      ++count;
      if (accumulated >= budget) break;
    }
  }
  return count;
}

uint64_t RecyclingListView::fontRevisionOf(const Layer& layer) {
  uint64_t hash = layer.fontRevision() + 0x9e3779b97f4a7c15ull;
  for (const auto& child: layer.children()) {
    if (child != nullptr) hash ^= fontRevisionOf(*child) + 0x9e3779b97f4a7c15ull +
      (hash << 6) + (hash >> 2);
  }
  return hash;
}

bool RecyclingListView::measureVisibleRows(const float contentWidth) {
  if (!mAutoRowHeight || mHeightProvider || mMounted.empty()) return false;
  const float width = contentWidth > 0.0f ? contentWidth : std::max(1.0f, bounds().getWidth());
  bool changed = false;
  for (auto& entry: mMounted) {
    Slot& slot = *entry.second;
    if (slot.layer == nullptr || !slot.measureDirty) continue;
    const Size measured = slot.layer->measure({ width, 0.0f });
    ++mMeasureCount;
    const float height = std::max(1.0f, measured.height);
    slot.measureDirty = false;
    const auto existing = mIntrinsicHeights.find(entry.first);
    if (existing == mIntrinsicHeights.end() || std::abs(existing->second - height) > 0.5f) {
      mIntrinsicHeights[entry.first] = height;
      changed = true;
    }
  }
  return changed;
}

void RecyclingListView::setRowHeight(const float height) {
  const float next = std::max(1.0f, height);
  if (next == mRowHeight) return;
  mRowHeight = next;
  rebuildRowMetrics();
  setNeedsLayout();
  if (mContent != nullptr) mContent->setNeedsLayout();
  syncRows();
}

std::shared_ptr<Layer> RecyclingListView::rowLayer(const std::string& key) const {
  const Slot* slot = findSlot(key);
  return slot != nullptr ? slot->layer : nullptr;
}

void RecyclingListView::layout() {
  ScrollView::layout();
  if (mAutoRowHeight && !mHeightProvider && mContent != nullptr) {
    const float width = mContent->bounds().getWidth();
    if (width > 0.0f && std::abs(width - mMeasuredWidth) > 0.5f) {
      mMeasuredWidth = width;
      mIntrinsicHeights.clear();
      for (auto& entry: mMounted) entry.second->measureDirty = true;
      for (auto& entry: mPool) entry.second->measureDirty = true;
      rebuildRowMetrics();
    }
  }
  refreshPrefetchBoost(std::chrono::steady_clock::now());
  syncRows();
}

void RecyclingListView::refreshPrefetchBoost(const std::chrono::steady_clock::time_point now) {
  constexpr auto kWindow = std::chrono::milliseconds(120);
  while (!mScrollSamples.empty() && now - mScrollSamples.front().time > kWindow) {
    mScrollSamples.pop_front();
  }
  if (mScrollSamples.empty()) {
    mPrefetchBoost = 0;
    return;
  }
  float distance = 0.0f;
  for (const ScrollSample& sample: mScrollSamples) distance += sample.distance;
  auto elapsed = now - mScrollSamples.front().time;
  if (elapsed < std::chrono::milliseconds(1)) elapsed = std::chrono::milliseconds(1);
  const double pixelsPerSecond = static_cast<double>(distance) /
    std::chrono::duration<double>(elapsed).count();
  const double rowsPerSecond = pixelsPerSecond / std::max(1.0f, rowHeight());
  // 100ms of look-ahead at the measured velocity, capped by the caller's budget.
  mPrefetchBoost = std::min(mMaxPrefetchExtra,
    static_cast<size_t>(std::ceil(rowsPerSecond * 0.1)));
}

bool RecyclingListView::onScroll(const ScrollEvent& event) {
  const bool handled = ScrollView::onScroll(event);
  if (handled) {
    // The pointer is now over a different row; hovering a detached key would be misleading.
    mHoveredKey.clear();
    const auto steadyNow = std::chrono::steady_clock::now();
    auto now = steadyNow;
    const auto platform = event.timestamp;
    double calibratedPpm = 0.0;
    if (mClockRateProvider) {
      calibratedPpm = mClockRateProvider();
    } else if (mUsePlatformClock) {
      calibratedPpm = platform::clockRatePpm();
    }
    if (calibratedPpm != 0.0) {
      const double ppm = calibratedPpm;
      if (std::abs(ppm - mCalibratedPpm) > 0.5) {
        mCalibratedPpm = ppm;
        const double nominal = 1.0 + ppm / 1000000.0;
        mClockRate = std::clamp(mClockRate, nominal * 0.95, nominal * 1.05);
      }
    }
    if (platform != std::chrono::steady_clock::time_point{}) {
      constexpr auto kResetGap = std::chrono::milliseconds(500);
      const auto platformDelta = platform - mTimestampBasePlatform;
      if (mHasClockSample) {
        const auto platformStep = platform - mLastPlatformSample;
        const auto steadyStep = steadyNow - mLastSteadySample;
        if (platformStep > std::chrono::steady_clock::duration::zero() &&
            platformStep < kResetGap && steadyStep >= std::chrono::steady_clock::duration::zero()) {
          const double platformSeconds = std::chrono::duration<double>(platformStep).count();
          const double steadySeconds = std::chrono::duration<double>(steadyStep).count();
          const double observedRate = steadySeconds / platformSeconds;
          mClockRate += (observedRate - mClockRate) * 0.02;
          const double nominal = 1.0 + mCalibratedPpm / 1000000.0;
          mClockRate = std::clamp(mClockRate, nominal * 0.95, nominal * 1.05);
        }
      }
      mLastPlatformSample = platform;
      mLastSteadySample = steadyNow;
      mHasClockSample = true;
      if (!mHasTimestampBase || platform < mTimestampBasePlatform || platformDelta > kResetGap) {
        mTimestampBasePlatform = platform;
        mTimestampBaseSteady = steadyNow;
        mClockDrift = std::chrono::steady_clock::duration::zero();
        mClockRate = 1.0 + mCalibratedPpm / 1000000.0;
        mHasTimestampBase = true;
      } else {
        const auto mapped = mTimestampBaseSteady +
          std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(std::chrono::duration<double>(platformDelta).count() *
              mClockRate));
        const auto error = mapped - steadyNow;
        constexpr auto kMaxDrift = std::chrono::milliseconds(500);
        if (error > kMaxDrift || error < -kMaxDrift) {
          mTimestampBasePlatform = platform;
          mTimestampBaseSteady = steadyNow;
          mClockDrift = std::chrono::steady_clock::duration::zero();
        } else {
          // Slow low-pass: platform clocks drift against the monotonic clock; correct the mapped
          // time instead of resetting the whole window on every jittery event.
          const double errorSeconds = std::chrono::duration<double>(error).count();
          const double driftSeconds = std::chrono::duration<double>(mClockDrift).count();
          // Adaptive low-pass: small jitter is smoothed heavily; a large sustained error adapts
          // faster instead of taking a fixed 16 samples.
          const double alpha = std::clamp(0.02 + std::abs(errorSeconds) * 2.0, 0.02, 0.25);
          mClockDrift = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(driftSeconds + (errorSeconds - driftSeconds) * alpha));
          now = std::min(steadyNow, mapped - mClockDrift);
        }
      }
    }
    mScrollSamples.push_back(ScrollSample{ now, std::abs(event.deltaY) });
    refreshPrefetchBoost(now);
    syncRows();
  }
  return handled;
}

void RecyclingListView::applyTheme(const Theme& theme) {
  ScrollView::applyTheme(theme);
  if (mAutoRowHeight) invalidateRowHeights();
}

const std::string* RecyclingListView::keyAt(const Point& position) const {
  if (mContent == nullptr || mRows.empty() || !bounds().contains(position)) return nullptr;
  const float relativeY = position.y - mContent->bounds().getTop();
  if (relativeY < 0.0f || relativeY >= contentHeight()) return nullptr;
  const auto offset = std::upper_bound(mRowOffsets.begin(), mRowOffsets.end(), relativeY);
  const int index = static_cast<int>(offset - mRowOffsets.begin()) - 1;
  if (index < 0 || index >= static_cast<int>(mRows.size())) return nullptr;
  return &mRows[static_cast<size_t>(index)];
}

bool RecyclingListView::onPointerDown(const PointerEvent& event) {
  if (const std::string* key = keyAt(event.position)) {
    setSelectedKey(*key);
    setFocusedKey(*key);
    return true;
  }
  return false;
}

bool RecyclingListView::onPointerMove(const PointerEvent& event) {
  const std::string* key = keyAt(event.position);
  setHoveredKey(key != nullptr ? *key : std::string());
  return false;
}

bool RecyclingListView::onPointerLeave() {
  setHoveredKey({});
  return false;
}

RecyclingListView::Slot* RecyclingListView::findSlot(const std::string& key) const {
  if (const auto it = mMounted.find(key); it != mMounted.end()) return it->second.get();
  if (const auto it = mPool.find(key); it != mPool.end()) return it->second.get();
  return nullptr;
}

void RecyclingListView::removeMissingRows() {
  auto absent = [this](const std::string& key) {
    return std::find(mRows.begin(), mRows.end(), key) == mRows.end();
  };
  for (auto it = mMounted.begin(); it != mMounted.end();) {
    if (absent(it->first)) {
      detachFocus(it->first, it->second->layer);
      mLastRowBounds.erase(it->first);
      if (it->second->layer != nullptr && mContent != nullptr &&
          it->second->layer->parent() == mContent.get()) {
        if (mRowTransition.count() > 0 && !reduceMotion()) {
          beginExit(it->second->layer);
        } else {
          mContent->removeChild(it->second->layer);
        }
      }
      it = mMounted.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = mPool.begin(); it != mPool.end();) {
    if (absent(it->first)) {
      if (it->first == mFocusedKey) mFocusedDescendant = nullptr;
      mPoolMeta.erase(it->first);
      it = mPool.erase(it);
    } else {
      ++it;
    }
  }
  trimPool();
}

void RecyclingListView::syncRows() {
  if (mContent == nullptr || mSyncDepth >= 4) return;
  struct MoveGuard {
    bool& flag;
    ~MoveGuard() { flag = false; }
  } moveGuard{ mSuppressRowMove };
  struct DepthGuard {
    int& depth;
    explicit DepthGuard(int& value) : depth(value) { ++depth; }
    ~DepthGuard() { --depth; }
  } guard(mSyncDepth);
  if (mRows.empty()) {
    for (auto& entry: mMounted) {
      detachFocus(entry.first, entry.second->layer);
      if (entry.second->layer != nullptr && entry.second->layer->parent() == mContent.get()) {
        mContent->removeChild(entry.second->layer);
      }
      mPoolMeta[entry.first].lastIndex = entry.second->index;
      mPool.emplace(entry.first, std::move(entry.second));
    }
    mMounted.clear();
    trimPool();
    mVisibleFirst = mVisibleLast = 0;
    return;
  }

  const float contentTop = mContent->bounds().getTop();
  const float contentLeft = mContent->bounds().getLeft();
  const float contentWidth = mContent->bounds().getWidth();
  const float viewportTop = bounds().getTop();
  const float viewportBottom = bounds().getBottom();
  const int count = static_cast<int>(mRows.size());
  const float relativeTop = viewportTop - contentTop;
  const float relativeBottom = viewportBottom - contentTop;
  const auto firstOffset = std::upper_bound(mRowOffsets.begin(), mRowOffsets.end(),
    std::max(0.0f, relativeTop));
  int first = std::max(0, static_cast<int>(firstOffset - mRowOffsets.begin()) - 1);
  const auto lastOffset = std::upper_bound(mRowOffsets.begin(), mRowOffsets.end(),
    std::max(0.0f, relativeBottom));
  int last = static_cast<int>(lastOffset - mRowOffsets.begin()) - 1;
  first = std::clamp(first, 0, count - 1);
  last = std::clamp(last, first, count - 1);
  mVisibleFirst = static_cast<size_t>(first);
  mVisibleLast = static_cast<size_t>(last);

  std::unordered_map<std::string, bool> visible;
  visible.reserve(static_cast<size_t>(last - first + 1));
  for (int index = first; index <= last; ++index) {
    const std::string& key = mRows[static_cast<size_t>(index)];
    visible[key] = true;

    std::unique_ptr<Slot> slot;
    if (const auto mounted = mMounted.find(key); mounted != mMounted.end()) {
      slot = std::move(mounted->second);
      mMounted.erase(mounted);
    } else if (const auto pooled = mPool.find(key); pooled != mPool.end()) {
      slot = std::move(pooled->second);
      mPool.erase(pooled);
      ++mPoolHits;
    } else {
      slot = std::make_unique<Slot>();
      slot->key = key;
      slot->reconciler = std::make_unique<Reconciler>();
    }

    slot->index = static_cast<size_t>(index);
    touchPoolEntry(key, static_cast<size_t>(index));
    const ElementPtr element = mBuilder ? mBuilder(key, static_cast<size_t>(index), stateFor(key)) : nullptr;
    const std::shared_ptr<Layer> previousLayer = slot->layer;
    const bool elementChanged = element != slot->element;
    const bool measureChanged = elementChanged &&
      (slot->element == nullptr || element == nullptr ||
        !element->measureEquivalent(*slot->element));
    if (elementChanged) {
      slot->element = element;
      if (measureChanged) slot->measureDirty = true;
      if (element != nullptr) {
        if (!slot->reconciler->mounted()) {
          slot->layer = slot->reconciler->mount(element);
        } else {
          slot->reconciler->update(element);
          slot->layer = slot->reconciler->root();
        }
      }
    }
    if (previousLayer != nullptr && previousLayer != slot->layer &&
        previousLayer->parent() == mContent.get()) {
      if (mRowTransition.count() > 0 && !reduceMotion()) {
        beginExit(previousLayer);
      } else {
        mContent->removeChild(previousLayer);
      }
    }
    if (slot->layer != nullptr) {
      const float newTop = contentTop + mRowOffsets[static_cast<size_t>(index)];
      const float rowHeight = rowHeightAt(static_cast<size_t>(index));
      const auto previous = mLastRowBounds.find(key);
      const bool moved = previous != mLastRowBounds.end() &&
        (std::abs(previous->second.getTop() - newTop) > 0.5f ||
          std::abs(previous->second.getHeight() - rowHeight) > 0.5f);
      const std::chrono::milliseconds moveDuration = mRowMoveTransition.count() >= 0
        ? mRowMoveTransition
        : mRowTransition;
      if (moveDuration.count() > 0 && moved && !mSuppressRowMove &&
          slot->layer->parent() == mContent.get()) {
        // Keyed rows that shift or change height animate from their old rectangle instead of jumping.
        const Rect from{ Rect::MakeXYWH(contentLeft, previous->second.getTop(), contentWidth,
          previous->second.getHeight()) };
        slot->layer->setBounds(from);
        slot->layer->animateBounds(Rect::MakeXYWH(contentLeft, newTop, contentWidth, rowHeight),
          moveDuration);
      } else if (!slot->layer->animatingBounds()) {
        // A later sync pass in the same frame must not cancel an animation that is already heading
        // for this exact target.
        slot->layer->setBounds(Rect::MakeXYWH(contentLeft, newTop, contentWidth, rowHeight));
      }
      mLastRowBounds[key] = Rect::MakeXYWH(contentLeft, newTop, contentWidth, rowHeight);
      if (slot->layer->parent() != mContent.get()) {
        mContent->addChild(slot->layer);
        if (mRowTransition.count() > 0 && !reduceMotion()) {
          slot->layer->setOpacity(0.0f);
          slot->layer->animateOpacity(1.0f, mRowTransition);
        }
      }
      // Give the row a stable accessible name (its key) so accessibility bridges announce rows
      // instead of skipping them as anonymous generic layers.
      slot->layer->setAccessibleName(key);
      slot->layer->setAccessibleRole(AccessibilityRole::ListItem);
      // The list owns the selection; a row only reports it and asks the list to change it.
      slot->layer->setAccessibleSelected(key == mSelectedKey);
      slot->layer->setAccessibleOnSelect([this, key] { setSelectedKey(key); });
      const uint64_t revision = fontRevisionOf(*slot->layer);
      if (revision != slot->fontRevision) {
        slot->fontRevision = revision;
        slot->measureDirty = true;
      }
      if (key == mFocusedKey) {
        restoreFocus(key, slot->layer);
        if (mFocusRequester != nullptr) mFocusRequester(slot->layer.get());
      }
    }
    mMounted.emplace(key, std::move(slot));
  }

  bool prefetchedChanged = false;
  const size_t prefetchRows = effectiveMeasurePrefetchRows();
  if (mAutoRowHeight && !mHeightProvider && prefetchRows > 0) {
    const auto prefetch = [&](const int target) {
      if (target < 0 || target >= count) return;
      const std::string& key = mRows[static_cast<size_t>(target)];
      if (mMounted.find(key) != mMounted.end() || mPool.find(key) != mPool.end()) return;
      if (mIntrinsicHeights.find(key) != mIntrinsicHeights.end()) return;
      const ElementPtr element = mBuilder
        ? mBuilder(key, static_cast<size_t>(target), stateFor(key)) : nullptr;
      if (element == nullptr) return;
      auto slot = std::make_unique<Slot>();
      slot->key = key;
      slot->index = static_cast<size_t>(target);
      slot->reconciler = std::make_unique<Reconciler>();
      slot->element = element;
      slot->layer = slot->reconciler->mount(element);
      if (slot->layer == nullptr) return;
      const float width = contentWidth > 0.0f ? contentWidth : std::max(1.0f, bounds().getWidth());
      const Size measured = slot->layer->measure({ width, 0.0f });
      ++mMeasureCount;
      const float height = std::max(1.0f, measured.height);
      slot->measureDirty = false;
      slot->fontRevision = fontRevisionOf(*slot->layer);
      mIntrinsicHeights[key] = height;
      prefetchedChanged = true;
      mPoolMeta[key] = PoolMeta{ 0, static_cast<size_t>(target) };
      mPool.emplace(key, std::move(slot));
    };
    const int above = static_cast<int>(prefetchRows);
    for (int i = 1; i <= above; ++i) prefetch(first - i);
    for (int i = 1; i <= above; ++i) prefetch(last + i);
  }

  for (auto it = mMounted.begin(); it != mMounted.end();) {
    if (visible.find(it->first) != visible.end()) {
      ++it;
      continue;
    }
    detachFocus(it->first, it->second->layer);
    if (it->second->layer != nullptr && it->second->layer->parent() == mContent.get()) {
      mContent->removeChild(it->second->layer);
    }
    mPoolMeta[it->first].lastIndex = it->second->index;
    mPool.emplace(it->first, std::move(it->second));
    it = mMounted.erase(it);
  }
  trimPool();
  if (measureVisibleRows(contentWidth) || prefetchedChanged) {
    rebuildRowMetrics();
    if (mContent != nullptr) mContent->setNeedsLayout();
    syncRows();
  }
}


void RecyclingListView::setOnActivate(std::function<void(const std::string&)> callback) {
  mOnActivate = std::move(callback);
}

void RecyclingListView::ensureKeyVisible(const std::string& key) {
  const auto position = std::find(mRows.begin(), mRows.end(), key);
  if (position == mRows.end()) return;
  const size_t index = static_cast<size_t>(position - mRows.begin());
  if (index >= mRowOffsets.size()) return;
  const float viewport = bounds().getHeight();
  if (viewport <= 0.0f) return;
  const float top = mRowOffsets[index];
  const float bottom = top + rowHeightAt(index);
  const float current = scrollY();
  if (top < current) {
    setScrollOffset(scrollX(), top);
  } else if (bottom > current + viewport) {
    setScrollOffset(scrollX(), bottom - viewport);
  }
}

void RecyclingListView::scrollToKey(const std::string& key) {
  if (std::find(mRows.begin(), mRows.end(), key) == mRows.end()) return;
  setFocusedKey(key);
  ensureKeyVisible(key);
}

void RecyclingListView::moveFocus(const int delta) {
  if (mRows.empty()) return;
  size_t index = 0;
  if (!mFocusedKey.empty()) {
    const auto found = std::find(mRows.begin(), mRows.end(), mFocusedKey);
    index = found == mRows.end() ? 0 : static_cast<size_t>(found - mRows.begin());
    if (delta < 0) {
      const size_t back = static_cast<size_t>(-delta);
      index = index > back ? index - back : 0;
    } else {
      index = std::min(index + static_cast<size_t>(delta), mRows.size() - 1);
    }
  } else if (delta < 0) {
    index = mRows.size() - 1;
  }
  setFocusedKey(mRows[index]);
  ensureKeyVisible(mRows[index]);
}

bool RecyclingListView::onKeyDown(const KeyEvent& event) {
  switch (event.key) {
    case Key::Down: moveFocus(1); return true;
    case Key::Up: moveFocus(-1); return true;
    case Key::Home:
      if (mRows.empty()) return true;
      scrollToKey(mRows.front());
      return true;
    case Key::End:
      if (mRows.empty()) return true;
      scrollToKey(mRows.back());
      return true;
    case Key::PageDown: {
      const int page = std::max(1, static_cast<int>(bounds().getHeight() / mRowHeight));
      moveFocus(page);
      return true;
    }
    case Key::PageUp: {
      const int page = std::max(1, static_cast<int>(bounds().getHeight() / mRowHeight));
      moveFocus(-page);
      return true;
    }
    case Key::Enter:
    case Key::Space:
      if (mOnActivate && !mFocusedKey.empty()) {
        mOnActivate(mFocusedKey);
        return true;
      }
      return false;
    default:
      return false;
  }
}


void RecyclingListView::setStickyKeys(std::vector<std::string> keys) {
  mStickyKeys = std::move(keys);
  setNeedsPaint();
}

std::string RecyclingListView::stickyHeaderKey() const {
  std::string candidate;
  for (const std::string& key: mStickyKeys) {
    const auto position = std::find(mRows.begin(), mRows.end(), key);
    if (position == mRows.end()) continue;
    const size_t index = static_cast<size_t>(position - mRows.begin());
    if (index >= mRowOffsets.size()) continue;
    if (mRowOffsets[index] <= scrollY()) {
      candidate = key;
    } else if (candidate.empty()) {
      // Before the first section scrolls under the top, its header is already pinned.
      candidate = key;
      break;
    }
  }
  return candidate;
}

float RecyclingListView::stickyHeaderHeight() const {
  const std::string key = stickyHeaderKey();
  if (key.empty()) return 0.0f;
  const auto position = std::find(mRows.begin(), mRows.end(), key);
  if (position == mRows.end()) return 0.0f;
  return rowHeightAt(static_cast<size_t>(position - mRows.begin()));
}

void RecyclingListView::setRowTransition(const std::chrono::milliseconds duration) {
  mRowTransition = std::max(duration, std::chrono::milliseconds{ 0 });
}

void RecyclingListView::beginExit(const std::shared_ptr<Layer>& layer) {
  if (layer == nullptr || mContent == nullptr) return;
  layer->animateOpacity(0.0f, mRowTransition);
  mExiting.push_back(layer);
}

void RecyclingListView::onFrame(const std::chrono::steady_clock::time_point now) {
  ScrollView::onFrame(now);
  updateStickyLayer();
  for (auto it = mExiting.begin(); it != mExiting.end();) {
    if (*it == nullptr || !(*it)->hasRunningAnimation()) {
      if (*it != nullptr && mContent != nullptr && (*it)->parent() == mContent.get()) {
        mContent->removeChild(*it);
      }
      it = mExiting.erase(it);
    } else {
      ++it;
    }
  }
}

void RecyclingListView::updateStickyLayer() {
  if (mContent == nullptr) return;
  const std::string key = stickyHeaderKey();
  if (key.empty()) {
    if (mStickyLayer != nullptr && mStickyLayer->parent() == mContent.get()) {
      mContent->removeChild(mStickyLayer);
    }
    mStickyLayer.reset();
    mStickyBuiltKey.clear();
    return;
  }
  if (key != mStickyBuiltKey || mStickyLayer == nullptr) {
    if (mStickyLayer != nullptr && mStickyLayer->parent() == mContent.get()) {
      mContent->removeChild(mStickyLayer);
    }
    mStickyLayer.reset();
    if (mStickyReconciler == nullptr) mStickyReconciler = std::make_unique<Reconciler>();
    const auto position = std::find(mRows.begin(), mRows.end(), key);
    const size_t index = position == mRows.end()
      ? 0
      : static_cast<size_t>(position - mRows.begin());
    RecyclingRowState state;
    state.selected = key == mSelectedKey;
    state.focused = key == mFocusedKey;
    state.hovered = key == mHoveredKey;
    const ElementPtr element = mBuilder(key, index, state);
    if (element == nullptr) return;
    mStickyLayer = mStickyReconciler->mount(element);
    if (mStickyLayer == nullptr) return;
    mStickyLayer->setZIndex(5000);
    mContent->addChild(mStickyLayer);
    mStickyBuiltKey = key;
  }
  mStickyLayer->setVisible(true);
  mStickyLayer->setBounds(Rect::MakeXYWH(mContent->bounds().getLeft(),
    mContent->bounds().getTop() + scrollY(), mContent->bounds().getWidth(),
    stickyHeaderHeight()));
  mStickyLayer->layout();
}

void RecyclingListView::setRowMoveTransition(const std::chrono::milliseconds duration) {
  mRowMoveTransition = duration;
}

void RecyclingListView::setScrollAnchoring(const bool enabled) {
  mScrollAnchoring = enabled;
}
} // namespace coral::ui
