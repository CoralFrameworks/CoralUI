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

#include <ui/Divider.h>

#include <ui/Theme.h>

#include <algorithm>

namespace coral::ui {

Divider::Divider(const Orientation orientation) : mOrientation(orientation) {}

void Divider::setOrientation(const Orientation orientation) {
  mOrientation = orientation;
  setNeedsLayout();
  setNeedsPaint();
}

void Divider::setColor(const Color color) {
  mColor = color;
  setNeedsPaint();
}

void Divider::setThickness(const float thickness) {
  mThickness = std::max(1.0f, thickness);
  setNeedsLayout();
  setNeedsPaint();
}

void Divider::setInset(const float inset) {
  mInset = std::max(0.0f, inset);
  setNeedsPaint();
}

Size Divider::measure(const Size& available) {
  if (mOrientation == Orientation::Horizontal) {
    setMeasuredSize({ available.width, mThickness });
  } else {
    setMeasuredSize({ mThickness, available.height });
  }
  return measuredSize();
}

void Divider::drawSelf(RenderContext& context) {
  graphics::Paint paint;
  paint.color = mColor;
  paint.style = graphics::Paint::Style::kFill;
  const float inset = std::min(mInset, (mOrientation == Orientation::Horizontal
    ? bounds().getWidth() : bounds().getHeight()) * 0.5f);
  if (mOrientation == Orientation::Horizontal) {
    context.canvas().drawRect(Rect::MakeXYWH(bounds().getLeft() + inset, bounds().getTop(),
      std::max(0.0f, bounds().getWidth() - inset * 2.0f), bounds().getHeight()), paint);
  } else {
    context.canvas().drawRect(Rect::MakeXYWH(bounds().getLeft(), bounds().getTop() + inset,
      bounds().getWidth(), std::max(0.0f, bounds().getHeight() - inset * 2.0f)), paint);
  }
}

void Divider::applyTheme(const Theme& theme) {
  mColor = theme.controls.border;
  setNeedsPaint();
}

} // namespace coral::ui
