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

#include <ui/ProgressRing.h>

#include <ui/Theme.h>

#include <algorithm>
#include <cmath>

namespace coral::ui {

namespace {
constexpr int kSegments = 48;
constexpr float kTwoPi = 6.2831853f;
}

ProgressRing::ProgressRing(const float size, const double value)
    : mSide(std::max(12.0f, size)), mValue(std::clamp(value, 0.0, 1.0)) {
  setAccessibleRole(AccessibilityRole::ProgressIndicator);
}

void ProgressRing::setSize(const float size) {
  mSide = std::max(12.0f, size);
  setNeedsLayout();
  setNeedsPaint();
}

void ProgressRing::setValue(const double value) {
  mValue = std::clamp(value, 0.0, 1.0);
  setNeedsPaint();
}

void ProgressRing::setThickness(const float thickness) {
  mThickness = std::max(1.0f, thickness);
  setNeedsPaint();
}

void ProgressRing::setIndeterminate(const bool indeterminate) {
  mIndeterminate = indeterminate;
  setAccessibleValue(mIndeterminate ? "Loading" : std::string());
  setNeedsPaint();
}

void ProgressRing::setColors(const Color track, const Color progress) {
  mTrack = track;
  mProgress = progress;
  setNeedsPaint();
}

Size ProgressRing::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ mSide, mSide });
  return measuredSize();
}

void ProgressRing::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  const float cx = bounds().getLeft() + bounds().getWidth() * 0.5f;
  const float cy = bounds().getTop() + bounds().getHeight() * 0.5f;
  const float radius = std::min(bounds().getWidth(), bounds().getHeight()) * 0.5f -
    mThickness * 0.5f;
  if (radius <= 0.0f) return;
  const int progressSegments = mIndeterminate
    ? 0 : static_cast<int>(std::round(mValue * kSegments));
  const int sweepSegments = kSegments / 4;
  const int sweepStart = mIndeterminate
    ? static_cast<int>(mPhase * static_cast<float>(kSegments)) % kSegments : 0;
  for (int index = 0; index < kSegments; ++index) {
    const bool inSweep = mIndeterminate &&
      (index - sweepStart + kSegments) % kSegments < sweepSegments;
    const bool active = mIndeterminate ? inSweep : index < progressSegments;
    const float from = -1.5707963f + kTwoPi * static_cast<float>(index) /
      static_cast<float>(kSegments);
    const float to = -1.5707963f + kTwoPi * static_cast<float>(index + 1) /
      static_cast<float>(kSegments);
    graphics::Paint paint;
    paint.color = active ? mProgress : mTrack;
    paint.style = graphics::Paint::Style::kStroke;
    paint.strokeWidth = mThickness;
    canvas.drawLine({ cx + std::cos(from) * radius, cy + std::sin(from) * radius },
      { cx + std::cos(to) * radius, cy + std::sin(to) * radius }, paint);
  }
}

std::optional<AccessibleRange> ProgressRing::accessibleRange() const {
  return AccessibleRange{ mValue, 0.0, 1.0, 0.0 };
}

void ProgressRing::onFrame(const std::chrono::steady_clock::time_point now) {
  Layer::onFrame(now);
  if (!mIndeterminate || !visible()) return;
  mPhase = std::fmod(mPhase + 0.03f, 1.0f);
  setNeedsPaint();
}

void ProgressRing::applyTheme(const Theme& theme) {
  mTrack = theme.controls.track;
  mProgress = theme.controls.accent;
  setNeedsPaint();
}

} // namespace coral::ui
