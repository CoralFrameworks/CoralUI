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

#ifndef CORAL_UI_COMBOBOX_H
#define CORAL_UI_COMBOBOX_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace coral::ui {

struct Theme;
class Menu;
class TextInput;

/**
 * A cross-platform editable combo box: a text field with a dropdown arrow.
 *
 * Typing is free-form; choosing a menu item replaces the text and reports `(index, text)`.
 * The arrow region opens the menu and the rest of the control focuses the embedded text field.
 */
class ComboBox final : public Layer {
 public:
  using ChangeHandler = std::function<void(int, std::string_view)>;

  ComboBox(std::vector<std::string> options, std::shared_ptr<const Font> font,
    std::string text = {});
  ~ComboBox() override;

  void setOptions(std::vector<std::string> options);
  [[nodiscard]] const std::vector<std::string>& options() const noexcept {
    return mOptions;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground, Color accent);
  void setPlaceholder(std::string placeholder);
  void setText(std::string text);
  [[nodiscard]] std::string text() const;
  void setSelectedIndex(int index);
  [[nodiscard]] int selectedIndex() const noexcept {
    return mSelected;
  }
  void setOnChanged(ChangeHandler callback) {
    mOnChanged = std::move(callback);
  }

  [[nodiscard]] bool focusable() const override {
    return true;
  }
  [[nodiscard]] bool acceptsTextInput() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  bool onTextInput(char32_t character) override;
  bool onTextComposition(const std::string& text) override;
  bool onTextCommit(const std::string& text) override;
  bool onTextDelete(int32_t index, uint32_t length) override;
  void onFocusChanged(bool focused) override;
  bool performAccessibilityAction(AccessibilityAction action,
    std::string_view value) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "combobox";
  }
  [[nodiscard]] StateMask styleStates() const override {
    StateMask mask = Layer::styleStates();
    if (mFocused) mask |= stateFlag(ControlState::Focused);
    return mask;
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void rebuildMenu();
  void openMenu();
  void choose(int index);
  void syncAccessibleText();

  std::vector<std::string> mOptions;
  std::shared_ptr<const Font> mFont;
  std::shared_ptr<TextInput> mInput;
  std::shared_ptr<Menu> mMenu;
  std::shared_ptr<bool> mAlive = std::make_shared<bool>(true);
  ChangeHandler mOnChanged;
  Color mBackground = Color::MakeRGBA8(35, 38, 46, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  float mPadding = 6.0f;
  float mRadius = 4.0f;
  int mSelected = -1;
  bool mFocused = false;
};

} // namespace coral::ui

#endif // CORAL_UI_COMBOBOX_H
