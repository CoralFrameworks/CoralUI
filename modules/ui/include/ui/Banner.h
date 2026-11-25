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

#ifndef CORAL_UI_BANNER_H
#define CORAL_UI_BANNER_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

enum class BannerSeverity {
  Info,
  Success,
  Warning,
  Danger,
};

/** A cross-platform inline message bar with an optional action and dismiss control. */
class Banner final : public Layer {
 public:
  Banner(std::string text, std::shared_ptr<const Font> font,
    BannerSeverity severity = BannerSeverity::Info);

  void setText(std::string text);
  [[nodiscard]] const std::string& text() const noexcept {
    return mText;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setSeverity(BannerSeverity severity);
  [[nodiscard]] BannerSeverity severity() const noexcept {
    return mSeverity;
  }
  void setAction(std::string label, std::function<void()> callback);
  void setDismissible(bool dismissible);
  /** Optional leading icon; the banner does not own the image. */
  void setIcon(std::shared_ptr<const graphics::Image> icon);
  [[nodiscard]] const std::shared_ptr<const graphics::Image>& icon() const noexcept {
    return mIcon;
  }
  [[nodiscard]] bool dismissible() const noexcept {
    return mDismissible;
  }
  void setOnDismiss(std::function<void()> callback) {
    mOnDismiss = std::move(callback);
  }
  /** Fade/slide the banner in; call after attaching it to a window. */
  void playEnterAnimation();
  /** Fade out and hide after the transition; onDismiss runs once. */
  void playExitAnimation();

  [[nodiscard]] bool focusable() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  void onFrame(std::chrono::steady_clock::time_point now) override;
  [[nodiscard]] bool hasRunningAnimation() const override {
    return mExitPending || Layer::hasRunningAnimation();
  }
  bool onPointerDown(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  bool performAccessibilityAction(AccessibilityAction action,
    std::string_view value) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "banner";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void dismiss();

  std::string mText;
  std::shared_ptr<const Font> mFont;
  std::shared_ptr<const graphics::Image> mIcon;
  BannerSeverity mSeverity = BannerSeverity::Info;
  std::string mActionLabel;
  std::function<void()> mOnAction;
  std::function<void()> mOnDismiss;
  Color mBackground = Color::MakeRGBA8(40, 62, 92, 255);
  Color mForeground = Color::MakeRGBA8(226, 240, 255, 255);
  Color mAccent = Color::MakeRGBA8(120, 180, 255, 255);
  float mPadding = 12.0f;
  float mRadius = 5.0f;
  bool mDismissible = true;
  bool mExitPending = false;
  std::chrono::steady_clock::time_point mExitStart{};
};

} // namespace coral::ui

#endif // CORAL_UI_BANNER_H
