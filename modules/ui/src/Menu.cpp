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

#include <ui/Menu.h>

#include <ui/Positioner.h>
#include "TextPainter.h"

#include <ui/Theme.h>

#include <graphics/Image.h>
#include <graphics/Paint.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

namespace {

constexpr float kItemGutter = 24.0f;

} // namespace anonymous

Menu::Menu(std::shared_ptr<const graphics::Font> font) : mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Menu);
  setVisible(false);
}

Menu::~Menu() {
  if (mCommands != nullptr && mCommandsToken != 0) {
    mCommands->removeChangedHandler(mCommandsToken);
  }
}

void Menu::addItem(std::string label, std::function<void()> action, const bool enabled) {
  Item item;
  item.label = std::move(label);
  item.action = std::move(action);
  item.enabled = enabled;
  mItems.push_back(std::move(item));
  updateWidth();
  setNeedsLayout();
  setNeedsPaint();
}

int Menu::addCheckItem(std::string label, const bool checked,
    std::function<void(bool)> onToggle, const bool enabled) {
  Item item;
  item.label = std::move(label);
  item.mark = Mark::Check;
  item.checked = checked;
  item.onToggle = std::move(onToggle);
  item.enabled = enabled;
  mItems.push_back(std::move(item));
  updateWidth();
  setNeedsLayout();
  setNeedsPaint();
  return static_cast<int>(mItems.size()) - 1;
}

int Menu::addRadioItem(std::string label, const bool checked,
    std::function<void()> onSelect, const bool enabled) {
  Item item;
  item.label = std::move(label);
  item.mark = Mark::Radio;
  item.checked = checked;
  item.action = std::move(onSelect);
  item.enabled = enabled;
  if (checked) {
    for (Item& entry: mItems) {
      if (entry.mark == Mark::Radio) entry.checked = false;
    }
  }
  mItems.push_back(std::move(item));
  updateWidth();
  setNeedsLayout();
  setNeedsPaint();
  return static_cast<int>(mItems.size()) - 1;
}

void Menu::addSeparator() {
  Item item;
  item.separator = true;
  mItems.push_back(std::move(item));
  setNeedsLayout();
  setNeedsPaint();
}

int Menu::addCommandItem(std::string label, std::string commandName, std::string shortcut) {
  Item item;
  item.label = std::move(label);
  item.commandName = std::move(commandName);
  item.shortcutOverride = std::move(shortcut);
  mItems.push_back(std::move(item));
  const int index = static_cast<int>(mItems.size()) - 1;
  refreshCommands();
  return index;
}

int Menu::addCommandCheckItem(std::string label, std::string commandName) {
  Item item;
  item.label = std::move(label);
  item.commandName = std::move(commandName);
  item.mark = Mark::Check;
  mItems.push_back(std::move(item));
  const int index = static_cast<int>(mItems.size()) - 1;
  refreshCommands();
  return index;
}

void Menu::setCommands(CommandRegistry* commands) {
  if (mCommands != nullptr && mCommandsToken != 0) {
    mCommands->removeChangedHandler(mCommandsToken);
    mCommandsToken = 0;
  }
  mCommands = commands;
  if (mCommands != nullptr) {
    // A command can be disabled or checked from elsewhere while this menu is open (a shortcut
    // toggles it, another view disables it); follow the registry instead of polling every frame.
    mCommandsToken = mCommands->addChangedHandler([this] {
      if (isOpen()) refreshCommands();
    });
  }
  refreshCommands();
  // Submenus answer requests from the same command table, so they must follow it too.
  for (Item& item: mItems) {
    if (item.submenu != nullptr) item.submenu->setCommands(commands);
  }
}

void Menu::refreshCommands() {
  if (mCommands == nullptr) return;
  for (Item& item: mItems) {
    if (item.commandName.empty()) continue;
    item.enabled = mCommands->isEnabled(item.commandName);
    if (item.mark == Mark::Check) item.checked = mCommands->isChecked(item.commandName);
    // A caller-provided label wins; otherwise the registry's shortcut text is the menu's label.
    item.shortcut = item.shortcutOverride.empty() ? mCommands->shortcutLabelFor(item.commandName)
                                                  : item.shortcutOverride;
  }
  updateWidth();
  setNeedsLayout();
  setNeedsPaint();
}

