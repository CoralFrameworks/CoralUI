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

#include <ui/ScrollView.h>
#include <ui/CanvasBackend.h>
#include <ui/Motion.h>

#include <ui/Theme.h>

#include <graphics/Canvas.h>
#include <gpu/DrawList.h>
#include <graphics/Paint.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace coral::ui {

ScrollView::ScrollView() {
  // Without a role the viewport is an unnamed generic layer, which the accessibility tree drops: a
  // screen reader would never see the scrolling container or offer its scroll actions.
  setAccessibleRole(AccessibilityRole::ScrollView);
}

ScrollView::~ScrollView() = default;

void ScrollView::setContent(std::shared_ptr<Layer> content) {
  if (mContent == content) {
    return;
  }
  if (mVirtualContent != nullptr && mContent != nullptr) {
    // A plain content replaces the lazy one; the provider has to drop the layers it pooled in the host.
    mVirtualContent->detach(*mContent);
  }
  mVirtualContent.reset();
  if (mContent != nullptr) {
    removeChild(mContent);
  }
  mContent = std::move(content);
  if (mContent != nullptr) {
    addChild(mContent);
  }
  mScrollX = 0.0f;
  mScrollY = 0.0f;
  setNeedsLayout();
  setNeedsPaint();
}

std::optional<AccessibleScroll> ScrollView::accessibleScroll() const {
  const float viewWidth = std::max(1.0f, bounds().getWidth());
  const float viewHeight = std::max(1.0f, bounds().getHeight());
  const float contentWidth = std::max(viewWidth, mContentSize.width);
  const float contentHeight = std::max(viewHeight, mContentSize.height);
  const float maxX = std::max(0.0f, contentWidth - viewWidth);
  const float maxY = std::max(0.0f, contentHeight - viewHeight);
  AccessibleScroll scroll;
  scroll.canScrollHorizontally = maxX > 0.0f;
  scroll.canScrollVertically = maxY > 0.0f;
  scroll.horizontalPercent = maxX > 0.0f ? std::clamp(mScrollX / maxX, 0.0f, 1.0f) : 0.0f;
  scroll.verticalPercent = maxY > 0.0f ? std::clamp(mScrollY / maxY, 0.0f, 1.0f) : 0.0f;
  scroll.horizontalViewSize = viewWidth / contentWidth;
  scroll.verticalViewSize = viewHeight / contentHeight;
  return scroll;
}

bool ScrollView::performAccessibilityAction(const AccessibilityAction action,
    const std::string_view value) {
  if (action != AccessibilityAction::Scroll) {
    return Layer::performAccessibilityAction(action, value);
  }
  const float viewWidth = std::max(1.0f, bounds().getWidth());
  const float viewHeight = std::max(1.0f, bounds().getHeight());
  const float maxX = std::max(0.0f, mContentSize.width - viewWidth);
  const float maxY = std::max(0.0f, mContentSize.height - viewHeight);
  float nextX = mScrollX;
  float nextY = mScrollY;
  // The platform bridge does not know our units, so it sends a command; percentages are absolute and
  // the rest are relative to what the user can see.
  const auto startsWith = [value](const std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
  };
  if (startsWith("set:")) {
    const std::string text(value.substr(4));
    const std::size_t comma = text.find(',');
    if (comma == std::string::npos) return false;
    const auto percent = [](const std::string& part) -> std::optional<float> {
      try {
        return std::stof(part);
      } catch (const std::exception&) {
        return std::nullopt;
      }
    };
    const std::optional<float> horizontal = percent(text.substr(0, comma));
    const std::optional<float> vertical = percent(text.substr(comma + 1));
    if (!horizontal.has_value() || !vertical.has_value()) return false;
    nextX = maxX * std::clamp(*horizontal, 0.0f, 1.0f);
    nextY = maxY * std::clamp(*vertical, 0.0f, 1.0f);
  } else if (value == "lineUp") {
    nextY -= 40.0f;
  } else if (value == "lineDown") {
    nextY += 40.0f;
  } else if (value == "lineLeft") {
    nextX -= 40.0f;
  } else if (value == "lineRight") {
    nextX += 40.0f;
  } else if (value == "pageUp") {
    nextY -= viewHeight * 0.9f;
  } else if (value == "pageDown") {
    nextY += viewHeight * 0.9f;
  } else if (value == "pageLeft") {
    nextX -= viewWidth * 0.9f;
  } else if (value == "pageRight") {
    nextX += viewWidth * 0.9f;
  } else {
    return false;
  }
  setScrollOffset(nextX, nextY);
  return true;
}

