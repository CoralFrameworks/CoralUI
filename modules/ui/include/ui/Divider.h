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

#ifndef CORAL_UI_DIVIDER_H
#define CORAL_UI_DIVIDER_H

#include <ui/Types.h>

#include <ui/Layer.h>

namespace coral::ui {

struct Theme;

/** A 1px horizontal or vertical divider that follows the theme border colour. */
class Divider final : public Layer {
 public:
  enum class Orientation {
    Horizontal,
    Vertical,
  };

  explicit Divider(Orientation orientation = Orientation::Horizontal);

  void setOrientation(Orientation orientation);
  [[nodiscard]] Orientation orientation() const noexcept {
    return mOrientation;
  }
  void setColor(Color color);
  void setThickness(float thickness);
  /** Inset from both ends in the main axis. */
  void setInset(float inset);

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "divider";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  Orientation mOrientation = Orientation::Horizontal;
  Color mColor = Color::MakeRGBA8(62, 67, 78, 255);
  float mThickness = 1.0f;
  float mInset = 0.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_DIVIDER_H