void Menu::addSubmenu(std::string label, std::shared_ptr<Menu> submenu) {
  Item item;
  item.label = std::move(label);
  item.submenu = std::move(submenu);
  if (item.submenu != nullptr) {
    item.submenu->mOwner = this;
    if (mCommands != nullptr) item.submenu->setCommands(mCommands);
  }
  mItems.push_back(std::move(item));
  updateWidth();
  setNeedsLayout();
  setNeedsPaint();
}

void Menu::updateWidth() {
  if (mFont == nullptr) return;
  float width = 0.0f;
  for (const Item& entry: mItems) {
    float entryWidth = mFont->measureWidth(entry.label);
    if (!entry.shortcut.empty()) entryWidth += mFont->measureWidth(entry.shortcut) + 20.0f;
    width = std::max(width, entryWidth);
  }
  mWidth = std::max(96.0f, width + kItemGutter + 18.0f);
}

void Menu::setShortcut(const int index, std::string shortcut) {
  if (index < 0 || index >= static_cast<int>(mItems.size())) return;
  mItems[static_cast<size_t>(index)].shortcut = std::move(shortcut);
  updateWidth();
  setNeedsLayout();
  setNeedsPaint();
}

void Menu::setItemEnabled(const int index, const bool enabled) {
  if (index < 0 || index >= static_cast<int>(mItems.size())) return;
  mItems[static_cast<size_t>(index)].enabled = enabled;
  setNeedsPaint();
}

void Menu::setItemChecked(const int index, const bool checked) {
  if (index < 0 || index >= static_cast<int>(mItems.size())) return;
  Item& item = mItems[static_cast<size_t>(index)];
  if (item.mark == Mark::None) return;
  item.checked = checked;
  if (checked && item.mark == Mark::Radio) {
    for (size_t other = 0; other < mItems.size(); ++other) {
      if (other != static_cast<size_t>(index) && mItems[other].mark == Mark::Radio) {
        mItems[other].checked = false;
      }
    }
  }
  setNeedsPaint();
}

std::string Menu::itemLabel(const int index) const {
  if (index < 0 || index >= static_cast<int>(mItems.size())) return {};
  return mItems[static_cast<size_t>(index)].label;
}

bool Menu::isItemChecked(const int index) const {
  if (index < 0 || index >= static_cast<int>(mItems.size())) return false;
  return mItems[static_cast<size_t>(index)].checked;
}

void Menu::setItemIcon(const int index, std::shared_ptr<const graphics::Image> icon) {
  if (index < 0 || index >= static_cast<int>(mItems.size())) return;
  mItems[static_cast<size_t>(index)].icon = std::move(icon);
  setNeedsPaint();
}

void Menu::clear() {
  hideSubmenu();
  mItems.clear();
  mHover = -1;
  setNeedsLayout();
  setNeedsPaint();
}

void Menu::showAt(const Point& position) {
  // A point anchor means "the panel's top-left is here"; the rectangle overload puts a panel below
  // its anchor, so undo that two-pixel offset for the point form.
  showAt(Rect::MakeXYWH(position.x, position.y - 2.0f, 0.0f, 0.0f));
}

void Menu::showAt(const Rect& anchor) {
  // A menu is rebuilt from the command table each time it opens: enabled/checked state may have
  // changed since it last closed.
  refreshCommands();
  const Size size = measure({});
  Rect parentBounds = anchor;
  if (parent() != nullptr) parentBounds = parent()->bounds();
  PlacementRequest request;
  request.anchor = anchor;
  request.size = size;
  request.available = parent() != nullptr ? parentBounds : Rect{};
  request.preferred = Placement::BelowStart;
  request.gap = 2.0f;
  // A menu is allowed to touch the window edge: the old behaviour had no inset and the flip/clamp
  // rules are covered by existing menu tests.
  request.margin = Insets{ 0.0f, 0.0f, 0.0f, 0.0f };
  mPanel = placeFloating(request).bounds;
  setBounds(parent() != nullptr ? parentBounds : mPanel);
  mHover = -1;
  hideSubmenu();
  setVisible(true);
  layout();
}

