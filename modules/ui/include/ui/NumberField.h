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

#ifndef CORAL_UI_NUMBERFIELD_H
#define CORAL_UI_NUMBERFIELD_H

#include <ui/Types.h>

#include <ui/Layer.h>


#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;
class TextInput;

/**
 * A numeric text field with a model value and an editable text representation.
 *
 * Focus lives on the field, not on its inner TextInput: the field forwards text editing to the child
 * and owns Enter/Escape/Up/Down. The value is clamped to the range; `setValue` is silent, while an
 * accepted edit reports through `setOnChanged`.
 */
class NumberField : public Layer {
 public:
  NumberField(std::shared_ptr<const Font> font, double minimum, double maximum,
    double value);
  ~NumberField() override;

  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground,
    Color accent);
  void setRange(double minimum, double maximum);
  void setStep(double step) {
    mStep = step;
  }
  void setDecimals(int decimals);
  void setValue(double value);
  [[nodiscard]] double value() const noexcept {
    return mValue;
  }
  void setOnChanged(std::function<void(double)> callback) {
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

 protected:
  explicit NumberField(std::shared_ptr<const Font> font, double minimum,
    double maximum, double value, bool steppers);

  void applyTheme(const Theme& theme) override;
  void nudge(double delta);
  void commit();

 private:
  [[nodiscard]] std::string formatValue() const;

  std::shared_ptr<const Font> mFont;
  std::shared_ptr<TextInput> mInput;
  Color mBackground = Color::MakeRGBA8(35, 38, 46, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mAccent = Color::MakeRGBA8(86, 156, 255, 255);
  std::function<void(double)> mOnChanged;
  float mPadding = 2.0f;
  float mRadius = 4.0f;
  double mMinimum = 0.0;
  double mMaximum = 1.0;
  double mValue = 0.0;
  double mStep = 1.0;
  int mDecimals = 2;
  bool mSteppers = false;
  bool mFocused = false;
};

/** A NumberField with increment/decrement affordances on its right edge. */
class SpinBox final : public NumberField {
 public:
  SpinBox(std::shared_ptr<const Font> font, double minimum, double maximum,
    double value)
      : NumberField(std::move(font), minimum, maximum, value, true) {}
};

} // namespace coral::ui

#endif // CORAL_UI_NUMBERFIELD_H