void ScrollView::setScrollOffset(const float x, const float y) {
  const float previousX = mScrollX;
  const float previousY = mScrollY;
  mScrollX = x;
  mScrollY = y;
  clampScroll();
  // A wheel event at an edge, or a caller that re-applies the offset it already had, must not
  // invalidate the viewport: at the viewport's scale that invalidation means repainting its content.
  // ## The offset snaps to the device grid
  //
  // A plan frame moves the content with the canvas translation, and a translation that is not a whole
  // device pixel cannot share the frame around it: coverage fields and clip masks are placed on whole
  // pixels, so a fractional move would open a frame per list. Snapping bounds the drift at half a device
  // pixel and keeps text on the pixel grid, which is what a fractional scroll would blur anyway.
  if (mLastScale > 0.0f) {
    mScrollX = bounds().getLeft() - std::round((bounds().getLeft() - mScrollX) * mLastScale) / mLastScale;
    mScrollY = bounds().getTop() - std::round((bounds().getTop() - mScrollY) * mLastScale) / mLastScale;
    clampScroll();
  }
  if (mScrollX == previousX && mScrollY == previousY) return;
  // A composite viewport positions its content with an item translation, so the scroll does not move
  // anything the layout produced: a paint is enough. Asking for a layout here is what made scrolling
  // re-measure and re-arrange the whole content every frame (the largest term left after R10).
  if (!mCompositeScroll) setNeedsLayout();
  setNeedsPaint();
}

void ScrollView::setScrollOffsetUnclamped(const float x, const float y) {
  if (mScrollX == x && mScrollY == y) return;
  mScrollX = x;
  mScrollY = y;
  setNeedsLayout();
  setNeedsPaint();
}

void ScrollView::setBounceEnabled(const bool enabled) {
  mBounceEnabled = enabled;
  if (!enabled) {
    mBounceReturnPending = false;
    setScrollOffset(mScrollX, mScrollY);
  }
}

void ScrollView::setBounceLimit(const float limit) {
  mBounceLimit = std::max(0.0f, limit);
}

Size ScrollView::measure(const Size& available) {
  if (mContent == nullptr) {
    setMeasuredSize(bounds().getSize());
    return measuredSize();
  }
  const float width = bounds().getWidth() > 0.0f ? bounds().getWidth()
    : (available.width > 0.0f ? available.width : 0.0f);
  if (mVirtualContent != nullptr) {
    // A lazy content measures itself: asking the host would measure the few rows it happens to hold.
    mContentSize = mVirtualContent->contentSize(Size{ width, available.height });
  } else {
    mContentSize = mContent->measureCached({ width, 0.0f });
  }
  const float viewportWidth = width > 0.0f ? width : mContentSize.width;
  // Do not use the currently assigned bounds as the preferred height: after a window was shrunk, the
  // assigned height can be a collapsed value and would otherwise stay collapsed when it grows again.
  const float availableHeight = available.height > 0.0f ? available.height : mContentSize.height;
  const float preferred = mPreferredHeight > 0.0f ? mPreferredHeight : mContentSize.height;
  const float viewportHeight = std::min(preferred, availableHeight);
  setMeasuredSize({ viewportWidth, viewportHeight });
  return measuredSize();
}

