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

#ifndef CORAL_UI_MENUBAR_H
#define CORAL_UI_MENUBAR_H

#include <ui/Types.h>
#include <ui/Layer.h>
#include <ui/Menu.h>

#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

class CommandRegistry;
class TextPainter;

/**
 * A horizontal application menu bar.
 *
 * Unlike a `Menu` (a transient popup), a menu bar is a permanent strip of top-level labels. Clicking a
 * label opens its menu below; while a menu is open, hovering another label switches to it (the usual
 * desktop convention) and Left/Right move between menus, Escape closes. The bar owns the labels, not
 * the menus: the caller keeps the `Menu` objects (and their command bindings) alive.
 *
 * Menus are added to the bar's parent when first opened, so the bar must be attached to the tree
 * before it is used.
 */
class MenuBar final : public Layer {
 public:
  explicit MenuBar(std::shared_ptr<const Font> font);
  ~MenuBar() override;

  /** Adds a top-level menu. @return the index of the label. */
  int addMenu(std::string label, std::shared_ptr<Menu> menu);

  /** Forwards the window command table to every added menu. */
  void setCommands(CommandRegistry* commands);

  /** @return true while one of the menus is open. */
  [[nodiscard]] bool isOpen() const noexcept {
    return mOpen >= 0;
  }
  /** The index of the open menu, or -1. */
  [[nodiscard]] int openIndex() const noexcept {
    return mOpen;
  }
  /** Closes the open menu, if any. */
  void close();

  /** The label rectangle of @p index, in this layer's coordinates; empty when out of range. */
  [[nodiscard]] Rect labelBounds(int index) const;

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

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  struct Entry {
    std::string label;
    std::shared_ptr<Menu> menu;
    float left = 0.0f;
    float width = 0.0f;
  };

  [[nodiscard]] int indexAt(float x) const;
  void openMenu(int index);
  void closeMenu();
  void updateWidths();

  std::shared_ptr<const Font> mFont;
  std::vector<Entry> mEntries;
  CommandRegistry* mCommands = nullptr;
  int mOpen = -1;
  float mHeight = 26.0f;
  float mPadding = 10.0f;
  Color mBackground = Color::MakeRGBA8(35, 38, 46, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mHover = Color::MakeRGBA8(52, 56, 66, 255);
  std::vector<std::unique_ptr<TextPainter>> mTextPainters;
};

} // namespace coral::ui

#endif // CORAL_UI_MENUBAR_H
