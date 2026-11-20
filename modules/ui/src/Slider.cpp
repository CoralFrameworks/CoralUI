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
#include <ui/Slider.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>
#include <ui/Undo.h>

#include <algorithm>
#include <cstdio>
#include <utility>

namespace coral::ui {

Slider::Slider(std::string label, std::shared_ptr<const graphics::Font> font,
    const double minimum, const double maximum, const double value)
    : mLabel(std::move(label)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Slider);
  setAccessibleName(mLabel);
  setRange(minimum, maximum);
  setValue(value);
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.2f", mValue);
  setAccessibleValue(buffer);
  notifyAccessibilityEvent(*this, AccessibilityEvent::ValueChanged);
}

void Slider::setLabel(std::string label) {
  mLabel = std::move(label);
  setNeedsLayout();
  setNeedsPaint();
}

void Slider::setFont(std::shared_ptr<const graphics::Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void Slider::setColors(const graphics::Color background, const graphics::Color foreground,
    const graphics::Color accent) {
  mBackground = background;
  mForeground = foreground;
  mAccent = accent;
  setNeedsPaint();
}

void Slider::setRange(const double minimum, const double maximum) {
  mMinimum = minimum;
  mMaximum = std::max(minimum + 1e-9, maximum);
  mValue = std::clamp(mValue, mMinimum, mMaximum);
  setNeedsPaint();
}

void Slider::setValue(const double value) {
  const double clamped = std::clamp(value, mMinimum, mMaximum);
  if (clamped == mValue) {
    return;
  }
  mValue = clamped;
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.2f", mValue);
  setAccessibleValue(buffer);
  notifyAccessibilityEvent(*this, AccessibilityEvent::ValueChanged);
  setNeedsPaint();
}

Size Slider::measure(const Size& available) {
  (void)available;
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + 24.0f
    : 40.0f;
  setMeasuredSize({ 260.0f, std::max(36.0f, height) });
  return measuredSize();
}

void Slider::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  // A named style replaces the theme copy for the surface, track, fill and labels; the knob stays a
  // light token so the thumb remains visible against the fill.
  graphics::Color surface = mBackground;
  graphics::Color borderColor = mFocused ? mAccent : mBorder;
  graphics::Color trackColor = mTrack;
  graphics::Color fillColor = mAccent;
  graphics::Color labelColor = mForeground;
  graphics::Color valueColor = mAccent;
  float radius = mRadius;
  float borderWidth = 1.0f;
  if (const auto styled = resolvedStyle()) {
    surface = styled->background;
    borderColor = styled->border;
    trackColor = styled->background;
    fillColor = styled->border;
    labelColor = styled->foreground;
    valueColor = styled->border;
    radius = styled->radius;
    borderWidth = styled->borderWidth;
  }
  graphics::Paint background;
  background.color = surface;
  background.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), radius), background);

  graphics::Paint border;
  border.color = borderColor;
  border.style = graphics::Paint::Style::kStroke;
  border.strokeWidth = borderWidth;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), radius), border);

  const float textHeight = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent
    : 16.0f;
  if (mFont != nullptr) {
    detail::drawLabel(context, mLabel,
      Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), bounds().getWidth(), textHeight),
      *mFont, labelColor, bounds().getLeft() + mPadding);
    char value[64]{};
    std::snprintf(value, sizeof(value), mFormat.c_str(), mValue);
    const float valueWidth = mFont->measureWidth(value);
    detail::drawLabel(context, value,
      Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), bounds().getWidth(), textHeight),
      *mFont, valueColor, bounds().getRight() - mPadding - valueWidth);
  }

  const float trackLeft = bounds().getLeft() + mPadding;
  const float trackWidth = std::max(1.0f, bounds().getWidth() - mPadding * 2.0f);
  const float trackY = bounds().getBottom() - 13.0f;
  const float ratio = static_cast<float>((mValue - mMinimum) / (mMaximum - mMinimum));
  graphics::Paint track;
  track.color = trackColor;
  track.style = graphics::Paint::Style::kFill;
  const Rect trackRect = Rect::MakeXYWH(trackLeft, trackY - mTrackHeight * 0.5f, trackWidth,
    mTrackHeight);
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(trackRect, mTrackHeight * 0.5f), track);

  graphics::Paint filled;
  filled.color = fillColor;
  filled.style = graphics::Paint::Style::kFill;
  const Rect filledRect = Rect::MakeXYWH(trackLeft, trackY - mTrackHeight * 0.5f,
    trackWidth * ratio, mTrackHeight);
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(filledRect, mTrackHeight * 0.5f), filled);

  graphics::Paint knob;
  knob.color = mKnob;
  knob.style = graphics::Paint::Style::kFill;
  canvas.drawCircle(trackLeft + trackWidth * ratio, trackY, mKnobRadius, knob);
  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, mRadius, focusRingWidth());
}

