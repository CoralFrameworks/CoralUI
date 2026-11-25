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

#include <ui/Breadcrumb.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Breadcrumb::Breadcrumb(std::vector<std::string> items, std::shared_ptr<const Font> font)
    : mItems(std::move(items)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
  mCurrent = mItems.empty() ? -1 : static_cast<int>(mItems.size()) - 1;
  if (mCurrent >= 0) setAccessibleValue(mItems[static_cast<size_t>(mCurrent)]);
}

void Breadcrumb::setItems(std::vector<std::string> items) {
  mItems = std::move(items);
  mCurrent = mItems.empty() ? -1 : static_cast<int>(mItems.size()) - 1;
  if (mCurrent >= 0) setAccessibleValue(mItems[static_cast<size_t>(mCurrent)]);
  setNeedsLayout();
  setNeedsPaint();
}

void Breadcrumb::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void Breadcrumb::setSeparator(std::string separator) {
  mSeparator = std::move(separator);
  setNeedsPaint();
}

void Breadcrumb::setColors(const Color foreground, const Color accent) {
  mForeground = foreground;
  mAccent = accent;
  setNeedsPaint();
}

Size Breadcrumb::measure(const Size& available) {
  (void)available;
  float width = 0.0f;
  for (size_t index = 0; index < mItems.size(); ++index) {
    width += mFont != nullptr ? mFont->measureWidth(mItems[index]) : 40.0f;
    if (index + 1 < mItems.size()) {
      width += mGap + (mFont != nullptr ? mFont->measureWidth(mSeparator) : 8.0f) + mGap;
    }
  }
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + 8.0f : 24.0f;
  setMeasuredSize({ width, std::max(22.0f, height) });
  return measuredSize();
}

std::vector<Rect> Breadcrumb::itemRects() const {
  std::vector<Rect> rects;
  float left = bounds().getLeft();
  for (size_t index = 0; index < mItems.size(); ++index) {
    const float width = mFont != nullptr ? mFont->measureWidth(mItems[index])
                                         : 40.0f;
    rects.push_back(Rect::MakeXYWH(left, bounds().getTop(), width, bounds().getHeight()));
    left += width;
    if (index + 1 < mItems.size()) {
      left += mGap + (mFont != nullptr ? mFont->measureWidth(mSeparator) : 8.0f) + mGap;
    }
  }
  return rects;
}

void Breadcrumb::drawSelf(RenderContext& context) {
  if (mFont == nullptr) return;
  auto& canvas = context.canvas();
  const auto rects = itemRects();
  for (size_t index = 0; index < mItems.size() && index < rects.size(); ++index) {
      const bool current = static_cast<int>(index) == mCurrent;
    detail::drawLabel(context, mItems[index], rects[index], *mFont,
      current ? mAccent : mForeground, rects[index].getLeft());
    if (index + 1 < mItems.size()) {
      const float separatorX = rects[index].getRight() + mGap;
      detail::drawLabel(context, mSeparator,
        Rect::MakeXYWH(separatorX, bounds().getTop(),
          mFont->measureWidth(mSeparator), bounds().getHeight()),
        *mFont, mForeground, separatorX);
    }
  }
  detail::drawFocusRing(canvas, bounds(), mFocused, mAccent, 4.0f, focusRingWidth());
}

bool Breadcrumb::onPointerDown(const PointerEvent& event) {
  const auto rects = itemRects();
  for (size_t index = 0; index < rects.size(); ++index) {
    if (rects[index].contains(event.position)) {
      mCurrent = static_cast<int>(index);
      setAccessibleValue(mItems[index]);
      setNeedsPaint();
      if (mOnSelect) mOnSelect(static_cast<int>(index), mItems[index]);
      return true;
    }
  }
  return true;
}

void Breadcrumb::setCurrentIndex(const int index) {
  if (mItems.empty()) {
    mCurrent = -1;
    return;
  }
  mCurrent = std::clamp(index, 0, static_cast<int>(mItems.size()) - 1);
  setAccessibleValue(mItems[static_cast<size_t>(mCurrent)]);
  setNeedsPaint();
}

bool Breadcrumb::onKeyDown(const KeyEvent& event) {
  if (mItems.empty()) return false;
  if (event.key == Key::Left) {
    setCurrentIndex(mCurrent <= 0 ? 0 : mCurrent - 1);
    return true;
  }
  if (event.key == Key::Right) {
    setCurrentIndex(mCurrent + 1 >= static_cast<int>(mItems.size())
      ? static_cast<int>(mItems.size()) - 1 : mCurrent + 1);
    return true;
  }
  if (event.key == Key::Enter || event.key == Key::Space) {
    if (mCurrent >= 0 && mCurrent < static_cast<int>(mItems.size()) && mOnSelect) {
      mOnSelect(mCurrent, mItems[static_cast<size_t>(mCurrent)]);
    }
    return true;
  }
  return false;
}

void Breadcrumb::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

void Breadcrumb::applyTheme(const Theme& theme) {
  mForeground = theme.controls.foreground;
  mAccent = theme.controls.accent;
  mGap = theme.spacing.xs;
  setNeedsPaint();
}

} // namespace coral::ui