void ScrollView::setVirtualContent(std::shared_ptr<VirtualContent> content) {
  if (mVirtualContent == content) return;
  if (mVirtualContent != nullptr && mContent != nullptr) {
    mVirtualContent->detach(*mContent);
  }
  mVirtualContent.reset();
  if (content == nullptr) {
    setNeedsLayout();
    setNeedsPaint();
    return;
  }
  // The host is a plain layer: the provider positions its children itself, so the viewport's layout must
  // not re-arrange them.
  auto host = std::make_shared<Layer>();
  host->setClipsToBounds(false);
  setContent(host);
  mVirtualContent = std::move(content);
  mRealizedTop = 0.0f;
  mRealizedBottom = -1.0f;
  mContentSize = mVirtualContent->contentSize(Size{ bounds().getWidth(), bounds().getHeight() });
  setNeedsLayout();
  setNeedsPaint();
}

void ScrollView::realizeVirtualContent() {
  if (mVirtualContent == nullptr || mContent == nullptr) return;
  const float viewport = std::max(1.0f, bounds().getHeight());
  // One viewport of slack on each side: scrolling within it needs no new work, and crossing it costs one
  // realization per viewport of travel rather than one per frame.
  const float margin = viewport;
  const float top = std::max(0.0f, mScrollY - margin);
  const float bottom = std::min(mContentSize.height, mScrollY + viewport + margin);
  if (bottom <= top) return;
  if (mRealizedBottom > mRealizedTop && top >= mRealizedTop && bottom <= mRealizedBottom) return;
  mVirtualContent->realize(*mContent, top, bottom);
  mRealizedTop = top;
  mRealizedBottom = bottom;
  // The provider positioned the rows; their own children still have to be laid out before this frame
  // draws them, or the first frame after a realization shows the previous arrangement.
  mContent->layout();
  // The provider set bounds on its rows, which dirties them; the content itself has to be dirty too, or
  // the plan path would keep a band recorded from the previous range.
  mContent->setNeedsPaint();
}

void ScrollView::layout() {
  if (mContent != nullptr) {
    if (mVirtualContent != nullptr) {
      mContentSize = mVirtualContent->contentSize(Size{ bounds().getWidth(), bounds().getHeight() });
    } else {
      mContentSize = mContent->measureCached({ bounds().getWidth(), 0.0f });
    }
    clampScroll();
    updateContentBounds();
    realizeVirtualContent();
    mContent->layout();
  }
  clearNeedsLayout();
}

void ScrollView::updateContentBounds() {
  if (mContent == nullptr) {
    return;
  }
  // In a plan-mode frame the scroll offset is a translation on the content's plan item, so the content
  // keeps a fixed device position: that is what lets its recorded list survive a scroll untouched.
  const float offsetX = mCompositeScroll ? 0.0f : mScrollX;
  const float offsetY = mCompositeScroll ? 0.0f : mScrollY;
  const float contentWidth = std::max(bounds().getWidth(), mContentSize.width);
  // RTL measures the horizontal offset from the right edge: at zero the content's right edge meets
  // the viewport's, and scrolling moves the content right to reveal the earlier part. LTR measures
  // from the left and moves left, which is the same formula with the sign flipped.
  const float left = direction() == Direction::Rtl
    ? bounds().getRight() - contentWidth + offsetX
    : bounds().getLeft() - offsetX;
  mContent->setBounds(Rect::MakeXYWH(left, bounds().getTop() - offsetY, contentWidth,
    mContentSize.height));
}

void ScrollView::clampScroll() {
  const float maxX = std::max(0.0f, mContentSize.width - bounds().getWidth());
  const float maxY = std::max(0.0f, mContentSize.height - bounds().getHeight());
  mScrollX = std::clamp(mScrollX, 0.0f, maxX);
  mScrollY = std::clamp(mScrollY, 0.0f, maxY);
}

