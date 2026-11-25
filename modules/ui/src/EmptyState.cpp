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

#include <ui/EmptyState.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

EmptyState::EmptyState(std::shared_ptr<const Font> font, std::string title,
    std::string description, std::shared_ptr<Layer> action)
    : mTitle(std::move(title)), mDescription(std::move(description)),
      mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName(mTitle);
  if (action != nullptr) setAction(std::move(action));
}

void EmptyState::setTitle(std::string title) {
  mTitle = std::move(title);
  setAccessibleName(mTitle);
  setNeedsLayout();
  setNeedsPaint();
}

void EmptyState::setDescription(std::string description) {
  mDescription = std::move(description);
  setNeedsLayout();
  setNeedsPaint();
}

void EmptyState::setAction(std::shared_ptr<Layer> action) {
  if (mAction != nullptr) removeChild(mAction);
  mAction = std::move(action);
  if (mAction != nullptr) addChild(mAction);
  setNeedsLayout();
  setNeedsPaint();
}

void EmptyState::setIcon(std::shared_ptr<const graphics::Image> icon) {
  mIcon = std::move(icon);
  setNeedsLayout();
  setNeedsPaint();
}

void EmptyState::setContent(std::shared_ptr<Layer> content) {
  if (mContent != nullptr) removeChild(mContent);
  mContent = std::move(content);
  if (mContent != nullptr) addChild(mContent);
  setNeedsLayout();
  setNeedsPaint();
}

void EmptyState::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void EmptyState::setColors(const Color titleColor, const Color descriptionColor) {
  mTitleColor = titleColor;
  mDescriptionColor = descriptionColor;
  setNeedsPaint();
}

Size EmptyState::measure(const Size& available) {
  (void)available;
  const float titleHeight = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent : 20.0f;
  float height = titleHeight;
  if (mIcon != nullptr) height += 32.0f + mGap;
  if (!mDescription.empty()) height += titleHeight + mGap;
  if (mContent != nullptr) height += mContent->measure({}).height + mGap;
  if (mAction != nullptr) height += mAction->measure({}).height + mGap * 2.0f;
  float width = 220.0f;
  if (mFont != nullptr) {
    width = std::max(width, mFont->measureWidth(mTitle) + 40.0f);
    if (!mDescription.empty()) {
      width = std::max(width, mFont->measureWidth(mDescription) + 40.0f);
    }
  }
  setMeasuredSize({ width, height });
  return measuredSize();
}

void EmptyState::layout() {
  if (mAction != nullptr) {
    const Size size = mAction->measure({});
    mAction->setBounds(Rect::MakeXYWH(
      bounds().getLeft() + (bounds().getWidth() - size.width) * 0.5f,
      bounds().getBottom() - size.height,
      size.width, size.height));
    mAction->layout();
  }
  if (mContent != nullptr) {
    const float lineHeight = mFont != nullptr
      ? mFont->metrics().ascent + mFont->metrics().descent : 20.0f;
    float top = bounds().getTop();
    if (mIcon != nullptr) top += 32.0f + mGap;
    if (!mTitle.empty()) top += lineHeight + mGap;
    if (!mDescription.empty()) top += lineHeight + mGap;
    const float bottom = mAction != nullptr ? mAction->bounds().getTop() - mGap : bounds().getBottom();
    mContent->setBounds(Rect::MakeXYWH(bounds().getLeft() + 8.0f, top,
      std::max(0.0f, bounds().getWidth() - 16.0f), std::max(0.0f, bottom - top)));
    mContent->layout();
  }
  clearNeedsLayout();
}

void EmptyState::drawSelf(RenderContext& context) {
  if (mFont == nullptr) return;
  const float lineHeight = mFont->metrics().ascent + mFont->metrics().descent;
  float y = bounds().getTop();
  if (mIcon != nullptr && mIcon->width() > 0 && mIcon->height() > 0) {
    const float maxSide = 28.0f;
    const float scale = std::min(maxSide / static_cast<float>(mIcon->width()),
      maxSide / static_cast<float>(mIcon->height()));
    const float iconW = static_cast<float>(mIcon->width()) * scale;
    const float iconH = static_cast<float>(mIcon->height()) * scale;
    graphics::Paint paint;
    paint.color = mTitleColor;
    paint.style = graphics::Paint::Style::kFill;
    context.canvas().save();
    context.canvas().translate(bounds().getLeft() + (bounds().getWidth() - iconW) * 0.5f, y);
    context.canvas().scale(scale);
    context.canvas().drawImage(*mIcon, 0.0f, 0.0f, paint);
    context.canvas().restore();
    y += 32.0f + mGap;
  }
  if (!mTitle.empty()) {
    const float width = mFont->measureWidth(mTitle);
    detail::drawLabel(context, mTitle,
      Rect::MakeXYWH(bounds().getLeft(), y, bounds().getWidth(), lineHeight),
      *mFont, mTitleColor, bounds().getLeft() + (bounds().getWidth() - width) * 0.5f);
    y += lineHeight + mGap;
  }
  if (!mDescription.empty()) {
    const float width = mFont->measureWidth(mDescription);
    detail::drawLabel(context, mDescription,
      Rect::MakeXYWH(bounds().getLeft(), y, bounds().getWidth(), lineHeight),
      *mFont, mDescriptionColor, bounds().getLeft() + (bounds().getWidth() - width) * 0.5f);
  }
  (void)y;
}

void EmptyState::applyTheme(const Theme& theme) {
  mTitleColor = theme.controls.foreground;
  mDescriptionColor = theme.textMuted;
  mGap = theme.spacing.sm;
  setNeedsPaint();
}

} // namespace coral::ui