void Menu::hide() {
  hideSubmenu();
  mHover = -1;
  setVisible(false);
}

void Menu::hideAll() {
  if (mOwner != nullptr) {
    mOwner->hideAll();
  } else {
    hide();
  }
}

void Menu::setColors(const graphics::Color background, const graphics::Color foreground) {
  mBackground = background;
  mForeground = foreground;
  setNeedsPaint();
}

Size Menu::measure(const Size& available) {
  (void)available;
  float height = mPadding * 2.0f;
  for (size_t index = 0; index < mItems.size(); ++index) height += itemHeight(index);
  setMeasuredSize({ mWidth, height });
  return measuredSize();
}

void Menu::layout() {
  clearNeedsLayout();
}

float Menu::itemHeight(const size_t index) const {
  return mItems[index].separator ? mSeparatorHeight : mRowHeight;
}

float Menu::itemTop(const size_t index) const {
  float top = mPadding;
  for (size_t current = 0; current < index; ++current) top += itemHeight(current);
  return top;
}

void Menu::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint background;
  background.color = mBackground;
  background.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(mPanel, mRadius), background);

  graphics::Paint border;
  border.color = mBorder;
  border.style = graphics::Paint::Style::kStroke;
  border.strokeWidth = 1.0f;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(mPanel, mRadius), border);

  if (mFont == nullptr) return;
  graphics::FontMetrics metrics = mFont->metrics();
  if (mTextPainters.size() < mItems.size()) mTextPainters.resize(mItems.size());
  for (size_t index = 0; index < mItems.size(); ++index) {
    const float top = mPanel.getTop() + itemTop(index);
    const float height = itemHeight(index);
    const Item& item = mItems[index];
    if (item.separator) {
      graphics::Paint separator;
      separator.color = mBorder;
      separator.style = graphics::Paint::Style::kFill;
      canvas.drawRect(Rect::MakeXYWH(mPanel.getLeft() + mPadding,
        top + height * 0.5f, std::max(0.0f, mPanel.getWidth() - mPadding * 2.0f), 1.0f), separator);
      continue;
    }
    if (static_cast<int>(index) == mHover && item.enabled) {
      graphics::Paint hover;
      hover.color = mHoverColor;
      hover.style = graphics::Paint::Style::kFill;
      canvas.drawRect(Rect::MakeXYWH(mPanel.getLeft(), top, mPanel.getWidth(), height), hover);
    }
    graphics::Paint text;
    text.color = item.enabled ? mForeground : mMuted;
    text.style = graphics::Paint::Style::kFill;
    const float baseline = top + (height + metrics.ascent - metrics.descent) * 0.5f;

    const float gutterLeft = mPanel.getLeft() + mPadding * 2.0f;
    const float gutterCenter = top + height * 0.5f;
    if (item.icon != nullptr && item.icon->width() > 0 && item.icon->height() > 0) {
      constexpr float iconSize = 14.0f;
      graphics::Paint iconPaint;
      iconPaint.color = item.enabled ? mForeground : mMuted;
      iconPaint.style = graphics::Paint::Style::kFill;
      canvas.save();
      canvas.translate(gutterLeft + 1.0f, gutterCenter - iconSize * 0.5f);
      canvas.scale(iconSize / static_cast<float>(item.icon->width()),
        iconSize / static_cast<float>(item.icon->height()));
      canvas.drawImage(*item.icon, 0.0f, 0.0f, iconPaint);
      canvas.restore();
    } else if (item.mark == Mark::Check && item.checked) {
      graphics::Paint check;
      check.color = item.enabled ? mForeground : mMuted;
      check.style = graphics::Paint::Style::kStroke;
      check.strokeWidth = 1.8f;
      canvas.drawLine({ gutterLeft + 2.0f, gutterCenter },
        { gutterLeft + 6.0f, gutterCenter + 4.0f }, check);
      canvas.drawLine({ gutterLeft + 6.0f, gutterCenter + 4.0f },
        { gutterLeft + 13.0f, gutterCenter - 4.0f }, check);
    } else if (item.mark == Mark::Radio) {
      graphics::Paint ring;
      ring.color = item.enabled ? mForeground : mMuted;
      ring.style = graphics::Paint::Style::kStroke;
      ring.strokeWidth = 1.2f;
      canvas.drawCircle(gutterLeft + 7.0f, gutterCenter, 5.0f, ring);
      if (item.checked) {
        graphics::Paint dot = ring;
        dot.style = graphics::Paint::Style::kFill;
        canvas.drawCircle(gutterLeft + 7.0f, gutterCenter, 2.6f, dot);
      }
    }

    if (mTextPainters[index] == nullptr) mTextPainters[index] = std::make_unique<TextPainter>();
    mTextPainters[index]->draw(context, item.label, gutterLeft + kItemGutter,
      baseline, *mFont, text);

    float right = mPanel.getRight() - mPadding * 2.0f;
    if (item.submenu != nullptr) {
      mTextPainters[index]->draw(context, ">", right - mFont->measureWidth(">"),
        baseline, *mFont, text);
      right -= mFont->measureWidth(">") + 8.0f;
    }
    if (!item.shortcut.empty()) {
      mTextPainters[index]->draw(context, item.shortcut,
        right - mFont->measureWidth(item.shortcut), baseline, *mFont, text);
    }
  }
}

