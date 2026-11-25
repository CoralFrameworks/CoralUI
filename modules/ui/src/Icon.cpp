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

#include <ui/Icon.h>

#include <graphics/Image.h>
#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Icon::Icon(std::shared_ptr<const graphics::Image> image, const float size)
    : mImage(std::move(image)), mSize(std::max(8.0f, size)) {
  setAccessibleRole(AccessibilityRole::Label);
}

void Icon::setImage(std::shared_ptr<const graphics::Image> image) {
  mImage = std::move(image);
  setNeedsPaint();
}

void Icon::setSize(const float size) {
  mSize = std::max(8.0f, size);
  setNeedsLayout();
  setNeedsPaint();
}

void Icon::setColor(const Color color) {
  mColor = color;
  setNeedsPaint();
}

Size Icon::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ mSize, mSize });
  return measuredSize();
}

void Icon::drawSelf(RenderContext& context) {
  if (mImage == nullptr || mImage->width() <= 0 || mImage->height() <= 0) return;
  const float scale = std::min(bounds().getWidth() / static_cast<float>(mImage->width()),
    bounds().getHeight() / static_cast<float>(mImage->height()));
  const float width = static_cast<float>(mImage->width()) * scale;
  const float height = static_cast<float>(mImage->height()) * scale;
  graphics::Paint paint;
  paint.color = mColor;
  paint.style = graphics::Paint::Style::kFill;
  context.canvas().drawImage(*mImage,
    bounds().getLeft() + (bounds().getWidth() - width) * 0.5f,
    bounds().getTop() + (bounds().getHeight() - height) * 0.5f, paint);
}

void Icon::applyTheme(const Theme& theme) {
  mColor = theme.controls.foreground;
  setNeedsPaint();
}

} // namespace coral::ui
