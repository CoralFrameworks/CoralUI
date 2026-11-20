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

#ifndef CORAL_UI_COLORLAYER_H
#define CORAL_UI_COLORLAYER_H

#include <ui/Types.h>

#include <ui/Layer.h>


namespace coral::ui {

/** A solid rectangle. The background, a toolbar strip and a divider are all this layer. */
class ColorLayer final : public Layer {
 public:
  explicit ColorLayer(Color color = Color::MakeRGBA8(0, 0, 0, 0));

  void setColor(Color color);
  [[nodiscard]] Color color() const noexcept {
    return mColor;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;

 protected:
  [[nodiscard]] bool cachesOwnPaint() const override { return true; }

 private:
  Color mColor;
};

} // namespace coral::ui

#endif // CORAL_UI_COLORLAYER_H
