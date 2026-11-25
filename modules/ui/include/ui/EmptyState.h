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

#ifndef CORAL_UI_EMPTYSTATE_H
#define CORAL_UI_EMPTYSTATE_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

/** A centered empty/zero-state block with a title, description and optional action. */
class EmptyState final : public Layer {
 public:
  EmptyState(std::shared_ptr<const Font> font, std::string title,
    std::string description = {}, std::shared_ptr<Layer> action = nullptr);

  void setTitle(std::string title);
  [[nodiscard]] const std::string& title() const noexcept {
    return mTitle;
  }
  void setDescription(std::string description);
  [[nodiscard]] const std::string& description() const noexcept {
    return mDescription;
  }
  void setAction(std::shared_ptr<Layer> action);
  [[nodiscard]] const std::shared_ptr<Layer>& action() const noexcept {
    return mAction;
  }
  void setContent(std::shared_ptr<Layer> content);
  [[nodiscard]] const std::shared_ptr<Layer>& content() const noexcept {
    return mContent;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color titleColor, Color descriptionColor);
  void setIcon(std::shared_ptr<const graphics::Image> icon);
  [[nodiscard]] const std::shared_ptr<const graphics::Image>& icon() const noexcept {
    return mIcon;
  }

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "emptystate";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::string mTitle;
  std::string mDescription;
  std::shared_ptr<const Font> mFont;
  std::shared_ptr<Layer> mAction;
  std::shared_ptr<Layer> mContent;
  std::shared_ptr<const graphics::Image> mIcon;
  Color mTitleColor = Color::MakeRGBA8(226, 232, 240, 255);
  Color mDescriptionColor = Color::MakeRGBA8(150, 160, 175, 255);
  float mGap = 8.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_EMPTYSTATE_H