void ScrollView::invalidateBands() {
  for (Band& band: mBands) {
    band.list.reset();
    band.bytes = 0;
  }
  mBandBytes = 0;
}

void ScrollView::trimBandCache(const std::size_t keep) {
  const std::size_t budget = std::max<std::size_t>(1, mBandBudgetBytes);
  while (mBandBytes > budget) {
    std::size_t victim = mBands.size();
    std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t index = 0; index < mBands.size(); ++index) {
      if (index == keep || mBands[index].list == nullptr) continue;
      if (mBands[index].lastUsed < oldest) {
        oldest = mBands[index].lastUsed;
        victim = index;
      }
    }
    if (victim == mBands.size()) break;
    mBandBytes -= mBands[victim].bytes;
    mBands[victim].list.reset();
    mBands[victim].bytes = 0;
  }
}

void ScrollView::resizeBandsIfNeeded(const float scale) {
  if (mContent == nullptr) return;
  const float contentTop = mContent->bounds().getTop();
  const Size contentSize{ std::max(bounds().getWidth(), mContentSize.width),
    std::max(bounds().getHeight(), mContentSize.height) };
  // A band boundary has to land on a whole device pixel: `ceil` makes the stride a whole number of
  // device pixels, so two adjacent bands share an edge exactly and a row straddling it is split without
  // a seam. A logical stride would put the edge half a pixel off at a fractional scale.
  const float deviceStride = std::max(1.0f, std::ceil(bounds().getHeight() * scale));
  const float stride = deviceStride / std::max(scale, 0.0001f);
  const float bandHeight = stride * 2.0f;
  if (mBandHeight == bandHeight && mBandStride == stride && mBandScale == scale &&
      mBandContentTop == contentTop &&
      mBandContentSize.width == contentSize.width && mBandContentSize.height == contentSize.height) {
    return;
  }
  mBands.clear();
  mBandBytes = 0;
  mBandHeight = bandHeight;
  mBandStride = stride;
  mBandScale = scale;
  mBandContentTop = contentTop;
  mBandContentSize = contentSize;
  const std::size_t count = static_cast<std::size_t>(
    std::max(1.0f, std::ceil(contentSize.height / stride)));
  mBands.resize(count);
  for (std::size_t index = 0; index < count; ++index) {
    mBands[index].top = contentTop + static_cast<float>(index) * stride;
  }
}

