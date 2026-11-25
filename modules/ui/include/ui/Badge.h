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

#ifndef CORAL_UI_BADGE_H
#define CORAL_UI_BADGE_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

/** A small non-interactive pill for status/count labels. */
class Badge final : public Layer {
 public:
  Badge(std::string text, std::shared_ptr<const Font> font);

  void setText(std::string text);
  [[nodiscard]] const std::string& text() const noexcept {
    return mText;
  }
  [[nodiscard]] bool dot() const noexcept {
    return mDot;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground);
  /** Optional leading icon; the badge does not own the image. */
  void setIcon(std::shared_ptr<const graphics::Image> icon);
  [[nodiscard]] const std::shared_ptr<const graphics::Image>& icon() const noexcept {
    return mIcon;
  }
  /** Draw a small status dot instead of a text pill. */
  void setDot(bool dot);

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "badge";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::string mText;
  std::shared_ptr<const Font> mFont;
  std::shared_ptr<const graphics::Image> mIcon;
  Color mBackground = Color::MakeRGBA8(86, 156, 255, 255);
  Color mForeground = Color::MakeRGBA8(255, 255, 255, 255);
  float mPadding = 7.0f;
  bool mDot = false;
};

} // namespace coral::ui

#endif // CORAL_UI_BADGE_H
