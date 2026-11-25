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

#ifndef CORAL_UI_STEPPER_H
#define CORAL_UI_STEPPER_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>

namespace coral::ui {

struct Theme;

/**
 * A cross-platform numeric stepper: a minus/plus pair with a clamped model value.
 *
 * Clicking a half, or pressing Up/Right and Down/Left, nudges the value by `step` and reports through
 * `setOnChanged`. The value is clamped to `[minimum, maximum]`; a value already at a limit does not
 * report a change.
 */
class Stepper final : public Layer {
 public:
  Stepper(double minimum, double maximum, double value);

  void setRange(double minimum, double maximum);
  void setStep(double step) {
    mStep = step;
  }
  [[nodiscard]] double step() const noexcept {
    return mStep;
  }
  void setValue(double value);
  [[nodiscard]] double value() const noexcept {
    return mValue;
  }
  void setColors(Color background, Color border, Color foreground, Color accent);
  void setOnChanged(std::function<void(double)> callback) {
    mOnChanged = std::move(callback);
  }

  [[nodiscard]] bool focusable() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  void onFocusChanged(bool focused) override;
  [[nodiscard]] std::optional<AccessibleRange> accessibleRange() const override;
  bool performAccessibilityAction(AccessibilityAction action,
    std::string_view value) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "stepper";
  }
  [[nodiscard]] StateMask styleStates() const override {
    StateMask mask = Layer::styleStates();
    if (mFocused) mask |= stateFlag(ControlState::Focused);
    return mask;
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void nudge(double delta);

  Color mBackground = Color::MakeRGBA8(35, 38, 46, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  Color mMuted = Color::MakeRGBA8(130, 140, 155, 255);
  std::function<void(double)> mOnChanged;
  double mMinimum = 0.0;
  double mMaximum = 1.0;
  double mValue = 0.0;
  double mStep = 1.0;
  float mRadius = 4.0f;
  bool mFocused = false;
};

} // namespace coral::ui

#endif // CORAL_UI_STEPPER_H