void ScrollView::draw(RenderContext& context) {
  if (!visible()) {
    return;
  }
  // A plan-mode frame takes the content as an item: the scroll offset becomes that item's translation and
  // the viewport its clip, so nothing is recorded under a canvas clip (which is what made a scroll cost a
  // full re-record). The viewport's own paint -- background and thumbs -- stays here.
  const bool wasComposite = mCompositeScroll;
  mCompositeScroll = context.assemblingPlan() && mContent != nullptr;
  if (mCompositeScroll != wasComposite) {
    // The bands are only meaningful while the plan path owns the content; a frame that ran the merged
    // path may have left them stale (or holding memory nobody will read).
    invalidateBands();
  }
  // The children's bounds are the layout's output, and the layout ran under the *previous* mode: a
  // viewport that turns composite keeps its children at the scrolled positions until a layout runs again.
  // That is one frame stale, and at scale 1 the translation hid it; at 1.5x it showed as a band out of
  // place. Layout the content once, here, when the mode changes.
  if (mCompositeScroll && !wasComposite && mContent != nullptr) {
    updateContentBounds();
    realizeVirtualContent();
    mContent->layout();
  }
  mLastScale = context.uiScale();
  auto& canvas = context.canvas();
  if (mBackground.alpha > 0.0f) {
    graphics::Paint background;
    background.color = mBackground;
    background.style = graphics::Paint::Style::kFill;
    canvas.drawRect(bounds(), background);
  }
  if (mContent != nullptr && context.assemblingPlan() && context.planSink() != nullptr) {
    updateContentBounds();
    realizeVirtualContent();
    // ## The content is lent in bands, not as one whole list
    //
    // A whole-content list carried every row of a 2400-row page into the device every frame: the record
    // was cheap (nothing re-recorded), but the submission precompiled every row the viewport never
    // showed. Bands cut the content into slices recorded under their slice's clip, so each list holds
    // only what it shows; bands overlap by one stride, so exactly one of them covers the viewport and a
    // frame submits exactly one band list -- its cost is the viewport's, whatever the content's height.
    const float scale = context.uiScale();
    if (mContent->subtreeNeedsPaint()) invalidateBands();
    resizeBandsIfNeeded(scale);
    const Rect viewportClip = Rect::MakeLTRB(bounds().getLeft() * scale,
      bounds().getTop() * scale, bounds().getRight() * scale, bounds().getBottom() * scale);
    const float contentLeft = mContent->bounds().getLeft();
    const float contentWidth = std::max(bounds().getWidth(), mContentSize.width);
    const float viewTop = std::max(0.0f, mScrollY);
    // Bands overlap by one stride, so the viewport is inside exactly one of them: `floor(scrollY /
    // stride)` names it. One band per frame is the point -- a second band would be a second list and a
    // second GPU frame for content the first band already covers.
    const float bandIndex = std::floor(viewTop / std::max(mBandStride, 0.001f));
    const std::size_t index = std::min(mBands.size() - 1,
      static_cast<std::size_t>(std::max(0.0f, bandIndex)));
    {
      Band& band = mBands[index];
      if (band.list == nullptr) {
        const Size deviceSize{ contentWidth * scale, std::max(1.0f, std::ceil(mBandHeight * scale)) };
        const Rect clip = Rect::MakeLTRB(contentLeft, band.top, contentLeft + contentWidth,
          band.top + mBandHeight);
        band.list = mContent->recordInto(deviceSize, scale, Point{ contentLeft, band.top }, clip);
        band.bytes = band.list != nullptr ? band.list->byteSize() : 0;
        mBandBytes += band.bytes;
        trimBandCache(index);
      }
      if (band.list != nullptr) {
        band.lastUsed = ++mBandClock;
        RenderContext::PlannedItem item;
        item.list = band.list;
        item.layer = mContent.get();
        // The list's origin is the band's top; the translation is what places it at the viewport.
        // As in `updateContentBounds`, RTL anchors the content's right edge and scrolls right.
        const float contentLeftNow = direction() == Direction::Rtl
          ? bounds().getRight() - contentWidth + mScrollX
          : bounds().getLeft() - mScrollX;
        item.translation = Point{ contentLeftNow * scale,
          (bounds().getTop() - mScrollY + (band.top - mBandContentTop)) * scale };
        item.clip = viewportClip;
        context.planSink()->push_back(item);
      }
    }
  } else {
    canvas.save();
    canvas.clipRect(bounds());
    if (mContent != nullptr) {
      updateContentBounds();
      realizeVirtualContent();
      mContent->draw(context);
    }
  }
  // A slim scrollbar is a hint, not a second layout pass: it is computed from the same sizes.
  // Both thumbs come from the same helpers the pointer handlers use, so the drawn geometry and the
  // hit geometry cannot drift; the helpers already shorten each track when the other bar is present.
  graphics::Paint thumb;
  thumb.color = mThumbColor;
  thumb.style = graphics::Paint::Style::kFill;
  const Rect vertical = scrollbarThumbBounds();
  const Rect horizontal = horizontalScrollbarThumbBounds();
  if (mContent != nullptr && context.assemblingPlan() && context.planSink() != nullptr) {
    // ## The viewport's own paint travels as its own item, after the content
    //
    // Everything a container draws into the frame's canvas is submitted *before* the lent lists, so a
    // scrollbar painted here would be covered by the content it belongs to. Recording it into a list and
    // appending that after the content item is what keeps "thumb above content" true in a plan frame.
    auto thumbList = std::make_shared<graphics::gpu::DrawList>();
    std::unique_ptr<graphics::Canvas> recorder = graphics::Canvas::createRecordingBackend(
      static_cast<int>(bounds().getWidth() * context.uiScale()),
      static_cast<int>(bounds().getHeight() * context.uiScale()), nullptr, thumbList);
    if (recorder != nullptr && (!vertical.isEmpty() || !horizontal.isEmpty())) {
      Canvas thumbCanvas = detail::makeCanvas(*recorder);
      thumbCanvas.scale(context.uiScale());
      // ## The list is anchored at a *device-aligned* origin
      //
      // A plan frame puts every list in one command buffer by baking whole-pixel translations; a
      // translation of `bounds().getTop() * scale` is fractional whenever the layout put the viewport
      // between pixels, which would cost this list a frame of its own. Anchoring at the device-aligned
      // corner and translating by the same corner keeps the list's device position exact (the
      // translation compensates the origin exactly) while making the translation an integer.
      const float scale = context.uiScale();
      const float anchorLeft = std::floor(bounds().getLeft() * scale) / scale;
      const float anchorTop = std::floor(bounds().getTop() * scale) / scale;
      thumbCanvas.translate(-anchorLeft, -anchorTop);
      if (!vertical.isEmpty()) thumbCanvas.drawRect(vertical, thumb);
      if (!horizontal.isEmpty()) thumbCanvas.drawRect(horizontal, thumb);
      RenderContext::PlannedItem item;
      item.list = thumbList;
      item.translation = Point{ anchorLeft * scale, anchorTop * scale };
      context.planSink()->push_back(item);
    }
  } else {
    if (!vertical.isEmpty()) canvas.drawRect(vertical, thumb);
    if (!horizontal.isEmpty()) canvas.drawRect(horizontal, thumb);
  }
  if (!mCompositeScroll) canvas.restore();
  clearNeedsLayout();
  clearPaintFlags();
}

