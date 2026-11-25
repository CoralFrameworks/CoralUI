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

#include <ui/Card.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Card::Card(std::shared_ptr<const Font> font, std::string title, std::shared_ptr<Layer> content)
    : mTitle(std::move(title)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName(mTitle);
  if (content != nullptr) setContent(std::move(content));
}

void Card::setTitle(std::string title) {
  mTitle = std::move(title);
  setAccessibleName(mTitle);
  setNeedsLayout();
  setNeedsPaint();
}

void Card::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void Card::setContent(std::shared_ptr<Layer> content) {
  if (mContent != nullptr) removeChild(mContent);
  mContent = std::move(content);
  if (mContent != nullptr) addChild(mContent);
  setNeedsLayout();
  setNeedsPaint();
}

void Card::setHeaderAction(std::shared_ptr<Layer> action) {
  if (mHeaderAction != nullptr) removeChild(mHeaderAction);
  mHeaderAction = std::move(action);
  if (mHeaderAction != nullptr) addChild(mHeaderAction);
  setNeedsLayout();
  setNeedsPaint();
}

void Card::setHeaderHeight(const float height) {
  mHeaderHeight = std::max(0.0f, height);
  setNeedsLayout();
  setNeedsPaint();
}

void Card::setRadius(const float radius) {
  mRadius = std::max(0.0f, radius);
  setNeedsPaint();
}

void Card::setPadding(const float padding) {
  mPadding = std::max(0.0f, padding);
  setNeedsLayout();
  setNeedsPaint();
}

void Card::setColors(const Color background, const Color border, const Color foreground) {
  mBackground = background;
  mBorder = border;
  mForeground = foreground;
  setNeedsPaint();
}

Size Card::measure(const Size& available) {
  const float header = (mTitle.empty() && mHeaderAction == nullptr) ? 0.0f : mHeaderHeight;
  float width = 180.0f;
  float contentHeight = 0.0f;
  if (mContent != nullptr) {
    const Size contentSize = mContent->measure({
      std::max(0.0f, available.width - mPadding * 2.0f),
      std::max(0.0f, available.height - mPadding * 2.0f - header) });
    width = std::max(width, contentSize.width + mPadding * 2.0f);
    contentHeight = contentSize.height;
  }
  if (mHeaderAction != nullptr) {
    const Size actionSize = mHeaderAction->measure({});
    width = std::max(width, actionSize.width + mPadding * 2.0f);
  }
  setMeasuredSize({ width, header + contentHeight + mPadding * 2.0f });
  return measuredSize();
}

void Card::layout() {
  const float header = (mTitle.empty() && mHeaderAction == nullptr) ? 0.0f : mHeaderHeight;
  if (mContent != nullptr) {
    mContent->setBounds(Rect::MakeXYWH(bounds().getLeft() + mPadding,
      bounds().getTop() + mPadding + header,
      std::max(0.0f, bounds().getWidth() - mPadding * 2.0f),
      std::max(0.0f, bounds().getHeight() - mPadding * 2.0f - header)));
    mContent->layout();
  }
  if (mHeaderAction != nullptr) {
    const Size actionSize = mHeaderAction->measure({});
    mHeaderAction->setBounds(Rect::MakeXYWH(
      bounds().getRight() - mPadding - actionSize.width,
      bounds().getTop() + mPadding + (mHeaderHeight - actionSize.height) * 0.5f,
      actionSize.width, actionSize.height));
    mHeaderAction->layout();
  }
  clearNeedsLayout();
}

void Card::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  detail::drawShadow(canvas, bounds(), mRadius, mShadow);
  graphics::Paint fill;
  fill.color = mBackground;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), fill);

  graphics::Paint border;
  border.color = mBorder;
  border.style = graphics::Paint::Style::kStroke;
  border.strokeWidth = 1.0f;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), border);

  if (!mTitle.empty() && mFont != nullptr) {
    detail::drawLabel(context, mTitle,
      Rect::MakeXYWH(bounds().getLeft() + mPadding, bounds().getTop() + mPadding,
        bounds().getWidth() - mPadding * 2.0f, mHeaderHeight),
      *mFont, mForeground, bounds().getLeft() + mPadding);
  }
  if (!mTitle.empty() || mHeaderAction != nullptr) {
    graphics::Paint divider;
    divider.color = mBorder;
    divider.style = graphics::Paint::Style::kFill;
    canvas.drawRect(Rect::MakeXYWH(bounds().getLeft() + mPadding,
      bounds().getTop() + mPadding + mHeaderHeight,
      std::max(0.0f, bounds().getWidth() - mPadding * 2.0f), 1.0f), divider);
  }
}

void Card::applyTheme(const Theme& theme) {
  mBackground = theme.surfaceRaised;
  mBorder = theme.controls.border;
  mForeground = theme.controls.foreground;
  mPadding = theme.spacing.md;
  mRadius = theme.radius.lg;
  mShadow = theme.shadow.md;
  setNeedsLayout();
  setNeedsPaint();
}

} // namespace coral::ui
