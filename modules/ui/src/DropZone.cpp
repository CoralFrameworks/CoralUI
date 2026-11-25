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

#include <ui/DropZone.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <utility>

namespace coral::ui {

DropZone::DropZone(std::string label, std::shared_ptr<const Font> font)
    : mLabel(std::move(label)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName(mLabel);
}

DropZone::~DropZone() = default;

void DropZone::setLabel(std::string label) {
  mLabel = std::move(label);
  setAccessibleName(mLabel);
  setNeedsPaint();
}

void DropZone::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

Size DropZone::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ 260.0f, 80.0f });
  return measuredSize();
}

void DropZone::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint fill;
  fill.color = mBackground;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), 8.0f), fill);
  graphics::Paint border;
  border.color = mHovered ? mHover : mBorder;
  border.style = graphics::Paint::Style::kStroke;
  border.strokeWidth = mHovered ? 2.0f : 1.0f;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), 8.0f), border);
  if (mFont != nullptr) {
    const float width = mFont->measureWidth(mLabel);
    detail::drawLabel(context, mLabel, bounds(), *mFont,
      mHovered ? mHover : mForeground,
      bounds().getLeft() + (bounds().getWidth() - width) * 0.5f);
  }
}

DragEffect DropZone::onDragEnter(const DragEvent& event) {
  const bool accepts = !event.payload.paths().empty() || event.payload.hasType("text/plain");
  mHovered = accepts;
  setNeedsPaint();
  return accepts ? DragEffect::Copy : DragEffect::None;
}

DragEffect DropZone::onDragOver(const DragEvent& event) {
  return onDragEnter(event);
}

void DropZone::onDragLeave() {
  mHovered = false;
  setNeedsPaint();
}

bool DropZone::onDrop(const DropEvent& event) {
  mHovered = false;
  setNeedsPaint();
  if (!event.paths.empty()) {
    if (mOnFilesDropped) mOnFilesDropped(event.paths);
    return true;
  }
  if (!event.payload.paths().empty()) {
    if (mOnFilesDropped) mOnFilesDropped(event.payload.paths());
    return true;
  }
  if (event.payload.hasType("text/plain")) {
    if (mOnTextDropped) mOnTextDropped(event.payload.data());
    return true;
  }
  return false;
}

void DropZone::applyTheme(const Theme& theme) {
  mBackground = theme.surface;
  mBorder = theme.controls.border;
  mHover = theme.controls.borderFocused;
  mForeground = theme.controls.foreground;
  setNeedsPaint();
}

} // namespace coral::ui
