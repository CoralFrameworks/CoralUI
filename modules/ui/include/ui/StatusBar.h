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

#ifndef CORAL_UI_STATUSBAR_H
#define CORAL_UI_STATUSBAR_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>
#include <string>
#include <vector>
#include <utility>

namespace coral::ui {

struct Theme;

/** A compact left/right status strip, typically at the bottom of a window. */
class StatusBar final : public Layer {
 public:
  StatusBar(std::shared_ptr<const Font> font, std::string left = {}, std::string right = {});

  void setLeftText(std::string text);
  [[nodiscard]] const std::string& leftText() const noexcept {
    return mLeft;
  }
  void setRightText(std::string text);
  [[nodiscard]] const std::string& rightText() const noexcept {
    return mRight;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground, Color border);
  void setHeight(float height);
  [[nodiscard]] float height() const noexcept {
    return mHeight;
  }
  /** Optional control placed at the far right, before the right text. */
  void setTrailingItem(std::shared_ptr<Layer> item);
  void addTrailingItem(std::shared_ptr<Layer> item);
  void addLeadingItem(std::shared_ptr<Layer> item);
  [[nodiscard]] int leadingItemCount() const noexcept {
    return static_cast<int>(mLeadingItems.size());
  }
  [[nodiscard]] int trailingItemCount() const noexcept {
    return static_cast<int>(mTrailingItems.size());
  }

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "statusbar";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::string mLeft;
  std::string mRight;
  std::shared_ptr<const Font> mFont;
  std::vector<std::shared_ptr<Layer>> mLeadingItems;
  std::vector<std::shared_ptr<Layer>> mTrailingItems;
  Color mBackground = Color::MakeRGBA8(28, 30, 36, 255);
  Color mForeground = Color::MakeRGBA8(170, 180, 195, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  float mHeight = 24.0f;
  float mPadding = 10.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_STATUSBAR_H
