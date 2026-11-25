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

#ifndef CORAL_UI_CARD_H
#define CORAL_UI_CARD_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

/** A cross-platform surface container with an optional title and one content child. */
class Card final : public Layer {
 public:
  Card(std::shared_ptr<const Font> font, std::string title = {},
    std::shared_ptr<Layer> content = nullptr);

  void setTitle(std::string title);
  [[nodiscard]] const std::string& title() const noexcept {
    return mTitle;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setContent(std::shared_ptr<Layer> content);
  [[nodiscard]] const std::shared_ptr<Layer>& content() const noexcept {
    return mContent;
  }
  /** Optional control placed at the right edge of the title area. */
  void setHeaderAction(std::shared_ptr<Layer> action);
  [[nodiscard]] const std::shared_ptr<Layer>& headerAction() const noexcept {
    return mHeaderAction;
  }
  void setPadding(float padding);
  void setHeaderHeight(float height);
  void setRadius(float radius);
  void setColors(Color background, Color border, Color foreground);

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "card";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::string mTitle;
  std::shared_ptr<const Font> mFont;
  std::shared_ptr<Layer> mContent;
  std::shared_ptr<Layer> mHeaderAction;
  Color mBackground = Color::MakeRGBA8(45, 49, 59, 255);
  Color mBorder = Color::MakeRGBA8(70, 76, 88, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  float mPadding = 12.0f;
  float mHeaderHeight = 30.0f;
  float mRadius = 6.0f;
  Shadow mShadow{};
};

} // namespace coral::ui

#endif // CORAL_UI_CARD_H
