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

#include <ui/Accessibility.h>
#include <ui/Toggle.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Toggle::Toggle(std::string label, std::shared_ptr<const graphics::Font> font, const bool on)
    : mLabel(std::move(label)), mFont(std::move(font)), mOn(on) {
  setAccessibleRole(AccessibilityRole::CheckBox);
  setAccessibleName(mLabel);
  setAccessibleSelected(on);
  notifyAccessibilityEvent(*this, AccessibilityEvent::ToggleChanged);
}

void Toggle::setLabel(std::string label) {
  mLabel = std::move(label);
  setNeedsLayout();
  setNeedsPaint();
}

void Toggle::setFont(std::shared_ptr<const graphics::Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void Toggle::setColors(const graphics::Color background, const graphics::Color foreground,
    const graphics::Color accent) {
  mBackground = background;
  mForeground = foreground;
  mAccent = accent;
  setNeedsPaint();
}

void Toggle::setOn(const bool on) {
  setAccessibleSelected(on);
  notifyAccessibilityEvent(*this, AccessibilityEvent::ToggleChanged);
  if (mOn == on) {
    return;
  }
  mOn = on;
  setNeedsPaint();
}

Size Toggle::measure(const Size& available) {
  (void)available;
  const float textWidth = mFont != nullptr ? mFont->measureWidth(mLabel) : 80.0f;
  const float height = mFont != nullptr
    ? std::max(26.0f, mFont->metrics().ascent + mFont->metrics().descent + 8.0f)
    : 26.0f;
  setMeasuredSize({ 46.0f + textWidth + mGap, height });
  return measuredSize();
}

void Toggle::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  const float height = std::min(22.0f, bounds().getHeight() - 2.0f);
  const float width = height * 1.9f;
  const Rect track = Rect::MakeXYWH(bounds().getLeft() + 1.0f,
    bounds().getTop() + (bounds().getHeight() - height) * 0.5f, width, height);

  // A named style replaces the theme copy; the on-state keeps the accent track unless the variant
  // overrides the background explicitly.
  graphics::Color trackColor = mOn ? mAccent : mBackground;
  graphics::Color borderColor = mBorder;
  graphics::Color labelColor = mForeground;
  float radius = mRadius;
  float borderWidth = 1.0f;
  if (const auto styled = resolvedStyle()) {
    trackColor = mOn ? styled->border : styled->background;
    borderColor = styled->border;
    labelColor = styled->foreground;
    radius = styled->radius;
    borderWidth = styled->borderWidth;
  }
  graphics::Paint trackPaint;
  trackPaint.color = trackColor;
  trackPaint.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(track, radius), trackPaint);

  graphics::Paint trackBorder;
  trackBorder.color = borderColor;
  trackBorder.style = graphics::Paint::Style::kStroke;
  trackBorder.strokeWidth = borderWidth;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(track, radius), trackBorder);

  const float knobRadius = height * 0.36f;
  const float knobX = mOn ? track.getRight() - knobRadius - 3.0f : track.getLeft() + knobRadius + 3.0f;
  graphics::Paint knob;
  knob.color = mKnob;
  knob.style = graphics::Paint::Style::kFill;
  canvas.drawCircle(knobX, track.getTop() + height * 0.5f, knobRadius, knob);

  if (mFont != nullptr) {
    detail::drawLabel(context, mLabel, bounds(), *mFont, labelColor, track.getRight() + mGap);
  }
  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, mRadius, focusRingWidth());
}

bool Toggle::onPointerDown(const PointerEvent& event) {
  mPressed = bounds().contains(event.position);
  setNeedsPaint();
  return true;
}

bool Toggle::onPointerUp(const PointerEvent& event) {
  const bool activated = mPressed && bounds().contains(event.position);
  mPressed = false;
  setNeedsPaint();
  if (activated) {
    activate();
  }
  return true;
}

bool Toggle::onKeyDown(const KeyEvent& event) {
  if (!event.repeat && (event.key == Key::Space || event.key == Key::Enter)) {
    activate();
    return true;
  }
  return false;
}

void Toggle::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

void Toggle::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mKnob = theme.controls.knob;
  mForeground = theme.controls.foreground;
  mAccent = theme.controls.accent;
  mGap = theme.spacing.sm;
  mRadius = theme.radius.pill;
  setNeedsLayout();
  setNeedsPaint();
}

void Toggle::activate() {
  ++mActivations;
  mOn = !mOn;
  setNeedsPaint();
  if (mOnChanged) {
    mOnChanged(mOn);
  }
}

bool Toggle::performAccessibilityAction(const AccessibilityAction action, const std::string_view value) {
  (void)value;
  if (action != AccessibilityAction::Toggle) return false;
  setOn(!mOn);
  return true;
}

} // namespace coral::ui
