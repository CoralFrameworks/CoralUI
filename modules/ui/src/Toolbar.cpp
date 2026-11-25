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

#include <ui/Toolbar.h>

#include <ui/Button.h>
#include <ui/Menu.h>
#include <ui/Theme.h>
#include <ui/Window.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Toolbar::Toolbar(std::shared_ptr<const Font> font) : mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
  if (mFont != nullptr) {
    mOverflowButton = std::make_shared<Button>("...", mFont);
    mOverflowButton->setVisible(false);
    mOverflowButton->setOnClick([this] {
      if (mOverflowMenu == nullptr || window() == nullptr || window()->root() == nullptr) return;
      if (mOverflowMenu->parent() != window()->root().get()) {
        if (mOverflowMenu->parent() != nullptr) mOverflowMenu->parent()->removeChild(mOverflowMenu);
        window()->root()->addChild(mOverflowMenu);
      }
      mOverflowMenu->showAt(mOverflowButton->boundsInWindow());
    });
    addChild(mOverflowButton);
  }
}

void Toolbar::setOverflowMenu(std::shared_ptr<Menu> menu) {
  mAutoOverflowMenu.reset();
  mOverflowMenu = std::move(menu);
  if (mOverflowMenu != nullptr) mOverflowMenu->hide();
}

void Toolbar::setHeight(const float height) {
  mHeight = std::max(24.0f, height);
  setNeedsLayout();
  setNeedsPaint();
}

void Toolbar::setColors(const Color background, const Color border) {
  mBackground = background;
  mBorder = border;
  setNeedsPaint();
}

void Toolbar::addItem(std::shared_ptr<Layer> item, const bool flexible,
    std::string overflowLabel, std::function<void()> overflowAction) {
  if (item == nullptr) return;
  Item entry;
  entry.layer = std::move(item);
  entry.flexible = flexible;
  entry.overflowLabel = std::move(overflowLabel);
  entry.overflowAction = std::move(overflowAction);
  if (!entry.overflowLabel.empty() && mOverflowMenu == nullptr && mFont != nullptr) {
    mAutoOverflowMenu = std::make_shared<Menu>(mFont);
    mOverflowMenu = mAutoOverflowMenu;
  }
  addChild(entry.layer);
  mItems.push_back(std::move(entry));
  setNeedsLayout();
  setNeedsPaint();
}

void Toolbar::addSeparator() {
  Item entry;
  entry.separator = true;
  mItems.push_back(std::move(entry));
  setNeedsLayout();
  setNeedsPaint();
}

void Toolbar::clear() {
  for (const Item& item: mItems) {
    if (item.layer != nullptr) removeChild(item.layer);
  }
  mItems.clear();
  mHasOverflow = false;
  if (mAutoOverflowMenu != nullptr) mAutoOverflowMenu->clear();
  if (mOverflowButton != nullptr) mOverflowButton->setVisible(false);
  setNeedsLayout();
  setNeedsPaint();
}

Size Toolbar::measure(const Size& available) {
  setMeasuredSize({ available.width, mHeight });
  return measuredSize();
}

void Toolbar::layout() {
  const float stripHeight = std::min(bounds().getHeight(), mHeight);
  const float innerHeight = std::max(0.0f, stripHeight - mPadding * 2.0f);
  const float overflowWidth = mOverflowButton != nullptr
    ? mOverflowButton->measure({}).width : 0.0f;
  float fixed = mPadding * 2.0f;
  int flexibleCount = 0;
  for (const Item& item: mItems) {
    if (item.separator) fixed += 1.0f;
    else if (item.layer != nullptr) {
      if (item.flexible) ++flexibleCount;
      else fixed += item.layer->measure({}).width + mGap;
    }
  }
  mHasOverflow = mOverflowButton != nullptr && mOverflowMenu != nullptr &&
    fixed + overflowWidth + mGap > bounds().getWidth();
  const float available = mHasOverflow
    ? std::max(0.0f, bounds().getWidth() - overflowWidth - mGap) : bounds().getWidth();
  const float remaining = std::max(0.0f, available - fixed);
  const float flexibleWidth = flexibleCount > 0 ? remaining / static_cast<float>(flexibleCount) : 0.0f;
  float left = bounds().getLeft() + mPadding;
  int tabOrder = 0;
  for (Item& item: mItems) {
    if (item.separator) {
      item.visible = !mHasOverflow || left < bounds().getLeft() + available - mPadding;
      if (item.visible) left += 1.0f;
      continue;
    }
    if (item.layer == nullptr) continue;
    const float width = item.flexible ? flexibleWidth : item.layer->measure({}).width;
    const bool fits = !mHasOverflow || left + width <= bounds().getLeft() + available - mPadding;
    item.visible = fits;
    item.layer->setVisible(fits);
    if (fits) item.layer->setTabOrder(tabOrder++);
    if (fits) {
      item.layer->setBounds(Rect::MakeXYWH(left, bounds().getTop() + mPadding,
        std::max(0.0f, width), innerHeight));
      item.layer->layout();
      left += width + mGap;
    }
  }
  if (mAutoOverflowMenu != nullptr && mOverflowMenu == mAutoOverflowMenu) {
    mAutoOverflowMenu->clear();
    for (const Item& item: mItems) {
      if (!item.separator && !item.visible && !item.overflowLabel.empty()) {
        const std::string label = item.overflowLabel;
        const auto action = item.overflowAction;
        mAutoOverflowMenu->addItem(label, [action] {
          if (action) action();
        });
      }
    }
  }
  if (mOverflowButton != nullptr) {
    mOverflowButton->setVisible(mHasOverflow);
    if (mHasOverflow) mOverflowButton->setTabOrder(tabOrder++);
    if (mHasOverflow) {
      mOverflowButton->setBounds(Rect::MakeXYWH(
        bounds().getRight() - mPadding - overflowWidth,
        bounds().getTop() + (stripHeight - mOverflowButton->measure({}).height) * 0.5f,
        overflowWidth, mOverflowButton->measure({}).height));
      mOverflowButton->layout();
    }
  }
  clearNeedsLayout();
}

void Toolbar::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  const float stripHeight = std::min(bounds().getHeight(), mHeight);
  graphics::Paint fill;
  fill.color = mBackground;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRect(Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(),
    bounds().getWidth(), stripHeight), fill);
  graphics::Paint border;
  border.color = mBorder;
  border.style = graphics::Paint::Style::kFill;
  canvas.drawRect(Rect::MakeXYWH(bounds().getLeft(), bounds().getTop() + stripHeight - 1.0f,
    bounds().getWidth(), 1.0f), border);
  float left = bounds().getLeft() + mPadding;
  for (const Item& item: mItems) {
    if (item.separator) {
      if (!item.visible) continue;
      graphics::Paint line;
      line.color = mBorder;
      line.style = graphics::Paint::Style::kFill;
      canvas.drawRect(Rect::MakeXYWH(left, bounds().getTop() + mPadding + 4.0f, 1.0f,
        std::max(0.0f, stripHeight - mPadding * 2.0f - 8.0f)), line);
      left += 1.0f;
      continue;
    }
    if (item.layer != nullptr && item.layer->visible()) {
      left += item.layer->bounds().getWidth() + mGap;
    }
  }
}

void Toolbar::applyTheme(const Theme& theme) {
  mBackground = theme.surface;
  mBorder = theme.controls.border;
  mPadding = theme.spacing.xs;
  mGap = theme.spacing.xs;
  setNeedsLayout();
  setNeedsPaint();
}

} // namespace coral::ui
