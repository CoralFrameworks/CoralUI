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

#ifndef CORAL_UI_WIDGET_DRAWING_H
#define CORAL_UI_WIDGET_DRAWING_H

#include <ui/RenderContext.h>
#include <ui/Theme.h>

#include <graphics/Canvas.h>
#include <graphics/Color.h>
#include <graphics/Font.h>
#include <graphics/Paint.h>

namespace coral::ui::detail {

/** The baseline that vertically centres one line of @p font in @p bounds. */
inline float textBaseline(const Rect& bounds, const graphics::Font& font) {
  const auto metrics = font.metrics();
  return bounds.getTop() +
    (bounds.getHeight() + metrics.ascent - metrics.descent) * 0.5f;
}

inline void drawFocusRing(Canvas& canvas, const Rect& bounds, const bool focused,
    const graphics::Color& color, const float radius, const float width) {  if (!focused) {
    return;
  }
  graphics::Paint ring;
  ring.color = color;
  ring.style = graphics::Paint::Style::kStroke;
  ring.strokeWidth = width;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds, radius), ring);
}

inline void drawFocusRing(Canvas& canvas, const Rect& bounds, const bool focused,
    const graphics::Color& color, const float radius) {
  drawFocusRing(canvas, bounds, focused, color, radius, 1.5f);
}

inline void drawFocusRing(Canvas& canvas, const Rect& bounds, const bool focused,
    const graphics::Color& color) {
  drawFocusRing(canvas, bounds, focused, color, 2.0f, 1.5f);
}

inline void drawFocusRing(Canvas& canvas, const Rect& bounds, const bool focused) {
  drawFocusRing(canvas, bounds, focused, graphics::Color::MakeRGBA8(86, 156, 255, 255));
}

/** Approximates a soft shadow with concentric rounded rectangles. */
inline void drawShadow(Canvas& canvas, const Rect& bounds, float radius, const Shadow& shadow) {
  constexpr int kLayers = 6;
  for (int layer = 0; layer < kLayers; ++layer) {
    const float t = static_cast<float>(layer + 1) / static_cast<float>(kLayers);
    const float grow = shadow.blurRadius * t;
    Color color = shadow.color;
    color.alpha *= (1.0f - t) * 0.24f;
    Paint paint;
    paint.color = color;
    paint.style = Paint::Style::kFill;
    const Rect expanded = Rect::MakeLTRB(bounds.getLeft() - grow + shadow.offsetX,
      bounds.getTop() - grow + shadow.offsetY, bounds.getRight() + grow + shadow.offsetX,
      bounds.getBottom() + grow + shadow.offsetY);
    canvas.drawRoundRect(RoundRect::MakeRectRadius(expanded, radius + grow), paint);
  }
}

inline void drawLabel(RenderContext& context, const std::string& text, const Rect& bounds,
    const graphics::Font& font, const graphics::Color& color, const float left) {
  graphics::Paint paint;
  paint.color = color;
  paint.style = graphics::Paint::Style::kFill;
  context.drawText(text, left, textBaseline(bounds, font), font, paint);
}

inline void drawCheckMark(Canvas& canvas, const Rect& box,
    const graphics::Color& color) {
  graphics::Paint stroke;
  stroke.color = color;
  stroke.style = graphics::Paint::Style::kStroke;
  stroke.strokeWidth = 2.0f;
  const float left = box.getLeft();
  const float top = box.getTop();
  const float width = box.getWidth();
  const float height = box.getHeight();
  canvas.drawLine({ left + width * 0.22f, top + height * 0.52f },
    { left + width * 0.43f, top + height * 0.72f }, stroke);
  canvas.drawLine({ left + width * 0.43f, top + height * 0.72f },
    { left + width * 0.78f, top + height * 0.30f }, stroke);
}

} // namespace coral::ui::detail

#endif // CORAL_UI_WIDGET_DRAWING_H
