/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CORAL_UI_COLORBUTTON_H
#define CORAL_UI_COLORBUTTON_H

#include <ui/Types.h>
#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <string>

namespace coral::ui {

struct Theme;
class ColorPicker;
class Popup;

/** A compact colour swatch that behaves like a button. */
class ColorButton final : public Layer {
 public:
  explicit ColorButton(Color color = Color::MakeRGBA8(86, 156, 255, 255),
    std::string accessibleName = {});
  ~ColorButton() override;

  void setColor(Color color);
  [[nodiscard]] Color color() const noexcept { return mColor; }

  void setOnClick(std::function<void()> callback);

  /**
   * Enables the built-in swatch popup. Clicking the button then shows a `ColorPicker` in a `Popup`
   * anchored below the swatch; selecting a colour updates `color()` and invokes `setOnPick`.
   */
  void setPickerEnabled(bool enabled);
  [[nodiscard]] bool pickerEnabled() const noexcept { return mPickerEnabled; }
  [[nodiscard]] const std::shared_ptr<ColorPicker>& picker() const noexcept { return mPicker; }
  [[nodiscard]] const std::shared_ptr<Popup>& popup() const noexcept { return mPopup; }
  void setOnPick(std::function<void(Color)> callback);

  [[nodiscard]] int activationCount() const noexcept { return mActivations; }

  [[nodiscard]] bool focusable() const override { return true; }
  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerUp(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  void onFocusChanged(bool focused) override;

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void activate();

  Color mColor;
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  std::function<void()> mOnClick;
  std::function<void(Color)> mOnPick;
  std::shared_ptr<ColorPicker> mPicker;
  std::shared_ptr<Popup> mPopup;
  float mSize = 32.0f;
  float mRadius = 4.0f;
  bool mPickerEnabled = false;
  bool mPressed = false;
  bool mFocused = false;
  bool mHovered = false;
  int mActivations = 0;
};

} // namespace coral::ui

#endif // CORAL_UI_COLORBUTTON_H
