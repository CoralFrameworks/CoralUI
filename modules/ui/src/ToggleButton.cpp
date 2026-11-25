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

#include <ui/ToggleButton.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <utility>

namespace coral::ui {

ToggleButton::ToggleButton(std::string label, std::shared_ptr<const Font> font, const bool selected)
    : mLabel(std::move(label)), mFont(std::move(font)), mSelected(selected) {
  setAccessibleRole(AccessibilityRole::CheckBox);
  setAccessibleName(mLabel);
  setAccessibleSelected(mSelected);
}

void ToggleButton::setLabel(std::string label) {
  mLabel = std::move(label);
  setAccessibleName(mLabel);
  setNeedsLayout();
  setNeedsPaint();
}

void ToggleButton::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void ToggleButton::setSelected(const bool selected) {
  if (mSelected == selected) return;
  mSelected = selected;
  setAccessibleSelected(mSelected);
  setNeedsPaint();
  if (mOnChanged) mOnChanged(mSelected);
}

void ToggleButton::setColors(const Color background, const Color selectedBackground,
    const Color foreground, const Color selectedForeground, const Color border,
    const Color focus) {
  mBackground = background;
  mSelectedBackground = selectedBackground;
  mForeground = foreground;
  mSelectedForeground = selectedForeground;
  mBorder = border;
  mFocus = focus;
  setNeedsPaint();
}

Size ToggleButton::measure(const Size& available) {
  (void)available;
  const float width = mFont != nullptr
    ? mFont->measureWidth(mLabel) + mPaddingX * 2.0f : 96.0f;
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + mPaddingY * 2.0f : 36.0f;
  setMeasuredSize({ width, height });
  return measuredSize();
}

void ToggleButton::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint fill;
  fill.color = mSelected ? mSelectedBackground : mBackground;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), fill);
  graphics::Paint border;
  // Focus belongs to the focus ring; the outline must not stay accent-coloured after an unselected
  // toggle keeps keyboard focus, or it reads as still selected.
  border.color = mBorder;
  border.style = graphics::Paint::Style::kStroke;
  border.strokeWidth = 1.0f;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), border);
  if (mFont != nullptr) {
    const float width = mFont->measureWidth(mLabel);
    detail::drawLabel(context, mLabel, bounds(), *mFont,
      mSelected ? mSelectedForeground : mForeground,
      bounds().getLeft() + (bounds().getWidth() - width) * 0.5f);
  }
  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, mRadius, focusRingWidth());
}

bool ToggleButton::onPointerDown(const PointerEvent&) {
  toggle();
  return true;
}

bool ToggleButton::onPointerMove(const PointerEvent&) {
  if (!mHovered) {
    mHovered = true;
    setNeedsPaint();
  }
  return true;
}

bool ToggleButton::onPointerLeave() {
  if (mHovered) {
    mHovered = false;
    setNeedsPaint();
  }
  return true;
}

bool ToggleButton::onKeyDown(const KeyEvent& event) {
  if (event.key == Key::Enter || event.key == Key::Space) {
    toggle();
    return true;
  }
  return false;
}

void ToggleButton::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

bool ToggleButton::performAccessibilityAction(const AccessibilityAction action, std::string_view) {
  if (action != AccessibilityAction::Invoke && action != AccessibilityAction::Toggle) return false;
  toggle();
  return true;
}

void ToggleButton::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mSelectedBackground = theme.controls.accent;
  mForeground = theme.controls.foreground;
  mSelectedForeground = Color::MakeRGBA8(255, 255, 255, 255);
  mBorder = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mRadius = theme.radius.md;
  setNeedsPaint();
}

void ToggleButton::toggle() {
  setSelected(!mSelected);
}

} // namespace coral::ui
