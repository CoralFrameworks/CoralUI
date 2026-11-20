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

#ifndef CORAL_UI_POPUP_H
#define CORAL_UI_POPUP_H

#include <ui/Types.h>
#include <ui/Theme.h>

#include <ui/Layer.h>


#include <memory>
#include <string>

namespace coral::ui {

struct Theme;

/**
 * A floating panel around arbitrary content.
 *
 * The popup itself consumes clicks on its padding; its child content keeps normal hit testing. It is
 * a high-z-index layer, but it does not install itself into a window: the owner adds it to the root
 * and calls `showAt`, which keeps lifetime and focus policy with the page.
 */
class Popup final : public Layer {
 public:
  explicit Popup(std::shared_ptr<Layer> content = nullptr);

  void setContent(std::shared_ptr<Layer> content);
  [[nodiscard]] const std::shared_ptr<Layer>& content() const noexcept {
    return mContent;
  }
  void setColors(Color background, Color border);

  void showAt(const Point& position);
  void showAt(const Rect& anchor);
  void hide();
  [[nodiscard]] bool isOpen() const noexcept {
    return visible();
  }

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::shared_ptr<Layer> mContent;
  Color mBackground = Color::MakeRGBA8(45, 49, 59, 255);
  Color mBorder = Color::MakeRGBA8(72, 80, 94, 255);
  Shadow mShadow;
  float mPadding = 8.0f;
  float mRadius = 4.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_POPUP_H