int Menu::itemAt(const Point& point) const {
  if (!mPanel.contains(point)) return -1;
  const float local = point.y - mPanel.getTop() - mPadding;
  if (local < 0.0f) return -1;
  float top = 0.0f;
  for (size_t index = 0; index < mItems.size(); ++index) {
    const float height = itemHeight(index);
    if (local >= top && local < top + height) return static_cast<int>(index);
    top += height;
  }
  return -1;
}

void Menu::showSubmenu(const int index) {
  if (index < 0 || index >= static_cast<int>(mItems.size())) return;
  const Item& item = mItems[static_cast<size_t>(index)];
  if (item.submenu == nullptr || !item.enabled) return;
  if (mOpenSubmenu == item.submenu) return;
  hideSubmenu();
  mOpenSubmenu = item.submenu;
  if (mOpenSubmenu->parent() != this) addChild(mOpenSubmenu);
  const float top = mPanel.getTop() + itemTop(static_cast<size_t>(index));
  mOpenSubmenu->showAt(Point{ mPanel.getRight() - 2.0f, top });
}

void Menu::hideSubmenu() {
  if (mOpenSubmenu != nullptr) {
    mOpenSubmenu->hide();
    mOpenSubmenu.reset();
  }
}

void Menu::activate(const int index) {
  if (index < 0 || index >= static_cast<int>(mItems.size())) return;
  Item& item = mItems[static_cast<size_t>(index)];
  if (item.separator || !item.enabled) return;
  if (item.submenu != nullptr) {
    showSubmenu(index);
    return;
  }
  if (!item.commandName.empty() && mCommands != nullptr) {
    // The registry is the single source of truth: re-check availability at click time and let the
    // command own the action instead of duplicating it in the item.
    if (!mCommands->isEnabled(item.commandName)) return;
    if (item.mark == Mark::Check) {
      mCommands->setChecked(item.commandName, !mCommands->isChecked(item.commandName));
      item.checked = mCommands->isChecked(item.commandName);
    }
    hideAll();
    mCommands->invoke(item.commandName);
    return;
  }
  if (item.mark == Mark::Check) {
    const bool checked = !item.checked;
    item.checked = checked;
    const auto onToggle = item.onToggle;
    hideAll();
    if (onToggle) onToggle(checked);
    return;
  }
  if (item.mark == Mark::Radio) {
    for (size_t other = 0; other < mItems.size(); ++other) {
      if (mItems[other].mark == Mark::Radio) mItems[other].checked = false;
    }
    item.checked = true;
    const auto action = item.action;
    hideAll();
    if (action) action();
    return;
  }
  const std::function<void()> action = item.action;
  hideAll();
  if (action) action();
}

