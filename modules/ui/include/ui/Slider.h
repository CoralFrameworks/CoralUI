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

#ifndef CORAL_UI_SLIDER_H
#define CORAL_UI_SLIDER_H

#include <ui/Types.h>

#include <ui/Layer.h>


#include <functional>
#include <memory>
#include <string>

namespace coral::ui {

class UndoStack;

struct Theme;

/**
 * A numeric slider with a label and a formatted current value.
 *
 * A drag brackets `setOnEditBegin`/`setOnEditEnd` so an owner can record one undo step; the value
 * callback fires for drag updates and keyboard steps. `setValue` is the model-driven path and is
 * silent.
 */
class Slider final : public Layer {
 public:
  Slider(std::string label, std::shared_ptr<const Font> font, double minimum,
    double maximum, double value);

  void setLabel(std::string label);
  [[nodiscard]] const std::string& label() const noexcept {
    return mLabel;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground,
    Color accent);
  void setRange(double minimum, double maximum);
  void setStep(double step) {
    mStep = step;
  }
  void setValueFormat(std::string format) {
    mFormat = std::move(format);
  }
  void setValue(double value);
  [[nodiscard]] double value() const noexcept {
    return mValue;
  }
  void setOnChanged(std::function<void(double)> callback) {
    mOnChanged = std::move(callback);
  }
  void setOnEditBegin(std::function<void()> callback) {
    mOnEditBegin = std::move(callback);
  }
  void setOnEditEnd(std::function<void()> callback) {
    mOnEditEnd = std::move(callback);
  }

  /**
   * Brackets a drag with one undo transaction on @p stack.
   *
   * The owner still records the value actions (its onChange callback knows the old/new value); the
   * slider only guarantees that a whole drag becomes one undo step instead of one per pixel.
   */
  void setUndoStack(UndoStack* stack, std::string label = "Change value");

  [[nodiscard]] bool focusable() const override {
    return true;
  }
  [[nodiscard]] bool capturesPointerOnPress() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerUp(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  void onFocusChanged(bool focused) override;
  [[nodiscard]] const char* defaultStyleName() const override { return "slider"; }
  [[nodiscard]] StateMask styleStates() const override {
    StateMask mask = Layer::styleStates();
    if (mDragging) mask |= stateFlag(ControlState::Pressed);
    if (mFocused) mask |= stateFlag(ControlState::Focused);
    return mask;
  }

  /** The slider reports its numeric range so a bridge can expose a real range pattern. */
  [[nodiscard]] std::optional<AccessibleRange> accessibleRange() const override;

 protected:
  void applyTheme(const Theme& theme) override;

  bool performAccessibilityAction(AccessibilityAction action, std::string_view value) override;

 private:
  void updateFromPointer(float x);
  void nudge(double delta);
  void notify();

  std::string mLabel;
  std::string mFormat = "%.2f";
  std::shared_ptr<const Font> mFont;
  Color mBackground = Color::MakeRGBA8(38, 42, 51, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mTrack = Color::MakeRGBA8(62, 68, 80, 255);
  Color mKnob = Color::MakeRGBA8(245, 247, 250, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mAccent = Color::MakeRGBA8(86, 156, 255, 255);
  float mPadding = 10.0f;
  float mRadius = 4.0f;
  float mTrackHeight = 4.0f;
  float mKnobRadius = 6.0f;
  double mMinimum = 0.0;
  double mMaximum = 1.0;
  double mValue = 0.0;
  double mStep = 0.0;
  bool mDragging = false;
  bool mFocused = false;
  std::function<void(double)> mOnChanged;
  std::function<void()> mOnEditBegin;
  std::function<void()> mOnEditEnd;
  UndoStack* mUndoStack = nullptr;
  std::string mUndoLabel = "Change value";
};

} // namespace coral::ui

#endif // CORAL_UI_SLIDER_H
