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

#include <ui/Kbd.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Kbd::Kbd(std::string label, std::shared_ptr<const Font> font)
    : mLabel(std::move(label)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Label);
  setAccessibleName(mLabel);
}

void Kbd::setLabel(std::string label) {
  mLabel = std::move(label);
  setAccessibleName(mLabel);
  setNeedsLayout();
  setNeedsPaint();
}

void Kbd::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

Size Kbd::measure(const Size& available) {
  (void)available;
  const float width = (mFont != nullptr ? mFont->measureWidth(mLabel) : 16.0f) +
    mPaddingX * 2.0f;
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + mPaddingY * 2.0f + 2.0f
    : 22.0f;
  setMeasuredSize({ std::max(18.0f, width), std::max(22.0f, height) });
  return measuredSize();
}

void Kbd::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint shadow;
  shadow.color = Color::MakeRGBA8(0, 0, 0, 70);
  shadow.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(
    Rect::MakeXYWH(bounds().getLeft(), bounds().getTop() + 1.0f,
      bounds().getWidth(), bounds().getHeight()), mRadius), shadow);
  graphics::Paint fill;
  fill.color = mBackground;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), fill);
  graphics::Paint border;
  border.color = mBorder;
  border.style = graphics::Paint::Style::kStroke;
  border.strokeWidth = 1.0f;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), border);
  if (mFont != nullptr) {
    const float width = mFont->measureWidth(mLabel);
    detail::drawLabel(context, mLabel, bounds(), *mFont, mForeground,
      bounds().getLeft() + (bounds().getWidth() - width) * 0.5f);
  }
}

void Kbd::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mForeground = theme.controls.foreground;
  mRadius = theme.radius.sm;
  setNeedsPaint();
}

} // namespace coral::ui
