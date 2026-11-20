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

#include <ui/SplitView.h>

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

SplitView::SplitView(const Axis axis, const float fraction)
    : mAxis(axis), mFraction(std::clamp(fraction, 0.0f, 1.0f)) {}

void SplitView::setPanes(std::shared_ptr<Layer> first, std::shared_ptr<Layer> second) {
  if (mFirst != nullptr) removeChild(mFirst);
  if (mSecond != nullptr) removeChild(mSecond);
  mFirst = std::move(first);
  mSecond = std::move(second);
  if (mFirst != nullptr) addChild(mFirst);
  if (mSecond != nullptr) addChild(mSecond);
  setNeedsLayout();
  setNeedsPaint();
}

void SplitView::setFraction(const float fraction) {
  const float next = std::clamp(fraction, 0.0f, 1.0f);
  if (next == mFraction) {
    return;
  }
  mFraction = next;
  setNeedsLayout();
  setNeedsPaint();
  if (mOnFractionChanged) {
    mOnFractionChanged(mFraction);
  }
}

void SplitView::setDividerWidth(const float width) {
  mDividerWidth = std::max(1.0f, width);
  setNeedsLayout();
  setNeedsPaint();
}

void SplitView::setMinimumPaneSize(const float size) {
  mMinimumPaneSize = std::max(0.0f, size);
  setNeedsLayout();
}

Size SplitView::measure(const Size& available) {
  const float mainAvailable = mAxis == Axis::Row ? available.width : available.height;
  const float crossAvailable = mAxis == Axis::Row ? available.height : available.width;
  const Size paneAvailable{ mAxis == Axis::Row
    ? std::max(0.0f, mainAvailable - mDividerWidth)
    : crossAvailable,
    mAxis == Axis::Row
    ? crossAvailable
    : std::max(0.0f, mainAvailable - mDividerWidth) };
  const Size firstSize = mFirst != nullptr ? mFirst->measure(paneAvailable) : Size{};
  const Size secondSize = mSecond != nullptr ? mSecond->measure(paneAvailable) : Size{};
  const float main = (mAxis == Axis::Row ? firstSize.width + secondSize.width
    : firstSize.height + secondSize.height) + mDividerWidth;
  const float cross = std::max(mAxis == Axis::Row ? firstSize.height : firstSize.width,
    mAxis == Axis::Row ? secondSize.height : secondSize.width);
  const Size size = mAxis == Axis::Row
    ? Size{ main, cross }
    : Size{ cross, main };
  setMeasuredSize(size);
  return size;
}

void SplitView::layout() {
  const float totalMain = mAxis == Axis::Row ? bounds().getWidth() : bounds().getHeight();
  const float totalCross = mAxis == Axis::Row ? bounds().getHeight() : bounds().getWidth();
  const float available = std::max(0.0f, totalMain - mDividerWidth);
  const float minFraction = available > 0.0f
    ? std::min(0.5f, mMinimumPaneSize / available)
    : 0.0f;
  mFraction = std::clamp(mFraction, minFraction, 1.0f - minFraction);
  const float firstMain = available * mFraction;
  const float secondMain = available - firstMain;

  if (mFirst != nullptr) {
    const Rect firstBounds = mAxis == Axis::Row
      ? Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), firstMain, totalCross)
      : Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), totalCross, firstMain);
    mFirst->setBounds(firstBounds);
    mFirst->layout();
  }
  if (mSecond != nullptr) {
    const Rect secondBounds = mAxis == Axis::Row
      ? Rect::MakeXYWH(bounds().getLeft() + firstMain + mDividerWidth, bounds().getTop(),
          secondMain, totalCross)
      : Rect::MakeXYWH(bounds().getLeft(), bounds().getTop() + firstMain + mDividerWidth,
          totalCross, secondMain);
    mSecond->setBounds(secondBounds);
    mSecond->layout();
  }
  clearNeedsLayout();
}

Rect SplitView::dividerBounds() const {
  const float totalMain = mAxis == Axis::Row ? bounds().getWidth() : bounds().getHeight();
  const float available = std::max(0.0f, totalMain - mDividerWidth);
  const float firstMain = available * mFraction;
  return mAxis == Axis::Row
    ? Rect::MakeXYWH(bounds().getLeft() + firstMain, bounds().getTop(), mDividerWidth,
        bounds().getHeight())
    : Rect::MakeXYWH(bounds().getLeft(), bounds().getTop() + firstMain, bounds().getWidth(),
        mDividerWidth);
}

void SplitView::drawSelf(RenderContext& context) {
  graphics::Paint paint;
  paint.color = mDividerColor;
  paint.style = graphics::Paint::Style::kFill;
  context.canvas().drawRect(dividerBounds(), paint);
}

bool SplitView::onPointerDown(const PointerEvent& event) {
  const Rect divider = dividerBounds();
  const Rect expanded = Rect::MakeLTRB(divider.getLeft() - 4.0f, divider.getTop() - 4.0f,
    divider.getRight() + 4.0f, divider.getBottom() + 4.0f);
  if (!expanded.contains(event.position)) {
    return false;
  }
  mDragging = true;
  updateFromPointer(event.position);
  return true;
}

bool SplitView::onPointerMove(const PointerEvent& event) {
  if (!mDragging) {
    return false;
  }
  updateFromPointer(event.position);
  return true;
}

bool SplitView::onPointerUp(const PointerEvent& event) {
  (void)event;
  if (!mDragging) {
    return false;
  }
  mDragging = false;
  return true;
}

void SplitView::updateFromPointer(const Point& point) {
  const float totalMain = mAxis == Axis::Row ? bounds().getWidth() : bounds().getHeight();
  const float available = std::max(1.0f, totalMain - mDividerWidth);
  const float offset = (mAxis == Axis::Row ? point.x - bounds().getLeft()
    : point.y - bounds().getTop()) - mDividerWidth * 0.5f;
  setFraction(offset / available);
}

void SplitView::applyTheme(const Theme& theme) {
  mDividerColor = theme.controls.border;
  setNeedsPaint();
}

} // namespace coral::ui
