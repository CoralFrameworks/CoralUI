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

#ifndef CORAL_UI_TOOLBAR_H
#define CORAL_UI_TOOLBAR_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace coral::ui {

struct Theme;
class Button;
class Menu;

/**
 * A cross-platform horizontal toolbar strip.
 *
 * Items keep their measured width; an item added with `flexible=true` shares the remaining space.
 * `addSeparator()` inserts a fixed 1px divider.
 */
class Toolbar final : public Layer {
 public:
  explicit Toolbar(std::shared_ptr<const Font> font = {});

  void setHeight(float height);
  [[nodiscard]] float height() const noexcept {
    return mHeight;
  }
  void setColors(Color background, Color border);
  void addItem(std::shared_ptr<Layer> item, bool flexible = false,
    std::string overflowLabel = {}, std::function<void()> overflowAction = {});
  void addSeparator();
  void setOverflowMenu(std::shared_ptr<Menu> menu);
  [[nodiscard]] const std::shared_ptr<Menu>& overflowMenu() const noexcept {
    return mOverflowMenu;
  }
  [[nodiscard]] bool hasOverflow() const noexcept {
    return mHasOverflow;
  }
  int itemCount() const noexcept {
    return static_cast<int>(mItems.size());
  }
  void clear();

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "toolbar";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  struct Item {
    std::shared_ptr<Layer> layer;
    bool separator = false;
    bool flexible = false;
    bool visible = true;
    std::string overflowLabel;
    std::function<void()> overflowAction;
  };

  std::vector<Item> mItems;
  Color mBackground = Color::MakeRGBA8(32, 35, 42, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  float mHeight = 44.0f;
  float mPadding = 8.0f;
  float mGap = 6.0f;
  std::shared_ptr<const Font> mFont;
  std::shared_ptr<Button> mOverflowButton;
  std::shared_ptr<Menu> mOverflowMenu;
  std::shared_ptr<Menu> mAutoOverflowMenu;
  bool mHasOverflow = false;
};

} // namespace coral::ui

#endif // CORAL_UI_TOOLBAR_H
