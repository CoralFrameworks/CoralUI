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

#ifndef CORAL_UI_PASSWORDFIELD_H
#define CORAL_UI_PASSWORDFIELD_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

/** A masked single-line secret field. */
class PasswordField final : public Layer {
 public:
  PasswordField(std::string placeholder, std::shared_ptr<const Font> font);

  void setText(std::string text);
  [[nodiscard]] const std::string& text() const noexcept {
    return mText;
  }
  void setPlaceholder(std::string placeholder);
  void setFont(std::shared_ptr<const Font> font);
  void setMaxLength(size_t length);
  void setOnChanged(std::function<void(const std::string&)> callback) {
    mOnChanged = std::move(callback);
  }
  void setColors(Color background, Color border, Color foreground, Color focus);

  [[nodiscard]] bool focusable() const override {
    return true;
  }
  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  void onFrame(std::chrono::steady_clock::time_point now) override;
  [[nodiscard]] bool hasRunningAnimation() const override {
    return mFocused;
  }
  bool onPointerDown(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  bool onTextInput(char32_t character) override;
  bool onTextComposition(const std::string& text) override;
  bool onTextCommit(const std::string& text) override;
  bool onTextDelete(int32_t index, uint32_t length) override;
  [[nodiscard]] bool acceptsTextInput() const override {
    return true;
  }
  [[nodiscard]] Rect imeCursorRect() const override;
  void onFocusChanged(bool focused) override;
  bool performAccessibilityAction(AccessibilityAction action,
    std::string_view value) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "password";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void syncAccessibleValue();
  void changed();
  [[nodiscard]] size_t cursorCodepoints() const;
  [[nodiscard]] size_t previousBoundary(size_t index) const;
  [[nodiscard]] size_t nextBoundary(size_t index) const;
  [[nodiscard]] float caretX() const;
  void insertText(const std::string& text);
  void paste();

  std::string mText;
  std::string mPlaceholder;
  std::string mComposition;
  std::shared_ptr<const Font> mFont;
  std::function<void(const std::string&)> mOnChanged;
  Color mBackground = Color::MakeRGBA8(45, 49, 59, 255);
  Color mBorder = Color::MakeRGBA8(70, 76, 88, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  size_t mCursor = 0;
  size_t mMaxLength = 64;
  float mPadding = 8.0f;
  float mRadius = 4.0f;
  bool mFocused = false;
  bool mCaretVisible = true;
  std::chrono::steady_clock::time_point mCaretStart{};
};

} // namespace coral::ui

#endif // CORAL_UI_PASSWORDFIELD_H
