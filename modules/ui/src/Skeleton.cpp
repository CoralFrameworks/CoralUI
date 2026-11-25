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

#include <ui/Skeleton.h>

#include <ui/Theme.h>

#include <algorithm>
#include <cmath>

namespace coral::ui {

Skeleton::Skeleton(const float width, const float height)
    : mWidth(std::max(1.0f, width)), mHeight(std::max(1.0f, height)) {}

void Skeleton::setSize(const float width, const float height) {
  mWidth = std::max(1.0f, width);
  mHeight = std::max(1.0f, height);
  setNeedsLayout();
  setNeedsPaint();
}

void Skeleton::setColors(const Color color) {
  mColor = color;
  setNeedsPaint();
}

void Skeleton::setLines(const int lines) {
  mLines = std::clamp(lines, 1, 8);
  setNeedsLayout();
  setNeedsPaint();
}

void Skeleton::setLineGap(const float gap) {
  mLineGap = std::max(0.0f, gap);
  setNeedsLayout();
  setNeedsPaint();
}

void Skeleton::setShape(const Shape shape) {
  mShape = shape;
  setNeedsPaint();
}

void Skeleton::setAnimating(const bool animating) {
  mAnimating = animating;
  setOpacity(1.0f);
  setNeedsPaint();
}

Size Skeleton::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ mWidth, mHeight * static_cast<float>(mLines) +
    mLineGap * static_cast<float>(std::max(0, mLines - 1)) });
  return measuredSize();
}

void Skeleton::drawSelf(RenderContext& context) {
  graphics::Paint paint;
  paint.color = mColor;
  paint.style = graphics::Paint::Style::kFill;
  for (int line = 0; line < mLines; ++line) {
    const float top = bounds().getTop() + static_cast<float>(line) * (mHeight + mLineGap);
    const float lineWidth = line == mLines - 1 && mLines > 1
      ? bounds().getWidth() * 0.62f : bounds().getWidth();
    const Rect lineBounds = Rect::MakeXYWH(bounds().getLeft(), top, lineWidth, mHeight);
    const float radius = mShape == Shape::Circle
      ? std::min(lineBounds.getWidth(), lineBounds.getHeight()) * 0.5f
      : lineBounds.getHeight() * 0.5f;
    context.canvas().drawRoundRect(
      graphics::RoundRect::MakeRectRadius(lineBounds, radius), paint);
  }
}

void Skeleton::onFrame(const std::chrono::steady_clock::time_point now) {
  Layer::onFrame(now);
  if (!mAnimating || !visible()) return;
  mPhase = std::fmod(mPhase + 0.08f, 6.2831853f);
  setOpacity(0.55f + 0.25f * (0.5f + 0.5f * std::sin(mPhase)));
}

void Skeleton::applyTheme(const Theme& theme) {
  mColor = theme.controls.track;
}

} // namespace coral::ui
