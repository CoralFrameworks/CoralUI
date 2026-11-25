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

#ifndef CORAL_UI_EXPANDER_H
#define CORAL_UI_EXPANDER_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

/** A cross-platform disclosure section: a clickable header and collapsible content. */
class Expander final : public Layer {
 public:
  Expander(std::string title, std::shared_ptr<const Font> font,
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
  void setColors(Color headerBackground, Color border, Color foreground, Color accent);
  void setExpanded(bool expanded);
  [[nodiscard]] bool expanded() const noexcept {
    return mExpanded;
  }
  void setHeaderHeight(float height);
  [[nodiscard]] float headerHeight() const noexcept {
    return mHeaderHeight;
  }
  void setAnimated(bool animated) {
    mAnimated = animated;
  }
  [[nodiscard]] bool animated() const noexcept {
    return mAnimated;
  }
  void setOnChanged(std::function<void(bool)> callback) {
    mOnChanged = std::move(callback);
  }

  [[nodiscard]] bool focusable() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  void onFocusChanged(bool focused) override;
  void onFrame(std::chrono::steady_clock::time_point now) override;
  [[nodiscard]] bool hasRunningAnimation() const override {
    return mAnimating;
  }
  bool performAccessibilityAction(AccessibilityAction action,
    std::string_view value) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "expander";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void toggle();

  std::string mTitle;
  std::shared_ptr<const Font> mFont;
  std::shared_ptr<Layer> mContent;
  std::function<void(bool)> mOnChanged;
  Color mHeaderBackground = Color::MakeRGBA8(45, 49, 59, 255);
  Color mBorder = Color::MakeRGBA8(70, 76, 88, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mAccent = Color::MakeRGBA8(86, 156, 255, 255);
  float mHeaderHeight = 32.0f;
  float mPadding = 10.0f;
  float mRadius = 4.0f;
  bool mExpanded = true;
  bool mFocused = false;
  bool mAnimated = false;
  bool mAnimating = false;
  float mExpansion = 1.0f;
  float mAnimationFrom = 1.0f;
  float mAnimationTo = 1.0f;
  std::chrono::steady_clock::time_point mAnimationStart{};
};

} // namespace coral::ui

#endif // CORAL_UI_EXPANDER_H
