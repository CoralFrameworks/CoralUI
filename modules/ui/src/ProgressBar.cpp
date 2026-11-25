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

#include <ui/ProgressBar.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace coral::ui {

ProgressBar::ProgressBar(std::shared_ptr<const graphics::Font> font, const double value)
    : mFont(std::move(font)), mValue(std::clamp(value, 0.0, 1.0)) {
  setAccessibleRole(AccessibilityRole::ProgressIndicator);
}

void ProgressBar::setFont(std::shared_ptr<const graphics::Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void ProgressBar::setColors(const graphics::Color background, const graphics::Color foreground,
    const graphics::Color accent) {
  mBackground = background;
  mForeground = foreground;
  mAccent = accent;
  setNeedsPaint();
}

void ProgressBar::setIndeterminate(const bool indeterminate) {
  mIndeterminate = indeterminate;
  mLastFrame = {};
  setNeedsPaint();
}

void ProgressBar::setValue(const double value) {
  const double clamped = std::clamp(value, 0.0, 1.0);
  if (clamped == mValue) {
    return;
  }
  mValue = clamped;
  setNeedsPaint();
}

Size ProgressBar::measure(const Size& available) {
  (void)available;
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + 8.0f
    : 20.0f;
  setMeasuredSize({ 220.0f, std::max(16.0f, height) });
  return measuredSize();
}

void ProgressBar::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mForeground = theme.controls.foreground;
  mAccent = theme.controls.accent;
  mRadius = theme.radius.md;
  setNeedsPaint();
}

std::optional<AccessibleRange> ProgressBar::accessibleRange() const {
  return AccessibleRange{ mValue, 0.0, 1.0, 0.0 };
}

void ProgressBar::onFrame(const std::chrono::steady_clock::time_point now) {
  Layer::onFrame(now);
  if (!mIndeterminate || !visible()) return;
  if (mLastFrame == std::chrono::steady_clock::time_point{}) {
    mLastFrame = now;
    return;
  }
  const double elapsed = std::chrono::duration<double>(now - mLastFrame).count();
  mLastFrame = now;
  mPhase = std::fmod(mPhase + elapsed * 0.6, 1.0);
  setNeedsPaint();
}

void ProgressBar::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint background;
  background.color = mBackground;
  background.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), background);

  if (mIndeterminate) {
    const float band = std::max(mRadius * 2.0f, bounds().getWidth() * 0.32f);
    const float x = bounds().getLeft() + (bounds().getWidth() - band) *
      static_cast<float>(mPhase);
    graphics::Paint fill;
    fill.color = mAccent;
    fill.style = graphics::Paint::Style::kFill;
    canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(
      Rect::MakeXYWH(x, bounds().getTop(), band, bounds().getHeight()), mRadius), fill);
  } else {
    const float ratio = static_cast<float>(mValue);
    if (ratio > 0.0f) {
      graphics::Paint fill;
      fill.color = mAccent;
      fill.style = graphics::Paint::Style::kFill;
      canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(
        Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), bounds().getWidth() * ratio,
          bounds().getHeight()), mRadius), fill);
    }
  }

  graphics::Paint border;
  border.color = mBorder;
  border.style = graphics::Paint::Style::kStroke;
  border.strokeWidth = 1.0f;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), border);

  if (mShowPercentage && !mIndeterminate && mFont != nullptr) {
    char text[32]{};
    std::snprintf(text, sizeof(text), "%d%%", static_cast<int>(mValue * 100.0 + 0.5));
    const float width = mFont->measureWidth(text);
    detail::drawLabel(context, text, bounds(), *mFont, mForeground,
      bounds().getLeft() + (bounds().getWidth() - width) * 0.5f);
  }
}

} // namespace coral::ui
