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

#include <ui/Meter.h>

#include <ui/Theme.h>

#include <algorithm>

namespace coral::ui {

Meter::Meter(const double minimum, const double maximum, const double value)
    : mMinimum(minimum), mMaximum(std::max(minimum + 1e-9, maximum)),
      mValue(std::clamp(value, minimum, std::max(minimum, maximum))) {
  setAccessibleRole(AccessibilityRole::ProgressIndicator);
  setAccessibleName("Meter");
}

void Meter::setRange(const double minimum, const double maximum) {
  mMinimum = minimum;
  mMaximum = std::max(minimum + 1e-9, maximum);
  setValue(mValue);
}

void Meter::setValue(const double value) {
  mValue = std::clamp(value, mMinimum, mMaximum);
  setNeedsPaint();
}

void Meter::setThresholds(const double low, const double high) {
  mLow = std::clamp(low, 0.0, 1.0);
  mHigh = std::clamp(std::max(low, high), 0.0, 1.0);
  setNeedsPaint();
}

void Meter::setColors(const Color normal, const Color warning, const Color danger,
    const Color track) {
  mNormal = normal;
  mWarning = warning;
  mDanger = danger;
  mTrack = track;
  setNeedsPaint();
}

void Meter::setThickness(const float thickness) {
  mThickness = std::max(2.0f, thickness);
  setNeedsLayout();
  setNeedsPaint();
}

Size Meter::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ 220.0f, mThickness });
  return measuredSize();
}

void Meter::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint track;
  track.color = mTrack;
  track.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(),
    mThickness * 0.5f), track);
  const double span = std::max(1e-9, mMaximum - mMinimum);
  const float fraction = static_cast<float>((mValue - mMinimum) / span);
  Color color = mNormal;
  if (fraction >= mHigh) color = mDanger;
  else if (fraction >= mLow) color = mWarning;
  graphics::Paint fill;
  fill.color = color;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(
    Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(),
      bounds().getWidth() * std::clamp(fraction, 0.0f, 1.0f), mThickness),
    mThickness * 0.5f), fill);
}

std::optional<AccessibleRange> Meter::accessibleRange() const {
  return AccessibleRange{ mValue, mMinimum, mMaximum, 0.0 };
}

void Meter::applyTheme(const Theme& theme) {
  mNormal = theme.controls.accent;
  mTrack = theme.controls.track;
  setNeedsPaint();
}

} // namespace coral::ui
