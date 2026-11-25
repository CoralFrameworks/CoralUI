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

#include <ui/MenuBar.h>

#include <ui/Command.h>
#include <ui/RenderContext.h>
#include <ui/Theme.h>
#include "TextPainter.h"

#include <algorithm>

namespace coral::ui {

MenuBar::MenuBar(std::shared_ptr<const Font> font) : mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Menu);
}

MenuBar::~MenuBar() = default;

int MenuBar::addMenu(std::string label, std::shared_ptr<Menu> menu) {
  if (menu == nullptr) return -1;
  Entry entry;
  entry.label = std::move(label);
  entry.menu = std::move(menu);
  mEntries.push_back(std::move(entry));
  if (mCommands != nullptr) mEntries.back().menu->setCommands(mCommands);
  updateWidths();
  return static_cast<int>(mEntries.size()) - 1;
}

void MenuBar::setCommands(CommandRegistry* commands) {
  mCommands = commands;
  for (Entry& entry: mEntries) entry.menu->setCommands(commands);
}

void MenuBar::updateWidths() {
  float left = 0.0f;
  for (Entry& entry: mEntries) {
    const float labelWidth = mFont != nullptr ? mFont->measureWidth(entry.label) : 0.0f;
    entry.left = left;
    entry.width = labelWidth + mPadding * 2.0f;
    left += entry.width;
  }
  setNeedsLayout();
  setNeedsPaint();
}

Rect MenuBar::labelBounds(const int index) const {
  if (index < 0 || index >= static_cast<int>(mEntries.size())) return Rect{};
  const Entry& entry = mEntries[static_cast<size_t>(index)];
  return Rect::MakeXYWH(bounds().getLeft() + entry.left, bounds().getTop(), entry.width, mHeight);
}

int MenuBar::indexAt(const float x) const {
  const float local = x - bounds().getLeft();
  for (size_t index = 0; index < mEntries.size(); ++index) {
    if (local >= mEntries[index].left && local < mEntries[index].left + mEntries[index].width) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

void MenuBar::openMenu(const int index) {
  if (index < 0 || index >= static_cast<int>(mEntries.size())) return;
  closeMenu();
  mOpen = index;
  Menu& menu = *mEntries[static_cast<size_t>(index)].menu;
  if (parent() != nullptr && menu.parent() != parent()) parent()->addChild(mEntries[static_cast<size_t>(index)].menu);
  const Rect bar = boundsInWindow();
  const Entry& entry = mEntries[static_cast<size_t>(index)];
  menu.showAt(Rect::MakeXYWH(bar.getLeft() + entry.left, bar.getTop(), entry.width, mHeight));
  setNeedsPaint();
}

void MenuBar::closeMenu() {
  if (mOpen < 0) return;
  if (mOpen < static_cast<int>(mEntries.size()) && mEntries[static_cast<size_t>(mOpen)].menu) {
    mEntries[static_cast<size_t>(mOpen)].menu->hide();
  }
  mOpen = -1;
  setNeedsPaint();
}

void MenuBar::close() {
  closeMenu();
}

Size MenuBar::measure(const Size& available) {
  (void)available;
  float width = 0.0f;
  for (const Entry& entry: mEntries) width += entry.width;
  setMeasuredSize({ width, mHeight });
  return measuredSize();
}

void MenuBar::layout() {
  updateWidths();
}

void MenuBar::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint background;
  background.color = mBackground;
  background.style = graphics::Paint::Style::kFill;
  canvas.drawRect(Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(),
    std::max(bounds().getWidth(), measuredSize().width), mHeight), background);
  if (mFont == nullptr) return;
  if (mTextPainters.size() < mEntries.size()) mTextPainters.resize(mEntries.size());
  const graphics::FontMetrics metrics = mFont->metrics();
  for (size_t index = 0; index < mEntries.size(); ++index) {
    const Rect label = labelBounds(static_cast<int>(index));
    if (static_cast<int>(index) == mOpen) {
      graphics::Paint highlight;
      highlight.color = mHover;
      highlight.style = graphics::Paint::Style::kFill;
      canvas.drawRect(label, highlight);
    }
    auto& painter = mTextPainters[index];
    if (!painter) painter = std::make_unique<TextPainter>();
    graphics::Paint text;
    text.color = mForeground;
    const float baseline = label.getTop() + (mHeight - (metrics.ascent + metrics.descent)) * 0.5f +
                           metrics.ascent;
    painter->draw(context, mEntries[index].label, label.getLeft() + mPadding, baseline, *mFont,
      text);
  }
}

bool MenuBar::onPointerDown(const PointerEvent& event) {
  const int index = indexAt(event.position.x);
  if (index < 0) {
    closeMenu();
    return true;
  }
  if (index == mOpen) {
    closeMenu();
    return true;
  }
  openMenu(index);
  return true;
}

bool MenuBar::onPointerMove(const PointerEvent& event) {
  // With a menu open, moving across the bar switches menus, which is what desktop menu bars do.
  if (mOpen < 0) return false;
  const int index = indexAt(event.position.x);
  if (index >= 0 && index != mOpen) openMenu(index);
  return false;
}

bool MenuBar::onPointerLeave() {
  return false;
}

bool MenuBar::onKeyDown(const KeyEvent& event) {
  if (event.key == Key::Escape) {
    if (mOpen < 0) return false;
    closeMenu();
    return true;
  }
  if (event.key == Key::Left || event.key == Key::Right) {
    if (mEntries.empty()) return false;
    if (mOpen < 0) {
      openMenu(0);
      return true;
    }
    const int count = static_cast<int>(mEntries.size());
    const int direction = event.key == Key::Right ? 1 : -1;
    openMenu((mOpen + direction + count) % count);
    return true;
  }
  return false;
}

void MenuBar::applyTheme(const Theme& theme) {
  mBackground = theme.surfaceRaised;
  mForeground = theme.text;
  mHover = theme.controls.backgroundHover;
  if (mFont == nullptr) mFont = theme.body;
  for (auto& painter: mTextPainters) painter.reset();
  updateWidths();
}

} // namespace coral::ui