bool Slider::onPointerDown(const PointerEvent& event) {
  if (!bounds().contains(event.position)) {
    return false;
  }
  if (mUndoStack != nullptr && !mUndoStack->inTransaction()) {
    mUndoStack->beginTransaction(mUndoLabel);
  }
  mDragging = true;
  if (mOnEditBegin) {
    mOnEditBegin();
  }
  updateFromPointer(event.position.x);
  return true;
}

bool Slider::onPointerMove(const PointerEvent& event) {
  if (!mDragging) {
    return false;
  }
  updateFromPointer(event.position.x);
  return true;
}

bool Slider::onPointerUp(const PointerEvent& event) {
  (void)event;
  if (!mDragging) {
    return false;
  }
  mDragging = false;
  if (mOnEditEnd) {
    mOnEditEnd();
  }
  if (mUndoStack != nullptr) {
    mUndoStack->commitTransaction();
  }
  return true;
}

bool Slider::onKeyDown(const KeyEvent& event) {
  const double step = mStep > 0.0 ? mStep : (mMaximum - mMinimum) / 100.0;
  switch (event.key) {
    case Key::Left:
    case Key::Down:
      nudge(-step);
      return true;
    case Key::Right:
    case Key::Up:
      nudge(step);
      return true;
    case Key::Home:
      setValue(mMinimum);
      notify();
      return true;
    case Key::End:
      setValue(mMaximum);
      notify();
      return true;
    default:
      return false;
  }
}

void Slider::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

void Slider::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mTrack = theme.controls.track;
  mKnob = theme.controls.knob;
  mFocus = theme.controls.borderFocused;
  mForeground = theme.controls.foreground;
  mAccent = theme.controls.accent;
  mPadding = theme.spacing.md;
  mRadius = theme.radius.md;
  mTrackHeight = 4.0f;
  mKnobRadius = 6.0f;
  setNeedsLayout();
  setNeedsPaint();
}

void Slider::updateFromPointer(const float x) {
  const float trackLeft = bounds().getLeft() + mPadding;
  const float trackWidth = std::max(1.0f, bounds().getWidth() - mPadding * 2.0f);
  const float ratio = std::clamp((x - trackLeft) / trackWidth, 0.0f, 1.0f);
  setValue(mMinimum + (mMaximum - mMinimum) * ratio);
  notify();
}

void Slider::nudge(const double delta) {
  setValue(mValue + delta);
  notify();
}

void Slider::notify() {
  if (mOnChanged) {
    mOnChanged(mValue);
  }
}


void Slider::setUndoStack(UndoStack* stack, std::string label) {
  mUndoStack = stack;
  mUndoLabel = std::move(label);
}

std::optional<AccessibleRange> Slider::accessibleRange() const {
  return AccessibleRange{ mValue, mMinimum, mMaximum, mStep > 0.0 ? mStep : (mMaximum - mMinimum) / 20.0 };
}

bool Slider::performAccessibilityAction(const AccessibilityAction action, const std::string_view value) {
  if (action != AccessibilityAction::SetValue) return false;
  const std::string text(value);
  char* end = nullptr;
  const double parsed = std::strtod(text.c_str(), &end);
  if (end == text.c_str() || !std::isfinite(parsed)) return false;
  setValue(parsed);
  return true;
}

} // namespace coral::ui