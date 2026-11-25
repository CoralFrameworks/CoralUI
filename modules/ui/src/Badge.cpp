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

#include <ui/Badge.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Badge::Badge(std::string text, std::shared_ptr<const Font> font)
    : mText(std::move(text)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Label);
  setAccessibleValue(mText);
}

void Badge::setText(std::string text) {
  mText = std::move(text);
  setAccessibleValue(mText);
  setNeedsLayout();
  setNeedsPaint();
}

void Badge::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void Badge::setIcon(std::shared_ptr<const graphics::Image> icon) {
  mIcon = std::move(icon);
  setNeedsLayout();
  setNeedsPaint();
}

void Badge::setDot(const bool dot) {
  mDot = dot;
  setNeedsLayout();
  setNeedsPaint();
}

void Badge::setColors(const Color background, const Color foreground) {
  mBackground = background;
  mForeground = foreground;
  setNeedsPaint();
}

Size Badge::measure(const Size& available) {
  (void)available;
  if (mDot) {
    setMeasuredSize({ 10.0f, 10.0f });
    return measuredSize();
  }
  const float textWidth = mFont != nullptr ? mFont->measureWidth(mText) : 24.0f;
  const float iconWidth = mIcon != nullptr ? 16.0f : 0.0f;
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + 6.0f : 18.0f;
  setMeasuredSize({ textWidth + iconWidth + mPadding * 2.0f, std::max(18.0f, height) });
  return measuredSize();
}

void Badge::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  if (mDot) {
    graphics::Paint dot;
    dot.color = mBackground;
    dot.style = graphics::Paint::Style::kFill;
    canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), 5.0f), dot);
    return;
  }
  const float radius = bounds().getHeight() * 0.5f;
  graphics::Paint fill;
  fill.color = mBackground;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), radius), fill);
  float contentLeft = bounds().getLeft() + mPadding;
  if (mIcon != nullptr && mIcon->width() > 0 && mIcon->height() > 0) {
    const float maxSide = 12.0f;
    const float scale = std::min(maxSide / static_cast<float>(mIcon->width()),
      maxSide / static_cast<float>(mIcon->height()));
    const float iconW = static_cast<float>(mIcon->width()) * scale;
    const float iconH = static_cast<float>(mIcon->height()) * scale;
    graphics::Paint paint;
    paint.color = mForeground;
    paint.style = graphics::Paint::Style::kFill;
    canvas.save();
    canvas.translate(contentLeft, bounds().getTop() + (bounds().getHeight() - iconH) * 0.5f);
    canvas.scale(scale);
    canvas.drawImage(*mIcon, 0.0f, 0.0f, paint);
    canvas.restore();
    contentLeft += 16.0f;
  }
  if (mFont != nullptr) {
    const float width = mFont->measureWidth(mText);
    detail::drawLabel(context, mText, bounds(), *mFont, mForeground,
      contentLeft + (bounds().getRight() - contentLeft - width) * 0.5f);
  }
}

void Badge::applyTheme(const Theme& theme) {
  mBackground = theme.controls.accent;
  mForeground = theme.surface;
  mPadding = theme.spacing.xs;
  setNeedsLayout();
  setNeedsPaint();
}

} // namespace coral::ui
