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

#include <ui/SegmentedControl.h>

#include "AccessibilityProxy.h"

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>

namespace coral::ui {

float SegmentedControl::segmentWidth(const std::string& label) const {
  return std::max(48.0f, (mFont != nullptr ? mFont->measureWidth(label) : 48.0f) +
    mSegmentPadding * 2.0f);
}

SegmentedControl::SegmentedControl(std::vector<std::string> segments,
    std::shared_ptr<const graphics::Font> font)
    : mSegments(std::move(segments)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::TabList);
  rebuildAccessibilityItems();
  if (!mSegments.empty()) setAccessibleValue(mSegments.front());
}

void SegmentedControl::setSegments(std::vector<std::string> segments) {
  mSegments = std::move(segments);
  mSelected = std::clamp(mSelected, 0,
    mSegments.empty() ? 0 : static_cast<int>(mSegments.size()) - 1);
  rebuildAccessibilityItems();
  setAccessibleValue(mSegments.empty() ? std::string() : mSegments[static_cast<size_t>(mSelected)]);
  setNeedsLayout();
  setNeedsPaint();
}

void SegmentedControl::setFont(std::shared_ptr<const graphics::Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void SegmentedControl::setColors(const graphics::Color background,
    const graphics::Color foreground, const graphics::Color accent) {
  mBackground = background;
  mForeground = foreground;
  mAccent = accent;
  setNeedsPaint();
}

void SegmentedControl::setSelectedIndex(const int index) {
  if (mSegments.empty()) {
    mSelected = 0;
    return;
  }
  mSelected = std::clamp(index, 0, static_cast<int>(mSegments.size()) - 1);
  if (!mSegments.empty()) setAccessibleValue(mSegments[static_cast<size_t>(mSelected)]);
  for (size_t index = 0; index < mAccessibilityItems.size(); ++index) {
    auto item = std::dynamic_pointer_cast<AccessibilityProxy>(mAccessibilityItems[index]);
    if (item != nullptr) item->setSelected(static_cast<int>(index) == mSelected);
  }
  setNeedsPaint();
}

Size SegmentedControl::measure(const Size& available) {
  (void)available;
  float width = 0.0f;
  for (const std::string& segment: mSegments) {
    width += segmentWidth(segment);
  }
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + 14.0f
    : 28.0f;
  setMeasuredSize({ width, std::max(26.0f, height) });
  return measuredSize();
}

void SegmentedControl::layout() {
  float left = bounds().getLeft();
  for (size_t index = 0; index < mAccessibilityItems.size() && index < mSegments.size(); ++index) {
    const float width = segmentWidth(mSegments[index]);
    if (mAccessibilityItems[index] != nullptr) {
      mAccessibilityItems[index]->setBounds(Rect::MakeXYWH(left, bounds().getTop(),
        width, bounds().getHeight()));
      mAccessibilityItems[index]->layout();
    }
    left += width;
  }
  clearNeedsLayout();
}

void SegmentedControl::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  // A named style replaces the theme copy for the track, the selected segment and the labels.
  graphics::Color track = mBackground;
  graphics::Color border = mBorder;
  graphics::Color selectedColor = mAccent;
  graphics::Color labelColor = mForeground;
  graphics::Color dividerColor = mDivider;
  float radius = mRadius;
  float borderWidth = 1.0f;
  if (const auto styled = resolvedStyle()) {
    track = styled->background;
    border = styled->border;
    selectedColor = styled->border;
    labelColor = styled->foreground;
    dividerColor = styled->border;
    radius = styled->radius;
    borderWidth = styled->borderWidth;
  }
  graphics::Paint background;
  background.color = track;
  background.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), radius), background);

  graphics::Paint outerBorder;
  outerBorder.color = border;
  outerBorder.style = graphics::Paint::Style::kStroke;
  outerBorder.strokeWidth = borderWidth;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), radius), outerBorder);

  if (mSegments.empty()) {
    detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, mRadius, focusRingWidth());
    return;
  }

  float left = bounds().getLeft();
  for (size_t index = 0; index < mSegments.size(); ++index) {
    const float width = segmentWidth(mSegments[index]);
    const Rect segment = Rect::MakeXYWH(left, bounds().getTop(), width, bounds().getHeight());
    if (static_cast<int>(index) == mSelected) {
      graphics::Paint selected;
      selected.color = selectedColor;
      selected.style = graphics::Paint::Style::kFill;
      canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(segment, radius), selected);
    }
    if (index > 0) {
      graphics::Paint divider;
      divider.color = dividerColor;
      divider.style = graphics::Paint::Style::kFill;
      canvas.drawRect(Rect::MakeXYWH(left, bounds().getTop(), 1.0f, bounds().getHeight()),
        divider);
    }
    if (mFont != nullptr) {
      const float textWidth = mFont->measureWidth(mSegments[index]);
      detail::drawLabel(context, mSegments[index], segment, *mFont, labelColor,
        segment.getLeft() + (width - textWidth) * 0.5f);
    }
    left += width;
  }
  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, mRadius, focusRingWidth());
}