void ScrollView::applyTheme(const Theme& theme) {
  mBackground = theme.surface;
  mThumbColor = theme.controls.muted;
  setNeedsPaint();
}

bool ScrollView::onScroll(const ScrollEvent& event) {
  if (!bounds().contains(event.position)) {
    return false;
  }
  const float beforeX = mScrollX;
  const float beforeY = mScrollY;
  // `deltaX` is a physical wheel/trackpad direction: positive means "right". In RTL, scrolling right
  // reveals the earlier part of the document, which is a *decrease* of the offset measured from the
  // right edge. Keeping the sign flip here is what makes the gesture agree with the bar it moves.
  const float horizontalDelta = direction() == Direction::Rtl ? -event.deltaX : event.deltaX;
  const float maxY = std::max(0.0f, mContentSize.height - bounds().getHeight());
  const float rawY = mScrollY - event.deltaY;
  if (mBounceEnabled && mBounceLimit > 0.0f && (rawY < 0.0f || rawY > maxY)) {
    setScrollOffsetUnclamped(mScrollX + horizontalDelta,
      std::clamp(rawY, -mBounceLimit, maxY + mBounceLimit));
    mBounceReturnPending = true;
    mFlingVelocity = 0.0f;
    mHasLastFrame = false;
    return true;
  }
  setScrollOffset(mScrollX + horizontalDelta, mScrollY - event.deltaY);
  // Scroll chaining: a wheel at an edge is not consumed, so the parent scroll view (or the page)
  // receives it instead of the gesture ending in a dead zone.
  if (mScrollX == beforeX && mScrollY == beforeY) {
    return false;
  }
  mFlingVelocity = 0.0f;
  mHasLastFrame = false;
  return true;
}


