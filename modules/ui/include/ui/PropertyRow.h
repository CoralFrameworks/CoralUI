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

#ifndef CORAL_UI_PROPERTYROW_H
#define CORAL_UI_PROPERTYROW_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

/** A settings-style label on the left and value text on the right. */
class PropertyRow final : public Layer {
 public:
  PropertyRow(std::string label, std::string value, std::shared_ptr<const Font> font);

  void setLabel(std::string label);
  void setValue(std::string value);
  [[nodiscard]] const std::string& value() const noexcept {
    return mValue;
  }
  void setHelpText(std::string text);
  void setFont(std::shared_ptr<const Font> font);
  void setLabelWidth(float width);

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "propertyrow";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::string mLabel;
  std::string mValue;
  std::string mHelp;
  std::shared_ptr<const Font> mFont;
  Color mLabelColor = Color::MakeRGBA8(148, 163, 184, 255);
  Color mValueColor = Color::MakeRGBA8(226, 232, 240, 255);
  Color mHelpColor = Color::MakeRGBA8(148, 163, 184, 255);
  float mLabelWidth = 110.0f;
  float mGap = 8.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_PROPERTYROW_H
