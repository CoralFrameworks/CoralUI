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

#ifndef CORAL_UI_SECTIONHEADER_H
#define CORAL_UI_SECTIONHEADER_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

/** A section title with optional subtitle and trailing action. */
class SectionHeader final : public Layer {
 public:
  SectionHeader(std::string title, std::shared_ptr<const Font> font,
    std::string subtitle = {}, std::shared_ptr<Layer> action = nullptr);

  void setTitle(std::string title);
  [[nodiscard]] const std::string& title() const noexcept {
    return mTitle;
  }
  void setSubtitle(std::string subtitle);
  void setAction(std::shared_ptr<Layer> action);
  void setFont(std::shared_ptr<const Font> font);

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "sectionheader";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::string mTitle;
  std::string mSubtitle;
  std::shared_ptr<const Font> mFont;
  std::shared_ptr<Layer> mAction;
  Color mTitleColor = Color::MakeRGBA8(226, 232, 240, 255);
  Color mSubtitleColor = Color::MakeRGBA8(148, 163, 184, 255);
  float mGap = 10.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_SECTIONHEADER_H
