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

#ifndef CORAL_UI_MULTISELECT_H
#define CORAL_UI_MULTISELECT_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

struct Theme;
class Menu;

/** A compact multi-choice dropdown with a checkbox menu. */
class MultiSelect final : public Layer {
 public:
  using ChangeHandler = std::function<void(std::vector<int>)>;

  MultiSelect(std::vector<std::string> options, std::shared_ptr<const Font> font);
  ~MultiSelect() override;

  void setOptions(std::vector<std::string> options);
  [[nodiscard]] const std::vector<std::string>& options() const noexcept {
    return mOptions;
  }
  void setSelectedIndices(std::vector<int> indices);
  [[nodiscard]] const std::vector<int>& selectedIndices() const noexcept {
    return mSelected;
  }
  void setPlaceholder(std::string placeholder);
  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color border, Color foreground, Color muted, Color accent);
  void setOnChanged(ChangeHandler handler) {
    mOnChanged = std::move(handler);
  }

  [[nodiscard]] std::string summary() const;
  [[nodiscard]] bool focusable() const override {
    return true;
  }
  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  void onFocusChanged(bool focused) override;
  bool performAccessibilityAction(AccessibilityAction action,
    std::string_view value) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "multiselect";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void rebuildMenu();
  void openMenu();
  void toggle(int index);

  std::vector<std::string> mOptions;
  std::vector<int> mSelected;
  std::shared_ptr<const Font> mFont;
  std::shared_ptr<Menu> mMenu;
  std::shared_ptr<bool> mAlive = std::make_shared<bool>(true);
  ChangeHandler mOnChanged;
  std::string mPlaceholder = "Select";
  Color mBackground = Color::MakeRGBA8(45, 49, 59, 255);
  Color mBorder = Color::MakeRGBA8(70, 76, 88, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mMuted = Color::MakeRGBA8(150, 160, 175, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  float mPadding = 8.0f;
  float mRadius = 4.0f;
  bool mFocused = false;
};

} // namespace coral::ui

#endif // CORAL_UI_MULTISELECT_H
