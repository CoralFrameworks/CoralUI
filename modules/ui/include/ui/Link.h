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

#ifndef CORAL_UI_LINK_H
#define CORAL_UI_LINK_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

/** A keyboard-activatable text link with hover and focus feedback. */
class Link final : public Layer {
 public:
  Link(std::string text, std::shared_ptr<const Font> font,
    std::function<void()> onActivate = {});

  void setText(std::string text);
  [[nodiscard]] const std::string& text() const noexcept {
    return mText;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setOnActivate(std::function<void()> callback) {
    mOnActivate = std::move(callback);
  }
  void setColors(Color color, Color hover, Color focus);

  [[nodiscard]] bool focusable() const override {
    return true;
  }
  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerLeave() override;
  bool onKeyDown(const KeyEvent& event) override;
  void onFocusChanged(bool focused) override;
  bool performAccessibilityAction(AccessibilityAction action,
    std::string_view value) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "link";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void activate();

  std::string mText;
  std::shared_ptr<const Font> mFont;
  std::function<void()> mOnActivate;
  Color mColor = Color::MakeRGBA8(86, 156, 255, 255);
  Color mHoverColor = Color::MakeRGBA8(120, 180, 255, 255);
  Color mFocusColor = Color::MakeRGBA8(160, 200, 255, 255);
  bool mHovered = false;
  bool mFocused = false;
};

} // namespace coral::ui

#endif // CORAL_UI_LINK_H
