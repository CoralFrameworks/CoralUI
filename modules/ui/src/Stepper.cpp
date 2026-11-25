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

#include <ui/Stepper.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>

namespace coral::ui {

Stepper::Stepper(const double minimum, const double maximum, const double value) {
  setAccessibleRole(AccessibilityRole::SpinButton);
  setRange(minimum, maximum);
  setValue(value);
}

void Stepper::setRange(const double minimum, const double maximum) {
  mMinimum = minimum;
  mMaximum = std::max(minimum + 1e-9, maximum);
  mValue = std::clamp(mValue, mMinimum, mMaximum);
  setNeedsPaint();
}

void Stepper::setValue(const double value) {
  mValue = std::clamp(value, mMinimum, mMaximum);
  setNeedsPaint();
}

void Stepper::setColors(const Color background, const Color border, const Color foreground,
    const Color accent) {
  mBackground = background;
  mBorder = border;
  mForeground = foreground;
  mFocus = accent;
  setNeedsPaint();
}

Size Stepper::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ 96.0f, 30.0f });
  return measuredSize();
}

void Stepper::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Color background = mBackground;
  graphics::Color border = mBorder;
  graphics::Color foreground = inputEnabled() ? mForeground : mMuted;
  float radius = mRadius;
  float borderWidth = 1.0f;
  if (const auto styled = resolvedStyle()) {
    background = styled->background;
    border = styled->border;
    foreground = styled->foreground;
    radius = styled->radius;
    borderWidth = styled->borderWidth;
  }

  graphics::Paint fill;
  fill.color = background;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), radius), fill);
  graphics::Paint stroke;
  stroke.color = border;
  stroke.style = graphics::Paint::Style::kStroke;
  stroke.strokeWidth = borderWidth;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), radius), stroke);

  const float mid = bounds().getLeft() + bounds().getWidth() * 0.5f;
  graphics::Paint divider = stroke;
  divider.strokeWidth = 1.0f;
  canvas.drawLine({ mid, bounds().getTop() + 4.0f },
    { mid, bounds().getBottom() - 4.0f }, divider);

  const float centerY = bounds().getTop() + bounds().getHeight() * 0.5f;
  graphics::Paint mark;
  mark.color = foreground;
  mark.style = graphics::Paint::Style::kStroke;
  mark.strokeWidth = 1.75f;
  const float minusX = bounds().getLeft() + bounds().getWidth() * 0.25f;
  const float plusX = bounds().getLeft() + bounds().getWidth() * 0.75f;
  canvas.drawLine({ minusX - 5.0f, centerY }, { minusX + 5.0f, centerY }, mark);
  canvas.drawLine({ plusX - 5.0f, centerY }, { plusX + 5.0f, centerY }, mark);
  canvas.drawLine({ plusX, centerY - 5.0f }, { plusX, centerY + 5.0f }, mark);

  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, radius, focusRingWidth());
}

bool Stepper::onPointerDown(const PointerEvent& event) {
  if (!inputEnabled()) return true;
  const float mid = bounds().getLeft() + bounds().getWidth() * 0.5f;
  nudge(event.position.x < mid ? -mStep : mStep);
  return true;
}

bool Stepper::onKeyDown(const KeyEvent& event) {
  if (!inputEnabled()) return false;
  switch (event.key) {
    case Key::Up:
    case Key::Right:
      nudge(mStep);
      return true;
    case Key::Down:
    case Key::Left:
      nudge(-mStep);
      return true;
    case Key::PageUp:
      nudge(mStep * 10.0);
      return true;
    case Key::PageDown:
      nudge(-mStep * 10.0);
      return true;
    default:
      return false;
  }
}

void Stepper::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

std::optional<AccessibleRange> Stepper::accessibleRange() const {
  return AccessibleRange{ mValue, mMinimum, mMaximum, mStep > 0.0 ? mStep : 1.0 };
}

bool Stepper::performAccessibilityAction(const AccessibilityAction action,
    const std::string_view value) {
  if (action != AccessibilityAction::SetValue) return false;
  const std::string text(value);
  char* end = nullptr;
  const double parsed = std::strtod(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0' || !std::isfinite(parsed)) return false;
  const double previous = mValue;
  setValue(parsed);
  if (mValue != previous && mOnChanged) mOnChanged(mValue);
  return true;
}

void Stepper::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mForeground = theme.controls.foreground;
  mFocus = theme.controls.borderFocused;
  mMuted = theme.controls.border;
  mRadius = theme.radius.md;
  setNeedsPaint();
}

void Stepper::nudge(const double delta) {
  const double previous = mValue;
  setValue(mValue + delta);
  if (mValue != previous && mOnChanged) mOnChanged(mValue);
}

} // namespace coral::ui