void ScrollView::animateScrollTo(const float y, const std::chrono::milliseconds duration,
    std::shared_ptr<const Curve> curve) {
  if (duration.count() <= 0 || reduceMotion()) {
    stopScrollAnimation();
    setScrollOffset(mScrollX, y);
    return;
  }
  mFlingVelocity = 0.0f;
  mHasLastFrame = false;
  mScrollAnimation.from = mScrollY;
  mScrollAnimation.to = y;
  mScrollAnimation.start = std::chrono::steady_clock::now();
  mScrollAnimation.duration = duration;
  mScrollAnimation.curve = curve != nullptr ? std::move(curve) : Curve::easeInOut();
  mScrollAnimation.active = true;
}

void ScrollView::stopScrollAnimation() {
  mScrollAnimation.active = false;
}


void ScrollView::fling(const float velocity) {
  stopScrollAnimation();
  mFlingVelocity = std::abs(velocity) > 1.0f ? velocity : 0.0f;
  mHasLastFrame = false;
}

void ScrollView::advanceFling(const std::chrono::steady_clock::time_point now) {
  if (mFlingVelocity == 0.0f) return;
  if (!mHasLastFrame) {
    mHasLastFrame = true;
    mLastFrame = now;
    return;
  }
  const float dt = std::min(
    std::chrono::duration<float>(now - mLastFrame).count(), 0.1f);
  mLastFrame = now;
  if (dt <= 0.0f) return;
  const float before = mScrollY;
  setScrollOffset(mScrollX, mScrollY + mFlingVelocity * dt);
  if (mScrollY == before) {
    // Reached an edge: momentum ends instead of fighting the clamp.
    mFlingVelocity = 0.0f;
    return;
  }
  mFlingVelocity *= std::pow(0.05f, dt);
  if (std::abs(mFlingVelocity) < 20.0f) mFlingVelocity = 0.0f;
}
void ScrollView::onFrame(const std::chrono::steady_clock::time_point now) {
  if (mBounceReturnPending && !mDraggingThumb && mFlingVelocity == 0.0f &&
      !mScrollAnimation.active) {
    mBounceReturnPending = false;
    const float targetY = std::clamp(mScrollY, 0.0f,
      std::max(0.0f, mContentSize.height - bounds().getHeight()));
    if (targetY != mScrollY) {
      animateScrollTo(targetY, std::chrono::milliseconds(180), Curve::easeOut());
    }
  }
  advanceFling(now);
  if (mScrollAnimation.active) {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - mScrollAnimation.start).count();
    const float t = mScrollAnimation.duration.count() > 0
      ? std::clamp(static_cast<float>(elapsed) /
            static_cast<float>(mScrollAnimation.duration.count()), 0.0f, 1.0f)
      : 1.0f;
    const float eased = mScrollAnimation.curve != nullptr
      ? mScrollAnimation.curve->transform(t)
      : t;
    setScrollOffset(mScrollX,
      mScrollAnimation.from + (mScrollAnimation.to - mScrollAnimation.from) * eased);
    if (t >= 1.0f) mScrollAnimation.active = false;
  }
  Layer::onFrame(now);
}

bool ScrollView::hasRunningAnimation() const {
  return mScrollAnimation.active || mFlingVelocity != 0.0f || Layer::hasRunningAnimation();
}


Rect ScrollView::scrollbarThumbBounds() const {
  if (mContentSize.height <= bounds().getHeight() || bounds().getHeight() <= 0.0f) return {};
  // Leave the bottom-right corner to the horizontal bar when both axes scroll.
  const bool both = mContentSize.width > bounds().getWidth();
  const float trackHeight = std::max(0.0f, bounds().getHeight() - (both ? 10.0f : 0.0f));
  const float thumbHeight = std::max(24.0f, trackHeight * (trackHeight / mContentSize.height));
  const float maxScroll = mContentSize.height - trackHeight;
  const float position = maxScroll > 0.0f
    ? (mScrollY / maxScroll) * (trackHeight - thumbHeight)
    : 0.0f;
  // An RTL interface puts the vertical bar on the left, where its reading starts; everything else
  // about the thumb (its length and scroll mapping) is direction-independent.
  const float barLeft = direction() == Direction::Rtl ? bounds().getLeft() + 2.0f
                                                      : bounds().getRight() - 6.0f;
  return Rect::MakeXYWH(barLeft, bounds().getTop() + position, 4.0f, thumbHeight);
}

