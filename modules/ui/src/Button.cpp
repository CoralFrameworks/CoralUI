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

#include <ui/Button.h>

#include "TextPainter.h"
#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <graphics/Paint.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Button::Button(std::string label, std::shared_ptr<const graphics::Font> font)
    : mLabel(std::move(label)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Button);
  setAccessibleName(mLabel);
}

Button::~Button() = default;

void Button::setLabel(std::string label) {
  if (accessibleName() == mLabel) setAccessibleName(label);
  mLabel = std::move(label);
  setNeedsLayout();
  setNeedsPaint();
}

void Button::setFont(std::shared_ptr<const graphics::Font> font) {
  if (mFont == font) return;
  mFont = std::move(font);
  markFontChanged();
  setNeedsLayout();
  setNeedsPaint();
}

void Button::setColors(const graphics::Color background, const graphics::Color foreground) {
  mBackground = background;
  mForeground = foreground;
  setNeedsPaint();
}

void Button::setOnClick(std::function<void()> callback) {
  mOnClick = std::move(callback);
}

void Button::onEnabledChanged(const bool enabled) {
  if (!enabled) mPressed = false;
  setNeedsPaint();
}
Size Button::measure(const Size& available) {
  (void)available;
  const float width = mFont != nullptr
    ? mFont->measureWidth(mLabel) + mPaddingX * 2.0f
    : 96.0f;
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + mPaddingY * 2.0f
    : 36.0f;
  setMeasuredSize({ width, height });
  return measuredSize();
}

void Button::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  // A named style replaces the theme copy for every state; without one the control keeps its own
  // (slightly stronger) pressed colour so existing pages do not change appearance.
  Color background = mPressed ? mPressedBackground : mBackground;
  Color border = mFocused ? mFocus : mBorder;
  Color foreground = mForeground;
  Color focusColor = mFocus;
  float radius = mRadius;
  if (const auto styled = resolvedStyle()) {
    background = styled->background;
    border = styled->border;
    foreground = styled->foreground;
    focusColor = styled->border;
    radius = styled->radius;
  }
  graphics::Paint paint;
  paint.color = background;
  paint.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), radius), paint);

  graphics::Paint borderPaint;
  borderPaint.color = border;
  borderPaint.style = graphics::Paint::Style::kStroke;
  borderPaint.strokeWidth = 1.0f;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), radius), borderPaint);
  detail::drawFocusRing(canvas, bounds(), mFocused, focusColor, radius, focusRingWidth());
  if (mFont != nullptr) {
    graphics::Paint text;
    text.color = enabled() ? foreground : Color::MakeRGBA8(120, 125, 135, 255);
    text.style = graphics::Paint::Style::kFill;
    const float width = mFont->measureWidth(mLabel);
    const float baseline = bounds().getTop() +
      (bounds().getHeight() + mFont->metrics().ascent - mFont->metrics().descent) * 0.5f;
    if (mTextPainter == nullptr) mTextPainter = std::make_unique<TextPainter>();
    mTextPainter->draw(context, mLabel, bounds().getLeft() + (bounds().getWidth() - width) * 0.5f,
      baseline, *mFont, text);
  }
}

bool Button::onPointerDown(const PointerEvent& event) {
  if (!enabled()) return false;
  mPressed = bounds().contains(event.position);
  setNeedsPaint();
  return true;
}

bool Button::onPointerMove(const PointerEvent& event) {
  const bool hover = enabled() && bounds().contains(event.position);
  if (hover != mHover) {
    mHover = hover;
    setNeedsPaint();
  }
  return hover || mPressed;
}

bool Button::onPointerLeave() {
  if (!mHover) return false;
  mHover = false;
  setNeedsPaint();
  return true;
}

StateMask Button::styleStates() const {
  StateMask mask = Layer::styleStates();
  if (mHover) mask |= stateFlag(ControlState::Hovered);
  if (mPressed) mask |= stateFlag(ControlState::Pressed);
  if (mFocused) mask |= stateFlag(ControlState::Focused);
  return mask;
}

bool Button::onPointerUp(const PointerEvent& event) {
  if (!enabled()) {
    mPressed = false;
    setNeedsPaint();
    return false;
  }
  const bool activated = mPressed && bounds().contains(event.position);
  mPressed = false;
  setNeedsPaint();
  if (activated) {
    activate();
  }
  return true;
}

bool Button::onKeyDown(const KeyEvent& event) {
  if (!enabled()) return false;
  if (event.key == Key::Space || event.key == Key::Enter) {
    activate();
    return true;
  }
  return false;
}

void Button::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

void Button::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mPressedBackground = theme.controls.backgroundPressed;
  mBorder = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mForeground = theme.controls.foreground;
  mPaddingX = theme.spacing.md;
  mPaddingY = theme.spacing.sm;
  mRadius = theme.radius.md;
  setNeedsLayout();
  setNeedsPaint();
}

void Button::activate() {
  ++mActivations;
  if (mOnClick) {
    mOnClick();
  }
}

bool Button::performAccessibilityAction(const AccessibilityAction action, const std::string_view value) {
  (void)value;
  if (action != AccessibilityAction::Invoke) return false;
  if (mOnClick) { mOnClick(); return true; }
  return false;
}

} // namespace coral::ui