bool SegmentedControl::onPointerDown(const PointerEvent& event) {
  if (mSegments.empty()) {
    return true;
  }
  float left = bounds().getLeft();
  for (size_t index = 0; index < mSegments.size(); ++index) {
    const float width = segmentWidth(mSegments[index]);
    if (event.position.x >= left && event.position.x <= left + width) {
      select(static_cast<int>(index));
      return true;
    }
    left += width;
  }
  return true;
}

bool SegmentedControl::onKeyDown(const KeyEvent& event) {
  if (mSegments.empty()) {
    return false;
  }
  if (event.key == Key::Left) {
    select(mSelected == 0 ? static_cast<int>(mSegments.size()) - 1 : mSelected - 1);
    return true;
  }
  if (event.key == Key::Right) {
    select((mSelected + 1) % static_cast<int>(mSegments.size()));
    return true;
  }
  if (event.key == Key::Home) {
    select(0);
    return true;
  }
  if (event.key == Key::End) {
    select(static_cast<int>(mSegments.size()) - 1);
    return true;
  }
  return false;
}

void SegmentedControl::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

bool SegmentedControl::performAccessibilityAction(const AccessibilityAction action,
    const std::string_view value) {
  if (action != AccessibilityAction::Select) return false;
  const std::string text(value);
  char* end = nullptr;
  const long index = std::strtol(text.c_str(), &end, 10);
  if (end != text.c_str() && *end == '\0') {
    select(static_cast<int>(index));
    return true;
  }
  for (size_t segment = 0; segment < mSegments.size(); ++segment) {
    if (mSegments[segment] == text) {
      select(static_cast<int>(segment));
      return true;
    }
  }
  return false;
}

void SegmentedControl::rebuildAccessibilityItems() {
  for (const auto& item: mAccessibilityItems) {
    if (item != nullptr) removeChild(item);
  }
  mAccessibilityItems.clear();
  for (size_t index = 0; index < mSegments.size(); ++index) {
    auto item = std::make_shared<AccessibilityProxy>(mSegments[index]);
    item->setSelected(false);
    item->setOnSelect([this, index] { select(static_cast<int>(index)); });
    addChild(item);
    mAccessibilityItems.push_back(item);
  }
  if (!mAccessibilityItems.empty()) {
    auto first = std::dynamic_pointer_cast<AccessibilityProxy>(mAccessibilityItems.front());
    if (first != nullptr) first->setSelected(mSelected == 0);
  }
}

void SegmentedControl::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mDivider = theme.controls.track;
  mFocus = theme.controls.borderFocused;
  mForeground = theme.controls.foreground;
  mAccent = theme.controls.accent;
  mSegmentPadding = theme.spacing.md;
  mRadius = theme.radius.md;
  setNeedsLayout();
  setNeedsPaint();
}

void SegmentedControl::select(const int index) {
  if (index < 0 || index >= static_cast<int>(mSegments.size()) || index == mSelected) {
    return;
  }
  mSelected = index;
  setAccessibleValue(mSegments[static_cast<size_t>(mSelected)]);
  for (size_t item = 0; item < mAccessibilityItems.size(); ++item) {
    auto proxy = std::dynamic_pointer_cast<AccessibilityProxy>(mAccessibilityItems[item]);
    if (proxy != nullptr) proxy->setSelected(static_cast<int>(item) == mSelected);
  }
  setNeedsPaint();
  if (mOnChanged) {
    mOnChanged(mSelected);
  }
}

} // namespace coral::ui
