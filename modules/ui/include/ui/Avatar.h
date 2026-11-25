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

#ifndef CORAL_UI_AVATAR_H
#define CORAL_UI_AVATAR_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

/** A circular identity avatar with initials and an optional image face. */
class Avatar final : public Layer {
 public:
  Avatar(std::string initials, std::shared_ptr<const Font> font, float size = 32.0f);

  void setInitials(std::string initials);
  [[nodiscard]] const std::string& initials() const noexcept {
    return mInitials;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setImage(std::shared_ptr<const graphics::Image> image);
  [[nodiscard]] const std::shared_ptr<const graphics::Image>& image() const noexcept {
    return mImage;
  }
  void setSize(float size);
  [[nodiscard]] float size() const noexcept {
    return mSide;
  }
  void setColors(Color background, Color foreground);
  void setStatusVisible(bool visible);
  [[nodiscard]] bool statusVisible() const noexcept {
    return mStatusVisible;
  }
  void setStatusColor(Color color);

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "avatar";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::string mInitials;
  std::shared_ptr<const Font> mFont;
  std::shared_ptr<const graphics::Image> mImage;
  Color mBackground = Color::MakeRGBA8(70, 90, 120, 255);
  Color mForeground = Color::MakeRGBA8(240, 245, 255, 255);
  float mSide = 32.0f;
  Color mStatusColor = Color::MakeRGBA8(80, 200, 120, 255);
  bool mStatusVisible = false;
};

} // namespace coral::ui

#endif // CORAL_UI_AVATAR_H
