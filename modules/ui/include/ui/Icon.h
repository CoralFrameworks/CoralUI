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

#ifndef CORAL_UI_ICON_H
#define CORAL_UI_ICON_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>

namespace graphics {
class Image;
}

namespace coral::ui {

struct Theme;

/** A square, optionally tinted image. */
class Icon final : public Layer {
 public:
  explicit Icon(std::shared_ptr<const graphics::Image> image = {},
    float size = 20.0f);

  void setImage(std::shared_ptr<const graphics::Image> image);
  [[nodiscard]] const std::shared_ptr<const graphics::Image>& image() const noexcept {
    return mImage;
  }
  void setSize(float size);
  [[nodiscard]] float size() const noexcept {
    return mSize;
  }
  void setColor(Color color);

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "icon";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::shared_ptr<const graphics::Image> mImage;
  Color mColor = Color::MakeRGBA8(226, 232, 240, 255);
  float mSize = 20.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_ICON_H
