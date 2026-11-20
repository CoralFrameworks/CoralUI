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

#ifndef CORAL_UI_COLORINPUT_H
#define CORAL_UI_COLORINPUT_H

#include <ui/Types.h>
#include <ui/Layer.h>

#include <core/observer/Observation.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

struct Theme;
class TextInput;

/**
 * A hex + RGB colour editor.
 *
 * The hex field accepts `#RGB`, `#RRGGBB` and `#RRGGBBAA` (the `#` is optional); the three numeric
 * fields accept 0–255. Invalid intermediate text is ignored rather than replacing the current colour,
 * so typing is not fought by the model. The four fields are ordinary focusable `TextInput`s, which
 * keeps pointer selection and tab focus consistent with the rest of the controls.
 */
class ColorInput final : public Layer {
 public:
  ColorInput(std::shared_ptr<const Font> font, Color color);
  ~ColorInput() override;

  void setColor(Color color);
  [[nodiscard]] Color color() const noexcept { return mColor; }
  void setOnChanged(std::function<void(Color)> callback);

  void setFont(std::shared_ptr<const Font> font);
  Size measure(const Size& available) override;
  void layout() override;

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void syncFields();
  void readHex();
  void readRgb();
  void notify();
  void setFieldText(TextInput& field, const std::string& text);

  std::shared_ptr<TextInput> mHex;
  std::shared_ptr<TextInput> mRed;
  std::shared_ptr<TextInput> mGreen;
  std::shared_ptr<TextInput> mBlue;
  std::vector<observer::Observation> mObservations;
  std::function<void(Color)> mOnChanged;
  Color mColor;
  bool mSyncing = false;
};

} // namespace coral::ui

#endif // CORAL_UI_COLORINPUT_H