bool ScrollView::onPointerDown(const PointerEvent& event) {
  const Rect horizontal = horizontalScrollbarThumbBounds();
  if (!horizontal.isEmpty() && horizontal.contains(event.position)) {
    mDraggingHorizontalThumb = true;
    mThumbDragStartY = event.position.x;
    mThumbDragStartScroll = mScrollX;
    return true;
  }
  const Rect thumb = scrollbarThumbBounds();
  if (thumb.isEmpty() || !thumb.contains(event.position)) return false;
  mDraggingThumb = true;
  mThumbDragStartY = event.position.y;
  mThumbDragStartScroll = mScrollY;
  return true;
}

bool ScrollView::onPointerMove(const PointerEvent& event) {
  if (mDraggingHorizontalThumb) {
    const float trackWidth = bounds().getWidth();
    const float thumbWidth = std::max(24.0f, trackWidth * (trackWidth / mContentSize.width));
    const float travel = trackWidth - thumbWidth;
    const float maxScroll = std::max(0.0f, mContentSize.width - trackWidth);
    if (travel <= 0.0f) return true;
    // In RTL the thumb is anchored at the right, so dragging it left advances the document.
    const float delta = direction() == Direction::Rtl ? mThumbDragStartY - event.position.x
                                                       : event.position.x - mThumbDragStartY;
    setScrollOffset(mThumbDragStartScroll + delta * (maxScroll / travel), mScrollY);
    return true;
  }
  if (!mDraggingThumb) return false;
  const float trackHeight = bounds().getHeight();
  const float thumbHeight = std::max(24.0f, trackHeight * (trackHeight / mContentSize.height));
  const float travel = trackHeight - thumbHeight;
  const float maxScroll = std::max(0.0f, mContentSize.height - trackHeight);
  if (travel <= 0.0f) return true;
  const float delta = event.position.y - mThumbDragStartY;
  setScrollOffset(mScrollX, mThumbDragStartScroll + delta * (maxScroll / travel));
  return true;
}

bool ScrollView::onPointerUp(const PointerEvent& event) {
  (void)event;
  if (mDraggingHorizontalThumb) {
    mDraggingHorizontalThumb = false;
    return true;
  }
  if (!mDraggingThumb) return false;
  mDraggingThumb = false;
  return true;
}

Rect ScrollView::horizontalScrollbarThumbBounds() const {
  if (mContentSize.width <= bounds().getWidth() || bounds().getWidth() <= 0.0f) return {};
  const bool both = mContentSize.height > bounds().getHeight();
  const float trackWidth = std::max(0.0f, bounds().getWidth() - (both ? 10.0f : 0.0f));
  const float thumbWidth = std::max(24.0f, trackWidth * (trackWidth / mContentSize.width));
  const float maxScroll = mContentSize.width - trackWidth;
  const float position = maxScroll > 0.0f
    ? (mScrollX / maxScroll) * (trackWidth - thumbWidth)
    : 0.0f;
  // The horizontal thumb mirrors its anchor in RTL. The scroll offset itself keeps its LTR meaning
  // (content moves left as mScrollX grows), which is a separate, larger change than the bar's side.
  const float thumbLeft = direction() == Direction::Rtl
    ? bounds().getRight() - position - thumbWidth
    : bounds().getLeft() + position;
  return Rect::MakeXYWH(thumbLeft, bounds().getBottom() - 6.0f, thumbWidth, 4.0f);
}
} // namespace coral::ui
