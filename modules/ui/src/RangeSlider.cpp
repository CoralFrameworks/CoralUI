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

#include <ui/RangeSlider.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace coral::ui {

RangeSlider::RangeSlider(const double minimum, const double maximum,
    const double low, const double high, const double step)
    : mMinimum(minimum), mMaximum(std::max(minimum + 1e-9, maximum)),
      mLow(std::clamp(low, minimum, std::max(minimum, maximum))),
      mHigh(std::clamp(high, mLow, std::max(minimum, maximum))),
      mStep(std::max(1e-9, step)) {
  setAccessibleRole(AccessibilityRole::Slider);
  setAccessibleName("Range");
  setAccessibleValue(std::to_string(mLow) + ", " + std::to_string(mHigh));
}

void RangeSlider::setRange(const double minimum, const double maximum) {
  mMinimum = minimum;
  mMaximum = std::max(minimum + 1e-9, maximum);
  mStep = std::min(mStep, (mMaximum - mMinimum) / 100.0);
  setLow(mLow);
  setHigh(mHigh);
}

void RangeSlider::setLow(const double low) {
  setHandle(Handle::Low, low, false);
}

void RangeSlider::setHigh(const double high) {
  setHandle(Handle::High, high, false);
}

void RangeSlider::setStep(const double step) {
  mStep = std::max(1e-9, step);
}

void RangeSlider::setColors(const Color track, const Color range, const Color handle,
    const Color focus) {
  mTrack = track;
  mRange = range;
  mHandle = handle;
  mFocus = focus;
  setNeedsPaint();
}

Size RangeSlider::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ 220.0f, 28.0f });
  return measuredSize();
}

void RangeSlider::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  const float left = bounds().getLeft() + 8.0f;
  const float right = bounds().getRight() - 8.0f;
  const float center = bounds().getTop() + bounds().getHeight() * 0.5f;
  const float lowX = xForValue(mLow);
  const float highX = xForValue(mHigh);
  graphics::Paint track;
  track.color = mTrack;
  track.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(
    Rect::MakeXYWH(left, center - 2.0f, std::max(0.0f, right - left), 4.0f), 2.0f), track);
  graphics::Paint range;
  range.color = mRange;
  range.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(
    Rect::MakeXYWH(lowX, center - 2.0f, std::max(0.0f, highX - lowX), 4.0f), 2.0f), range);
  for (const auto [value, active]: { std::pair{ mLow, mActive == Handle::Low },
           std::pair{ mHigh, mActive == Handle::High } }) {
    graphics::Paint handle;
    handle.color = mFocused && active ? mFocus : mHandle;
    handle.style = graphics::Paint::Style::kFill;
    canvas.drawCircle(xForValue(value), center, 6.0f, handle);
  }
  if (mFocused) {
    detail::drawFocusRing(canvas,
      Rect::MakeLTRB(bounds().getLeft() + 1.0f, bounds().getTop() + 1.0f,
        bounds().getRight() - 1.0f, bounds().getBottom() - 1.0f),
      true, mFocus, 7.0f, focusRingWidth());
  }
}

bool RangeSlider::onPointerDown(const PointerEvent& event) {
  const float lowDistance = std::abs(event.position.x - xForValue(mLow));
  const float highDistance = std::abs(event.position.x - xForValue(mHigh));
  mActive = lowDistance <= highDistance ? Handle::Low : Handle::High;
  mDragging = true;
  updateFromPoint(event.position);
  return true;
}

bool RangeSlider::onPointerMove(const PointerEvent& event) {
  if (mDragging) updateFromPoint(event.position);
  return true;
}

bool RangeSlider::onPointerUp(const PointerEvent& event) {
  if (mDragging) updateFromPoint(event.position);
  mDragging = false;
  return true;
}

bool RangeSlider::onKeyDown(const KeyEvent& event) {
  if (event.key == Key::Tab) {
    mActive = mActive == Handle::Low ? Handle::High : Handle::Low;
    setNeedsPaint();
    return true;
  }
  const double direction = event.key == Key::Left ? -1.0 : (event.key == Key::Right ? 1.0 : 0.0);
  if (direction != 0.0) {
    setHandle(mActive, (mActive == Handle::Low ? mLow : mHigh) + direction * mStep, true);
    return true;
  }
  if (event.key == Key::Home) {
    setHandle(mActive, mMinimum, true);
    return true;
  }
  if (event.key == Key::End) {
    setHandle(mActive, mMaximum, true);
    return true;
  }
  return false;
}

void RangeSlider::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

std::optional<AccessibleRange> RangeSlider::accessibleRange() const {
  return AccessibleRange{ mLow, mMinimum, mMaximum, mStep };
}

bool RangeSlider::performAccessibilityAction(const AccessibilityAction action,
    const std::string_view value) {
  if (action != AccessibilityAction::SetValue) return false;
  try {
    const std::string text(value);
    const auto comma = text.find(',');
    if (comma == std::string::npos) {
      setHandle(mActive, std::stod(text), true);
    } else {
      setLow(std::stod(text.substr(0, comma)));
      setHigh(std::stod(text.substr(comma + 1)));
      if (mOnChanged) mOnChanged(mLow, mHigh);
    }
  } catch (...) {
    return false;
  }
  return true;
}

void RangeSlider::applyTheme(const Theme& theme) {
  mTrack = theme.controls.track;
  mRange = theme.controls.accent;
  mHandle = theme.controls.foreground;
  mFocus = theme.controls.borderFocused;
  setNeedsPaint();
}

float RangeSlider::xForValue(const double value) const {
  const float left = bounds().getLeft() + 8.0f;
  const float width = std::max(0.0f, bounds().getWidth() - 16.0f);
  const double span = std::max(1e-9, mMaximum - mMinimum);
  return left + static_cast<float>((value - mMinimum) / span) * width;
}

double RangeSlider::valueForX(const float x) const {
  const float left = bounds().getLeft() + 8.0f;
  const float width = std::max(1.0f, bounds().getWidth() - 16.0f);
  const double raw = mMinimum + (static_cast<double>(x - left) / width) * (mMaximum - mMinimum);
  return std::clamp(std::round(raw / mStep) * mStep, mMinimum, mMaximum);
}

void RangeSlider::setHandle(const Handle handle, const double value, const bool notify) {
  const double next = std::clamp(std::round(value / mStep) * mStep, mMinimum, mMaximum);
  if (handle == Handle::Low) {
    mLow = std::clamp(next, mMinimum, mHigh);
  } else {
    mHigh = std::clamp(next, mLow, mMaximum);
  }
  setAccessibleValue(std::to_string(mLow) + ", " + std::to_string(mHigh));
  setNeedsPaint();
  if (notify && mOnChanged) mOnChanged(mLow, mHigh);
}

void RangeSlider::updateFromPoint(const Point point) {
  setHandle(mActive, valueForX(point.x), true);
}

} // namespace coral::ui
