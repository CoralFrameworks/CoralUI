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

#ifndef CORAL_UI_TITLEBAR_H
#define CORAL_UI_TITLEBAR_H

#include <ui/Layer.h>
#include <ui/Types.h>

#include <functional>
#include <memory>
#include <string>

namespace coral::ui {

class Button;
class Flex;
class TextLayer;

/**
 * A client-side title bar for `Window::Decorations::Client`.
 *
 * The framework already maps a press in the caption band and the resize border to the platform's
 * native move/resize. What it deliberately does not do is choose a title-bar design for the product:
 * this widget is the reusable default. Put it at the top of the window (y = 0) and set
 * `Window::setChromeMetrics({.titleBarHeight = 40, ...})`; the window's hit test then treats the
 * caption and the three buttons exactly like a native frame, while the buttons deliver their own
 * clicks to the callbacks.
 */
class TitleBar final : public Layer {
 public:
  TitleBar(std::string title, std::shared_ptr<const Font> font,
    Color titleColor = Color::MakeRGBA8(226, 232, 240, 255));

  void setTitle(std::string title);
  [[nodiscard]] const std::string& title() const noexcept;

  void setOnMinimize(std::function<void()> callback);
  void setOnMaximize(std::function<void()> callback);
  void setOnClose(std::function<void()> callback);

  /** Hides a control that the window does not offer (for example a fixed-size dialog). */
  void setShowMinimize(bool show);
  void setShowMaximize(bool show);
  void setShowClose(bool show);

  Size measure(const Size& available) override;
  void layout() override;

  [[nodiscard]] const char* defaultStyleName() const override {
    return "titlebar";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::shared_ptr<Flex> mBar;
  std::shared_ptr<TextLayer> mTitle;
  std::shared_ptr<Button> mMinimize;
  std::shared_ptr<Button> mMaximize;
  std::shared_ptr<Button> mClose;
};

} // namespace coral::ui

#endif // CORAL_UI_TITLEBAR_H
