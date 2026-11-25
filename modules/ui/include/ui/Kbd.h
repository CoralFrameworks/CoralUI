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

#ifndef CORAL_UI_KBD_H
#define CORAL_UI_KBD_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

/** A compact keyboard-shortcut key cap. */
class Kbd final : public Layer {
 public:
  Kbd(std::string label, std::shared_ptr<const Font> font);

  void setLabel(std::string label);
  [[nodiscard]] const std::string& label() const noexcept {
    return mLabel;
  }
  void setFont(std::shared_ptr<const Font> font);

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "kbd";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::string mLabel;
  std::shared_ptr<const Font> mFont;
  Color mBackground = Color::MakeRGBA8(45, 49, 59, 255);
  Color mBorder = Color::MakeRGBA8(90, 96, 108, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  float mPaddingX = 6.0f;
  float mPaddingY = 3.0f;
  float mRadius = 4.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_KBD_H
