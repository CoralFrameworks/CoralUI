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

#ifndef CORAL_UI_RADIOBUTTON_H
#define CORAL_UI_RADIOBUTTON_H

#include <ui/Types.h>

#include <ui/Layer.h>


#include <functional>
#include <memory>
#include <string>

namespace coral::ui {

struct Theme;

/**
 * A radio button.
 *
 * The control owns one value only; group exclusivity is the owner's policy, which keeps it usable in
 * a small form without imposing a panel/group container. `setSelected` is silent, activation reports
 * through `setOnSelected`.
 */
class RadioButton final : public Layer {
 public:
  RadioButton(std::string label, std::shared_ptr<const Font> font,
    bool selected = false);

  void setLabel(std::string label);
  [[nodiscard]] const std::string& label() const noexcept {
    return mLabel;
  }
  void setColors(Color background, Color foreground,
    Color accent);
  void setSelected(bool selected);
  [[nodiscard]] bool selected() const noexcept {
    return mSelected;
  }
  void setOnSelected(std::function<void()> callback) {
    mOnSelected = std::move(callback);
  }

  [[nodiscard]] bool focusable() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerUp(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  void onFocusChanged(bool focused) override;
  [[nodiscard]] const char* defaultStyleName() const override { return "radio"; }
  [[nodiscard]] StateMask styleStates() const override {
    StateMask mask = Layer::styleStates();
    if (mSelected) mask |= stateFlag(ControlState::Checked);
    if (mPressed) mask |= stateFlag(ControlState::Pressed);
    if (mFocused) mask |= stateFlag(ControlState::Focused);
    return mask;
  }

 protected:
  void applyTheme(const Theme& theme) override;

  bool performAccessibilityAction(AccessibilityAction action, std::string_view value) override;

 private:
  void activate();

  std::string mLabel;
  std::shared_ptr<const Font> mFont;
  Color mBackground = Color::MakeRGBA8(35, 38, 46, 255);
  Color mBorder = Color::MakeRGBA8(100, 110, 126, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mAccent = Color::MakeRGBA8(86, 156, 255, 255);
  std::function<void()> mOnSelected;
  float mGap = 8.0f;
  float mRadius = 4.0f;
  bool mSelected = false;
  bool mPressed = false;
  bool mFocused = false;
};

} // namespace coral::ui

#endif // CORAL_UI_RADIOBUTTON_H
