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

#ifndef CORAL_UI_TOAST_H
#define CORAL_UI_TOAST_H

#include <ui/Types.h>

#include <ui/Layer.h>


#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace coral::ui {

struct Theme;

/**
 * A transient, non-interactive message.
 *
 * `showFor` installs a deadline; `Layer::onFrame` (called by `Window` before drawing) hides the
 * toast after it passes and invokes the dismissal callback. Without a deadline the toast stays until
 * `hide` is called.
 */
class Toast final : public Layer {
 public:
  Toast(std::string text, std::shared_ptr<const Font> font);

  void setText(std::string text);
  [[nodiscard]] const std::string& text() const noexcept {
    return mText;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground);

  void show(std::string text);
  void showFor(std::string text, std::chrono::milliseconds duration);
  void showAt(const Point& position);
  void showAt(const Rect& area);
  void hide();
  [[nodiscard]] bool isOpen() const noexcept {
    return visible();
  }

  void setOnDismissed(std::function<void()> callback) {
    mOnDismissed = std::move(callback);
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  void onFrame(std::chrono::steady_clock::time_point now) override;

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void place(const Point& position);

  std::string mText;
  std::shared_ptr<const Font> mFont;
  Color mBackground = Color::MakeRGBA8(52, 56, 66, 245);
  Color mBorder = Color::MakeRGBA8(78, 86, 100, 255);
  Color mForeground = Color::MakeRGBA8(245, 247, 250, 255);
  float mPadding = 12.0f;
  float mRadius = 4.0f;
  std::function<void()> mOnDismissed;
  std::optional<std::chrono::steady_clock::time_point> mDeadline;
};

} // namespace coral::ui

#endif // CORAL_UI_TOAST_H