bool Menu::onPointerDown(const PointerEvent& event) {
  if (!mPanel.contains(event.position)) {
    hideAll();
    return true;
  }
  const int index = itemAt(event.position);
  if (index < 0) return true;
  if (mItems[static_cast<size_t>(index)].separator ||
      !mItems[static_cast<size_t>(index)].enabled) {
    return true;
  }
  activate(index);
  return true;
}

bool Menu::onPointerMove(const PointerEvent& event) {
  const int index = itemAt(event.position);
  const bool valid = index >= 0 && !mItems[static_cast<size_t>(index)].separator &&
    mItems[static_cast<size_t>(index)].enabled;
  const int hover = valid ? index : -1;
  if (hover != mHover) {
    mHover = hover;
    setNeedsPaint();
  }
  if (valid && mItems[static_cast<size_t>(index)].submenu != nullptr) {
    showSubmenu(index);
  } else {
    hideSubmenu();
  }
  return valid;
}

bool Menu::onPointerLeave() {
  if (mHover != -1) {
    mHover = -1;
    setNeedsPaint();
  }
  return true;
}

void Menu::applyTheme(const Theme& theme) {
  mBackground = theme.surfaceRaised;
  mBorder = theme.controls.border;
  mHoverColor = theme.controls.backgroundPressed;
  mForeground = theme.controls.foreground;
  mMuted = theme.textMuted;
  mPadding = theme.spacing.xs;
  mRadius = theme.radius.md;
  setNeedsLayout();
  setNeedsPaint();
}

bool Menu::onKeyDown(const KeyEvent& event) {
  if (!visible()) return false;
  if (event.key == Key::Escape) {
    hideAll();
    return true;
  }
  if (event.key == Key::Tab) {
    // A menu is modal enough for navigation: Tab must not move focus to a control behind it.
    return true;
  }
  const int count = static_cast<int>(mItems.size());
  if (count == 0) return false;
  if (event.key == Key::Down || event.key == Key::Up) {
    const int step = event.key == Key::Down ? 1 : -1;
    int index = mHover < 0 ? (step > 0 ? -1 : count) : mHover;
    for (int attempt = 0; attempt < count; ++attempt) {
      index = (index + step + count) % count;
      if (!mItems[static_cast<size_t>(index)].separator &&
          mItems[static_cast<size_t>(index)].enabled) {
        mHover = index;
        if (mItems[static_cast<size_t>(index)].submenu != nullptr) showSubmenu(index);
        else hideSubmenu();
        setNeedsPaint();
        return true;
      }
    }
    return false;
  }
  if (event.key == Key::Home || event.key == Key::End) {
    const int start = event.key == Key::Home ? 0 : count - 1;
    const int step = event.key == Key::Home ? 1 : -1;
    for (int attempt = 0; attempt < count; ++attempt) {
      const int index = (start + step * attempt + count) % count;
      if (!mItems[static_cast<size_t>(index)].separator &&
          mItems[static_cast<size_t>(index)].enabled) {
        mHover = index;
        setNeedsPaint();
        return true;
      }
    }
    return false;
  }
  if (event.key == Key::Right) {
    if (mHover >= 0) showSubmenu(mHover);
    return true;
  }
  if (event.key == Key::Left) {
    if (mOpenSubmenu != nullptr) {
      hideSubmenu();
    } else if (mOwner != nullptr) {
      mOwner->hideSubmenu();
    }
    return true;
  }
  if (event.key == Key::Enter || event.key == Key::Space) {
    if (mHover >= 0) activate(mHover);
    return true;
  }
  return false;
}

} // namespace coral::ui
