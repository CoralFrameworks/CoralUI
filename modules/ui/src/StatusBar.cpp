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

#include <ui/StatusBar.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

StatusBar::StatusBar(std::shared_ptr<const Font> font, std::string left, std::string right)
    : mLeft(std::move(left)), mRight(std::move(right)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
}

void StatusBar::setLeftText(std::string text) {
  mLeft = std::move(text);
  setAccessibleValue(mLeft.empty() ? mRight : mLeft);
  setNeedsPaint();
}

void StatusBar::setRightText(std::string text) {
  mRight = std::move(text);
  setAccessibleValue(mLeft.empty() ? mRight : mLeft);
  setNeedsPaint();
}

void StatusBar::addLeadingItem(std::shared_ptr<Layer> item) {
  if (item == nullptr) return;
  addChild(item);
  mLeadingItems.push_back(std::move(item));
  setNeedsLayout();
  setNeedsPaint();
}

void StatusBar::setTrailingItem(std::shared_ptr<Layer> item) {
  for (const auto& existing: mTrailingItems) {
    if (existing != nullptr) removeChild(existing);
  }
  mTrailingItems.clear();
  addTrailingItem(std::move(item));
}

void StatusBar::addTrailingItem(std::shared_ptr<Layer> item) {
  if (item == nullptr) return;
  addChild(item);
  mTrailingItems.push_back(std::move(item));
  setNeedsLayout();
  setNeedsPaint();
}

void StatusBar::clearItems() {
  for (const auto& item: mLeadingItems) {
    if (item != nullptr) removeChild(item);
  }
  for (const auto& item: mTrailingItems) {
    if (item != nullptr) removeChild(item);
  }
  mLeadingItems.clear();
  mTrailingItems.clear();
  setNeedsLayout();
  setNeedsPaint();
}

void StatusBar::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsPaint();
}

void StatusBar::setColors(const Color background, const Color foreground, const Color border) {
  mBackground = background;
  mForeground = foreground;
  mBorder = border;
  setNeedsPaint();
}

void StatusBar::setHeight(const float height) {
  mHeight = std::max(16.0f, height);
  setNeedsLayout();
  setNeedsPaint();
}

Size StatusBar::measure(const Size& available) {
  setMeasuredSize({ available.width, mHeight });
  return measuredSize();
}

void StatusBar::layout() {
  int tabOrder = 0;
  float left = bounds().getLeft() + mPadding;
  for (const auto& item: mLeadingItems) {
    if (item == nullptr) continue;
    const Size size = item->measure({});
    item->setBounds(Rect::MakeXYWH(left,
      bounds().getTop() + (bounds().getHeight() - size.height) * 0.5f,
      size.width, size.height));
    item->layout();
    item->setTabOrder(tabOrder++);
    left += size.width + 8.0f;
  }
  for (const auto& item: mTrailingItems) {
    if (item != nullptr) item->setTabOrder(tabOrder++);
  }
  float right = bounds().getRight() - mPadding;
  for (auto item = mTrailingItems.rbegin(); item != mTrailingItems.rend(); ++item) {
    if (*item == nullptr) continue;
    const Size size = (*item)->measure({});
    right -= size.width;
    (*item)->setBounds(Rect::MakeXYWH(right,
      bounds().getTop() + (bounds().getHeight() - size.height) * 0.5f,
      size.width, size.height));
    (*item)->layout();
    right -= 8.0f;
  }
  clearNeedsLayout();
}

void StatusBar::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint fill;
  fill.color = mBackground;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRect(bounds(), fill);
  graphics::Paint border;
  border.color = mBorder;
  border.style = graphics::Paint::Style::kFill;
  canvas.drawRect(Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), bounds().getWidth(), 1.0f),
    border);
  if (mFont == nullptr) return;
  const float baseline = bounds().getTop() + (bounds().getHeight() -
    (mFont->metrics().ascent + mFont->metrics().descent)) * 0.5f;
  if (!mLeft.empty()) {
    float leading = 0.0f;
    for (const auto& item: mLeadingItems) {
      if (item != nullptr) leading += item->bounds().getWidth() + 8.0f;
    }
    detail::drawLabel(context, mLeft, bounds(), *mFont, mForeground,
      bounds().getLeft() + mPadding + leading);
  }
  if (!mRight.empty()) {
    const float width = mFont->measureWidth(mRight);
    float trailing = 0.0f;
    for (const auto& item: mTrailingItems) {
      if (item != nullptr) trailing += item->bounds().getWidth() + 8.0f;
    }
    detail::drawLabel(context, mRight, bounds(), *mFont, mForeground,
      bounds().getRight() - mPadding - trailing - width);
  }
  (void)baseline;
}

void StatusBar::applyTheme(const Theme& theme) {
  mBackground = theme.surface;
  mForeground = theme.textMuted;
  mBorder = theme.controls.border;
  mPadding = theme.spacing.sm;
  setNeedsPaint();
}

} // namespace coral::ui
