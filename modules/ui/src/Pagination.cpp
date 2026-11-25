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

#include <ui/Pagination.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <string>
#include <utility>

namespace coral::ui {

Pagination::Pagination(std::shared_ptr<const Font> font, const int pageCount, const int page)
    : mFont(std::move(font)), mPageCount(std::max(1, pageCount)),
      mPage(std::clamp(page, 0, std::max(0, pageCount - 1))) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName("Pagination");
  setAccessibleValue(std::to_string(mPage + 1) + " / " + std::to_string(mPageCount));
}

void Pagination::setPageCount(const int pageCount) {
  mPageCount = std::max(1, pageCount);
  mPage = std::clamp(mPage, 0, mPageCount - 1);
  setAccessibleValue(std::to_string(mPage + 1) + " / " + std::to_string(mPageCount));
  setNeedsLayout();
  setNeedsPaint();
}

void Pagination::setPage(const int page) {
  choose(page);
}

Size Pagination::measure(const Size& available) {
  (void)available;
  const int count = visibleCount() + 2;
  setMeasuredSize({ static_cast<float>(count) * mItem +
    static_cast<float>(count - 1) * mGap, 30.0f });
  return measuredSize();
}

void Pagination::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  const int first = firstPage();
  const int count = visibleCount();
  const int total = count + 2;
  for (int index = 0; index < total; ++index) {
    const Rect item = Rect::MakeXYWH(bounds().getLeft() + static_cast<float>(index) *
      (mItem + mGap), bounds().getTop(), mItem, bounds().getHeight());
    const bool current = index > 0 && index <= count && first + index - 1 == mPage;
    graphics::Paint fill;
    fill.color = current ? mAccent : mBackground;
    fill.style = graphics::Paint::Style::kFill;
    canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(item, mRadius), fill);
    graphics::Paint stroke;
    stroke.color = mFocused ? mAccent : mBorder;
    stroke.style = graphics::Paint::Style::kStroke;
    stroke.strokeWidth = 1.0f;
    canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(item, mRadius), stroke);
    if (mFont == nullptr) continue;
    std::string label;
    if (index == 0) label = "<";
    else if (index == total - 1) label = ">";
    else label = std::to_string(first + index);
    const float width = mFont->measureWidth(label);
    detail::drawLabel(context, label, item, *mFont,
      current ? Color::MakeRGBA8(255, 255, 255, 255) : mForeground,
      item.getLeft() + (item.getWidth() - width) * 0.5f);
  }
  if (mFocused) {
    detail::drawFocusRing(canvas, bounds(), true, mAccent, mRadius, focusRingWidth());
  }
}

bool Pagination::onPointerDown(const PointerEvent& event) {
  if (event.position.y < bounds().getTop() || event.position.y > bounds().getBottom()) {
    return true;
  }
  const int index = indexAt(event.position);
  if (index < 0) return true;
  const int total = visibleCount() + 2;
  if (index == 0) {
    choose(mPage - 1);
  } else if (index == total - 1) {
    choose(mPage + 1);
  } else {
    choose(firstPage() + index - 1);
  }
  return true;
}

bool Pagination::onKeyDown(const KeyEvent& event) {
  if (event.key == Key::Left) {
    choose(mPage - 1);
    return true;
  }
  if (event.key == Key::Right) {
    choose(mPage + 1);
    return true;
  }
  if (event.key == Key::Home) {
    choose(0);
    return true;
  }
  if (event.key == Key::End) {
    choose(mPageCount - 1);
    return true;
  }
  return false;
}

void Pagination::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

bool Pagination::performAccessibilityAction(const AccessibilityAction action,
    const std::string_view value) {
  if (action != AccessibilityAction::SetValue) return false;
  try {
    choose(std::stoi(std::string(value)) - 1);
  } catch (...) {
    return false;
  }
  return true;
}

void Pagination::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mForeground = theme.controls.foreground;
  mAccent = theme.controls.accent;
  mBorder = theme.controls.border;
  mRadius = theme.radius.md;
  setNeedsPaint();
}

int Pagination::firstPage() const {
  if (mPageCount <= 5) return 0;
  return std::clamp(mPage - 2, 0, mPageCount - 5);
}

int Pagination::visibleCount() const {
  return std::min(5, mPageCount);
}

int Pagination::indexAt(const Point point) const {
  const int total = visibleCount() + 2;
  for (int index = 0; index < total; ++index) {
    const Rect item = Rect::MakeXYWH(bounds().getLeft() + static_cast<float>(index) *
      (mItem + mGap), bounds().getTop(), mItem, bounds().getHeight());
    if (item.contains(point)) return index;
  }
  return -1;
}

void Pagination::choose(const int page) {
  const int next = std::clamp(page, 0, mPageCount - 1);
  if (next == mPage) return;
  mPage = next;
  setAccessibleValue(std::to_string(mPage + 1) + " / " + std::to_string(mPageCount));
  setNeedsPaint();
  if (mOnChanged) mOnChanged(mPage);
}

} // namespace coral::ui
