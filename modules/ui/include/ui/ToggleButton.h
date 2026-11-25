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

#ifndef CORAL_UI_TOGGLEBUTTON_H
#define CORAL_UI_TOGGLEBUTTON_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

/** A button that keeps a selected/checked state, like a toolbar toggle. */
class ToggleButton final : public Layer {
 public:
  ToggleButton(std::string label, std::shared_ptr<const Font> font, bool selected = false);

  void setLabel(std::string label);
  [[nodiscard]] const std::string& label() const noexcept {
    return mLabel;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setSelected(bool selected);
  [[nodiscard]] bool selected() const noexcept {
    return mSelected;
  }
  void setOnChanged(std::function<void(bool)> callback) {
    mOnChanged = std::move(callback);
  }
  void setColors(Color background, Color selectedBackground, Color foreground,
    Color selectedForeground, Color border, Color focus);

  [[nodiscard]] bool focusable() const override {
    return true;
  }
  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerLeave() override;
  bool onKeyDown(const KeyEvent& event) override;
  void onFocusChanged(bool focused) override;
  bool performAccessibilityAction(AccessibilityAction action,
    std::string_view value) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "togglebutton";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void toggle();

  std::string mLabel;
  std::shared_ptr<const Font> mFont;
  std::function<void(bool)> mOnChanged;
  Color mBackground = Color::MakeRGBA8(52, 56, 66, 255);
  Color mSelectedBackground = Color::MakeRGBA8(86, 156, 255, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mSelectedForeground = Color::MakeRGBA8(255, 255, 255, 255);
  Color mBorder = Color::MakeRGBA8(70, 76, 88, 255);
  Color mFocus = Color::MakeRGBA8(120, 180, 255, 255);
  float mPaddingX = 16.0f;
  float mPaddingY = 10.0f;
  float mRadius = 4.0f;
  bool mSelected = false;
  bool mHovered = false;
  bool mFocused = false;
};

} // namespace coral::ui

#endif // CORAL_UI_TOGGLEBUTTON_H
