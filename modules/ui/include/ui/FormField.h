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

#ifndef CORAL_UI_FORMFIELD_H
#define CORAL_UI_FORMFIELD_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

/** A labelled form control with optional help/error text. */
class FormField final : public Layer {
 public:
  FormField(std::string label, std::shared_ptr<Layer> content,
    std::shared_ptr<const Font> font);

  void setLabel(std::string label);
  void setHelpText(std::string text);
  void setErrorText(std::string text);
  void setContent(std::shared_ptr<Layer> content);
  [[nodiscard]] const std::shared_ptr<Layer>& content() const noexcept {
    return mContent;
  }
  void setFont(std::shared_ptr<const Font> font);

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "formfield";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::string mLabel;
  std::string mHelp;
  std::string mError;
  std::shared_ptr<Layer> mContent;
  std::shared_ptr<const Font> mFont;
  Color mLabelColor = Color::MakeRGBA8(226, 232, 240, 255);
  Color mHelpColor = Color::MakeRGBA8(148, 163, 184, 255);
  Color mErrorColor = Color::MakeRGBA8(230, 100, 100, 255);
  float mGap = 4.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_FORMFIELD_H
