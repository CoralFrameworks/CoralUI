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

#ifndef CORAL_UI_SEARCHFIELD_H
#define CORAL_UI_SEARCHFIELD_H

#include <ui/Types.h>

#include <ui/Layer.h>


#include <functional>
#include <memory>
#include <string>

namespace coral::ui {

struct Theme;
class TextInput;

/**
 * A single-line search box: magnifier, placeholder, and a clear affordance.
 *
 * Like NumberField, the outer layer owns focus and forwards editing to an embedded TextInput. Enter
 * invokes `setOnSearch`; the clear button empties the field and reports the change.
 */
class SearchField final : public Layer {
 public:
  SearchField(std::string placeholder, std::shared_ptr<const Font> font);
  ~SearchField() override;

  void setText(std::string text);
  [[nodiscard]] const std::string& text() const;
  void setPlaceholder(std::string placeholder);
  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground,
    Color accent);
  void clear();
  void setOnChanged(std::function<void(const std::string&)> callback) {
    mOnChanged = std::move(callback);
  }
  void setOnSearch(std::function<void(const std::string&)> callback) {
    mOnSearch = std::move(callback);
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
  [[nodiscard]] const char* defaultStyleName() const override {
    return "searchfield";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void notifyChanged();

  std::shared_ptr<const Font> mFont;
  std::shared_ptr<TextInput> mInput;
  Color mBackground = Color::MakeRGBA8(35, 38, 46, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mMuted = Color::MakeRGBA8(148, 163, 184, 255);
  Color mAccent = Color::MakeRGBA8(86, 156, 255, 255);
  std::function<void(const std::string&)> mOnChanged;
  std::function<void(const std::string&)> mOnSearch;
  float mRadius = 4.0f;
  bool mFocused = false;
};

} // namespace coral::ui

#endif // CORAL_UI_SEARCHFIELD_H
