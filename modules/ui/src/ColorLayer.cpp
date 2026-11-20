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

#include <ui/ColorLayer.h>

#include <graphics/Paint.h>

namespace coral::ui {

ColorLayer::ColorLayer(const graphics::Color color) : mColor(color) {}

void ColorLayer::setColor(const graphics::Color color) {
  mColor = color;
  setNeedsPaint();
}

Size ColorLayer::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ bounds().getWidth(), bounds().getHeight() });
  return measuredSize();
}

void ColorLayer::drawSelf(RenderContext& context) {
  if (mColor.alpha == 0.0f) {
    return;
  }
  auto& canvas = context.canvas();
  graphics::Paint paint;
  paint.color = mColor;
  paint.style = graphics::Paint::Style::kFill;
  canvas.drawRect(bounds(), paint);
}

} // namespace coral::ui
