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

#include <ui/Avatar.h>

#include "WidgetDrawing.h"

#include <graphics/Path.h>
#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Avatar::Avatar(std::string initials, std::shared_ptr<const Font> font, const float size)
    : mInitials(std::move(initials)), mFont(std::move(font)),
      mSide(std::max(16.0f, size)) {
  setAccessibleRole(AccessibilityRole::Label);
  setAccessibleValue(mInitials);
}

void Avatar::setInitials(std::string initials) {
  mInitials = std::move(initials);
  setAccessibleValue(mInitials);
  setNeedsPaint();
}

void Avatar::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsPaint();
}

void Avatar::setImage(std::shared_ptr<const graphics::Image> image) {
  mImage = std::move(image);
  setNeedsPaint();
}

void Avatar::setSize(const float size) {
  mSide = std::max(16.0f, size);
  setNeedsLayout();
  setNeedsPaint();
}

void Avatar::setStatusVisible(const bool visible) {
  mStatusVisible = visible;
  setNeedsPaint();
}

void Avatar::setStatusColor(const Color color) {
  mStatusColor = color;
  setNeedsPaint();
}

void Avatar::setColors(const Color background, const Color foreground) {
  mBackground = background;
  mForeground = foreground;
  setNeedsPaint();
}

Size Avatar::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ mSide, mSide });
  return measuredSize();
}

void Avatar::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  const float radius = bounds().getWidth() * 0.5f;
  graphics::Paint fill;
  fill.color = mBackground;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), radius), fill);

  const bool hasImage = mImage != nullptr && mImage->width() > 0 && mImage->height() > 0;
  if (hasImage) {
    const float scale = std::min(bounds().getWidth() / static_cast<float>(mImage->width()),
      bounds().getHeight() / static_cast<float>(mImage->height()));
    const float drawWidth = static_cast<float>(mImage->width()) * scale;
    const float drawHeight = static_cast<float>(mImage->height()) * scale;
    const float left = bounds().getLeft() + (bounds().getWidth() - drawWidth) * 0.5f;
    const float top = bounds().getTop() + (bounds().getHeight() - drawHeight) * 0.5f;
    const float cx = bounds().getLeft() + bounds().getWidth() * 0.5f;
    const float cy = bounds().getTop() + bounds().getHeight() * 0.5f;
    const float r = std::min(bounds().getWidth(), bounds().getHeight()) * 0.5f;
    constexpr float k = 0.5522848f;
    graphics::Path circle;
    circle.moveTo({ cx + r, cy });
    circle.cubicTo({ cx + r, cy + r * k }, { cx + r * k, cy + r }, { cx, cy + r });
    circle.cubicTo({ cx - r * k, cy + r }, { cx - r, cy + r * k }, { cx - r, cy });
    circle.cubicTo({ cx - r, cy - r * k }, { cx - r * k, cy - r }, { cx, cy - r });
    circle.cubicTo({ cx + r * k, cy - r }, { cx + r, cy - r * k }, { cx + r, cy });
    circle.close();
    graphics::Paint paint;
    paint.color = graphics::Color::White();
    paint.style = graphics::Paint::Style::kFill;
    canvas.save();
    canvas.clipPath(circle);
    canvas.translate(left, top);
    canvas.scale(scale);
    canvas.drawImage(*mImage, 0.0f, 0.0f, paint);
    canvas.restore();
  } else if (mFont != nullptr) {
    const float width = mFont->measureWidth(mInitials);
    detail::drawLabel(context, mInitials, bounds(), *mFont, mForeground,
      bounds().getLeft() + (bounds().getWidth() - width) * 0.5f);
  }
  if (mStatusVisible) {
    const float dot = std::max(7.0f, bounds().getWidth() * 0.24f);
    const Rect dotBounds = Rect::MakeXYWH(bounds().getRight() - dot - 1.0f,
      bounds().getBottom() - dot - 1.0f, dot, dot);
    graphics::Paint ring;
    ring.color = mBackground;
    ring.style = graphics::Paint::Style::kFill;
    canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(
      Rect::MakeXYWH(dotBounds.getLeft() - 1.5f, dotBounds.getTop() - 1.5f,
        dot + 3.0f, dot + 3.0f), dot * 0.5f + 1.5f), ring);
    graphics::Paint status;
    status.color = mStatusColor;
    status.style = graphics::Paint::Style::kFill;
    canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(dotBounds, dot * 0.5f), status);
  }
}

void Avatar::applyTheme(const Theme& theme) {
  mBackground = theme.controls.accent;
  mForeground = theme.surface;
  setNeedsPaint();
}

} // namespace coral::ui
