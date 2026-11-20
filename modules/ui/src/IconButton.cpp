/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ui/IconButton.h>

#include <ui/IconTheme.h>
#include <ui/RenderContext.h>
#include <ui/Theme.h>

#include "WidgetDrawing.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace coral::ui {

IconButton::IconButton(std::shared_ptr<const Image> icon, std::string accessibleName,
    const IconButtonSize size, const IconButtonVariant variant)
    : mIcon(std::move(icon)), mVariant(variant) {
  setAccessibleName(std::move(accessibleName));
  setAccessibleRole(AccessibilityRole::Button);
  setSize(size);
}

IconButton::~IconButton() = default;

void IconButton::setSize(const IconButtonSize size) {
  switch (size) {
    case IconButtonSize::Compact:
      mSide = 24.0f;
      mIconSize = 14.0f;
      mPadding = 4.0f;
      break;
    case IconButtonSize::Large:
      mSide = 40.0f;
      mIconSize = 20.0f;
      mPadding = 10.0f;
      break;
    case IconButtonSize::Regular:
    default:
      mSide = 32.0f;
      mIconSize = 16.0f;
      mPadding = 8.0f;
      break;
  }
  setNeedsLayout();
  setNeedsPaint();
}

void IconButton::setVariant(const IconButtonVariant variant) {
  mVariant = variant;
  setNeedsPaint();
}

void IconButton::setIconSize(const float size) {
  mIconSize = std::max(1.0f, size);
  setNeedsPaint();
}

void IconButton::setIcon(std::shared_ptr<const Image> icon) {
  mIcon = std::move(icon);
  setNeedsLayout();
  setNeedsPaint();
}

void IconButton::setIconTheme(std::shared_ptr<const IconTheme> theme, std::string name) {
  mIconTheme = std::move(theme);
  mIconName = std::move(name);
  setNeedsPaint();
}

void IconButton::setOnClick(std::function<void()> callback) {
  mOnClick = std::move(callback);
}

Size IconButton::measure(const Size& available) {
  const float side = mSide;
  const float width = available.width > 0.0f ? std::min(available.width, side) : side;
  const float height = available.height > 0.0f ? std::min(available.height, side) : side;
  setMeasuredSize({ width, height });
  return measuredSize();
}

void IconButton::drawSelf(RenderContext& context) {
  Canvas& canvas = context.canvas();
  const float radius = mRadius;
  const bool ghost = mVariant == IconButtonVariant::Ghost;
  const Color background = mPressed ? mPressedBackground : (mHovered ? mHoverBackground : mBackground);

  if (!ghost || mPressed || mHovered) {
    Paint paint;
    paint.color = background;
    paint.style = Paint::Style::kFill;
    canvas.drawRoundRect(RoundRect::MakeRectRadius(bounds(), radius), paint);
  }
  if (!ghost) {
    Paint border;
    border.color = mBorder;
    border.style = Paint::Style::kStroke;
    border.strokeWidth = 1.0f;
    canvas.drawRoundRect(RoundRect::MakeRectRadius(bounds(), radius), border);
  }
  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, radius, focusRingWidth());

  const Image* image = mIcon.get();
  if (image == nullptr && mIconTheme != nullptr) {
    image = mIconTheme->resolve(mIconName, mIconSize);
  }
  if (image == nullptr || image->width() <= 0 || image->height() <= 0) return;
  const float maxSide = std::max(1.0f, mIconSize);
  const float scale = std::min(maxSide / static_cast<float>(image->width()),
    maxSide / static_cast<float>(image->height()));
  const float width = static_cast<float>(image->width()) * scale;
  const float height = static_cast<float>(image->height()) * scale;
  Paint iconPaint;
  iconPaint.color = mForeground;
  iconPaint.style = Paint::Style::kFill;
  canvas.save();
  canvas.translate(bounds().getLeft() + (bounds().getWidth() - width) * 0.5f,
    bounds().getTop() + (bounds().getHeight() - height) * 0.5f);
  canvas.scale(scale);
  canvas.drawImage(*image, 0.0f, 0.0f, iconPaint);
  canvas.restore();
}

bool IconButton::onPointerDown(const PointerEvent& event) {
  if (event.button != PointerButton::Primary) return false;
  mPressed = true;
  // Hover is a pointer-position fact, not a side effect of pressing: setting it unconditionally here
  // left the button highlighted forever when the pointer never produced a leave event.
  mHovered = bounds().contains(event.position);
  setNeedsPaint();
  return true;
}

bool IconButton::onPointerMove(const PointerEvent& event) {
  const bool hovered = bounds().contains(event.position);
  if (hovered == mHovered) return false;
  mHovered = hovered;
  setNeedsPaint();
  return false;
}

bool IconButton::onPointerLeave() {
  if (!mHovered) return false;
  mHovered = false;
  setNeedsPaint();
  return false;
}

bool IconButton::onPointerUp(const PointerEvent& event) {
  if (event.button != PointerButton::Primary) return false;
  const bool activateNow = mPressed && bounds().contains(event.position);
  mPressed = false;
  setNeedsPaint();
  if (activateNow) activate();
  return true;
}

bool IconButton::onKeyDown(const KeyEvent& event) {
  if (event.key != Key::Enter && event.key != Key::Space) return false;
  activate();
  return true;
}

void IconButton::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

void IconButton::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mHoverBackground = theme.controls.backgroundHover;
  mPressedBackground = theme.controls.backgroundPressed;
  mBorder = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mForeground = theme.controls.foreground;
  mRadius = theme.radius.md;
  setNeedsPaint();
}

void IconButton::activate() {
  ++mActivations;
  if (mOnClick != nullptr) mOnClick();
}

bool IconButton::performAccessibilityAction(const AccessibilityAction action, const std::string_view value) {
  (void)value;
  if (action != AccessibilityAction::Invoke) return false;
  if (mOnClick) { mOnClick(); return true; }
  return false;
}

} // namespace coral::ui
