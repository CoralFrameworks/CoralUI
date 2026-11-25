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

#include <ui/Spinner.h>

#include <algorithm>
#include <cmath>

namespace coral::ui {

namespace {
constexpr float kTurnMilliseconds = 900.0f;
constexpr float kTwoPi = 6.28318530718f;
} // namespace

Spinner::Spinner() {
  setAccessibleRole(AccessibilityRole::ProgressIndicator);
  setAccessibleValue("Loading");
}

void Spinner::setColor(const Color color) {
  mColor = color;
  setNeedsPaint();
}

void Spinner::setDotCount(const int count) {
  mDotCount = std::clamp(count, 3, 24);
  setNeedsPaint();
}

void Spinner::setSpinning(const bool spinning) {
  if (mSpinning == spinning) return;
  mSpinning = spinning;
  mLastFrame = {};
  setNeedsPaint();
}

void Spinner::advance(const std::chrono::milliseconds delta) {
  if (!mSpinning || delta.count() <= 0) return;
  const float turns = static_cast<float>(delta.count()) / kTurnMilliseconds;
  mPhase = std::fmod(mPhase + turns * kTwoPi, kTwoPi);
  setNeedsPaint();
}

Size Spinner::measure(const Size& available) {
  const float width = bounds().getWidth() > 0.0f ? bounds().getWidth() : available.width;
  const float height = bounds().getHeight() > 0.0f ? bounds().getHeight() : available.height;
  const float size = std::max(8.0f, std::min(width > 0.0f ? width : 20.0f,
    height > 0.0f ? height : 20.0f));
  setMeasuredSize({ size, size });
  return measuredSize();
}

void Spinner::onFrame(const std::chrono::steady_clock::time_point now) {
  if (!mSpinning) {
    mLastFrame = {};
    return;
  }
  if (mLastFrame.time_since_epoch().count() != 0) {
    advance(std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastFrame));
  }
  mLastFrame = now;
}

void Spinner::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  const float size = std::min(bounds().getWidth(), bounds().getHeight());
  if (size <= 0.0f) return;
  const float centreX = bounds().getLeft() + bounds().getWidth() * 0.5f;
  const float centreY = bounds().getTop() + bounds().getHeight() * 0.5f;
  const float ring = size * 0.5f - 2.0f;
  const float dot = std::max(1.2f, ring * 0.24f);
  for (int index = 0; index < mDotCount; ++index) {
    const float angle = mPhase + kTwoPi * static_cast<float>(index) / static_cast<float>(mDotCount);
    // The dot at the phase is the brightest and the tail fades out, which reads as rotation.
    const float weight = static_cast<float>(index) / static_cast<float>(mDotCount);
    graphics::Paint paint;
    paint.style = graphics::Paint::Style::kFill;
    paint.color = Color::MakeRGBA8(
      static_cast<uint8_t>(mColor.red * 255.0f),
      static_cast<uint8_t>(mColor.green * 255.0f),
      static_cast<uint8_t>(mColor.blue * 255.0f),
      static_cast<uint8_t>(255.0f * (0.12f + 0.88f * weight)));
    canvas.drawCircle(centreX + std::sin(angle) * ring, centreY - std::cos(angle) * ring, dot,
      paint);
  }
}


void Spinner::applyTheme(const Theme& theme) {
  mColor = theme.accent;
  setNeedsPaint();
}
} // namespace coral::ui
