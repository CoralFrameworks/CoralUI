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

#ifndef CORAL_UI_BUTTON_H
#define CORAL_UI_BUTTON_H

#include <ui/Types.h>

#include <ui/Layer.h>


#include <functional>
#include <memory>
#include <string>

namespace coral::ui {

struct Theme;
class TextPainter;

/** The first real widget: a focusable, clickable button with a centred label. */
class Button final : public Layer {
 public:
  Button(std::string label, std::shared_ptr<const Font> font);
  ~Button() override;

  void setLabel(std::string label);
  [[nodiscard]] const std::string& label() const noexcept {
    return mLabel;
  }

  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground);
  void setOnClick(std::function<void()> callback);

  /** Disabled buttons ignore pointer/key activation and draw a dimmed label. */
  /** A disabled button drops its pressed state (the base class stores the flag). */
  void onEnabledChanged(bool enabled) override;

  [[nodiscard]] bool focusable() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerUp(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerLeave() override;
  [[nodiscard]] StateMask styleStates() const override;
  [[nodiscard]] const char* defaultStyleName() const override { return "button"; }
  bool onKeyDown(const KeyEvent& event) override;
  void onFocusChanged(bool focused) override;

  [[nodiscard]] int activationCount() const noexcept {
    return mActivations;
  }

 protected:
  void applyTheme(const Theme& theme) override;

  bool performAccessibilityAction(AccessibilityAction action, std::string_view value) override;

 private:
  void activate();

  std::string mLabel;
  std::shared_ptr<const Font> mFont;
  Color mBackground = Color::MakeRGBA8(52, 56, 66, 255);
  Color mPressedBackground = Color::MakeRGBA8(75, 85, 99, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  std::function<void()> mOnClick;
  float mPaddingX = 16.0f;
  float mPaddingY = 10.0f;
  float mRadius = 4.0f;
  bool mHover = false;
  bool mPressed = false;
  bool mFocused = false;
  int mActivations = 0;
  std::unique_ptr<TextPainter> mTextPainter;
};

} // namespace coral::ui

#endif // CORAL_UI_BUTTON_H
