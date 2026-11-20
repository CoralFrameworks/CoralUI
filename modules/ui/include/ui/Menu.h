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

#ifndef CORAL_UI_MENU_H
#define CORAL_UI_MENU_H

#include <ui/Types.h>

#include <ui/Command.h>
#include <ui/Layer.h>


#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace graphics {
class Image;
}

namespace coral::ui {

struct Theme;
class TextPainter;

/**
 * A small popup menu: a list of labelled actions shown at a point.
 *
 * The menu is an ordinary layer added to the root. While open it uses the parent bounds as a hit
 * catcher and draws its panel inside them; `showAt(Rect)` anchors below the rectangle and flips
 * above/left when it would overflow. Clicking an item runs its callback and dismisses the menu;
 * clicking outside dismisses it; Escape, arrows, Home/End and Enter/Space are handled by the popup
 * key route. A submenu is a child menu with the same behavior.
 */
class Menu final : public Layer {
 public:
  explicit Menu(std::shared_ptr<const Font> font);
  ~Menu() override;

  void addItem(std::string label, std::function<void()> action, bool enabled = true);
  /**
   * A menu-only command item: enabled state, checked state and shortcut text come from @p commandName
   * in the registry set by `setCommands`. @return the item index.
   */
  int addCommandItem(std::string label, std::string commandName, std::string shortcut = {});
  /** A command item whose checked state lives in the registry (toggled before the handler runs). */
  int addCommandCheckItem(std::string label, std::string commandName);
  /**
   * Binds this menu to a window's command table; `nullptr` detaches it.
   *
   * The registry must outlive the menu. While bound, the menu follows the registry's change
   * notifications: a command that is disabled or checked elsewhere updates an already-open menu
   * without an explicit `refreshCommands()`. Submenus inherit the binding.
   */
  void setCommands(CommandRegistry* commands);
  /** Re-reads enabled/checked/shortcut text for every command item. */
  void refreshCommands();
  /** A checkable item; @p onToggle receives the new checked state. */
  int addCheckItem(std::string label, bool checked, std::function<void(bool)> onToggle,
    bool enabled = true);
  /** A radio item; checking it unchecks the other radio items in this menu. */
  int addRadioItem(std::string label, bool checked, std::function<void()> onSelect,
    bool enabled = true);
  void addSeparator();
  void addSubmenu(std::string label, std::shared_ptr<Menu> submenu);
  void setShortcut(int index, std::string shortcut);
  void setItemEnabled(int index, bool enabled);
  void setItemChecked(int index, bool checked);
  [[nodiscard]] bool isItemChecked(int index) const;
  /** The label of item @p index, or an empty string when the index is out of range. */
  [[nodiscard]] std::string itemLabel(int index) const;
  void setItemIcon(int index, std::shared_ptr<const Image> icon);
  void clear();

  void showAt(const Point& position);
  /** Anchors below @p anchor, flipping above/left when the parent bounds would overflow. */
  void showAt(const Rect& anchor);
  void hide();
  [[nodiscard]] bool isOpen() const noexcept {
    return visible();
  }

  void setColors(Color background, Color foreground);

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerLeave() override;
  bool onKeyDown(const KeyEvent& event) override;
  [[nodiscard]] bool focusable() const override {
    return true;
  }
  [[nodiscard]] bool isPopup() const override {
    return true;
  }

  /** @return the index of the action at @p point, or -1. */
  [[nodiscard]] int itemAt(const Point& point) const;
  /** The visible panel rectangle, independent of a full-parent hit-catcher bounds. */
  [[nodiscard]] const Rect& panelBounds() const noexcept {
    return mPanel;
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  enum class Mark {
    None,
    Check,
    Radio,
  };

  struct Item {
    std::string label;
    std::string shortcut;
    std::function<void()> action;
    std::function<void(bool)> onToggle;
    std::shared_ptr<Menu> submenu;
    std::shared_ptr<const Image> icon;
    Mark mark = Mark::None;
    bool checked = false;
    bool separator = false;
    bool enabled = true;
    std::string commandName;
    std::string shortcutOverride;
  };

  void updateWidth();
  float itemHeight(size_t index) const;
  float itemTop(size_t index) const;
  void activate(int index);
  void showSubmenu(int index);
  void hideSubmenu();
  void hideAll();

  std::shared_ptr<const Font> mFont;
  std::vector<Item> mItems;
  CommandRegistry* mCommands = nullptr;
  /** Token of this menu's registry change listener; 0 when not bound. */
  int mCommandsToken = 0;
  Menu* mOwner = nullptr;
  std::shared_ptr<Menu> mOpenSubmenu;
  Rect mPanel{};
  Color mBackground = Color::MakeRGBA8(45, 49, 58, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mHoverColor = Color::MakeRGBA8(62, 67, 78, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mMuted = Color::MakeRGBA8(148, 163, 184, 255);
  float mRowHeight = 28.0f;
  float mWidth = 140.0f;
  float mPadding = 4.0f;
  float mRadius = 4.0f;
  int mHover = -1;
  float mSeparatorHeight = 9.0f;
  std::vector<std::unique_ptr<TextPainter>> mTextPainters;
};

/** A context menu is the same layer with an anchor; the alias keeps call sites intention-revealing. */
using ContextMenu = Menu;

} // namespace coral::ui

#endif // CORAL_UI_MENU_H
