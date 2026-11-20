/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CORAL_UI_ICONBUTTON_H
#define CORAL_UI_ICONBUTTON_H

#include <ui/Types.h>
#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <string>

namespace coral::ui {

struct Theme;
class IconTheme;

enum class IconButtonSize {
  Compact,
  Regular,
  Large,
};

enum class IconButtonVariant {
  Standard,
  Ghost,
};

/**
 * A compact, focusable button that draws an `Image` icon instead of a label.
 *
 * The accessible name is separate from the pixels: an icon-only control still needs a name for
 * screen readers and tests. The button owns no image data; callers keep the `Image` alive.
 */
class IconButton final : public Layer {
 public:
  IconButton(std::shared_ptr<const Image> icon, std::string accessibleName = {},
    IconButtonSize size = IconButtonSize::Regular,
    IconButtonVariant variant = IconButtonVariant::Standard);
  ~IconButton() override;

  void setIcon(std::shared_ptr<const Image> icon);
  [[nodiscard]] const std::shared_ptr<const Image>& icon() const noexcept {
    return mIcon;
  }
  void setIconTheme(std::shared_ptr<const IconTheme> theme, std::string name);
  [[nodiscard]] const std::shared_ptr<const IconTheme>& iconTheme() const noexcept {
    return mIconTheme;
  }
  [[nodiscard]] const std::string& iconName() const noexcept { return mIconName; }
  void setSize(IconButtonSize size);
  void setVariant(IconButtonVariant variant);
  void setIconSize(float size);

  void setOnClick(std::function<void()> callback);

  [[nodiscard]] bool focusable() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerUp(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerLeave() override;
  /** Whether the pointer is currently over the button (the hover background is drawn from this). */
  [[nodiscard]] bool hovered() const noexcept {
    return mHovered;
  }
  bool onKeyDown(const KeyEvent& event) override;
  void onFocusChanged(bool focused) override;

  [[nodiscard]] int activationCount() const noexcept {
    return mActivations;
  }

 protected:
  void applyTheme(const Theme& theme) override;

  bool performAccessibilityAction(AccessibilityAction action, std::string_view value) override;

 private:
  void activate();

  std::shared_ptr<const Image> mIcon;
  std::shared_ptr<const IconTheme> mIconTheme;
  std::string mIconName;
  Color mBackground = Color::MakeRGBA8(52, 56, 66, 255);
  Color mHoverBackground = Color::MakeRGBA8(62, 67, 78, 255);
  Color mPressedBackground = Color::MakeRGBA8(75, 85, 99, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  std::function<void()> mOnClick;
  float mSide = 32.0f;
  float mIconSize = 16.0f;
  float mPadding = 8.0f;
  float mRadius = 4.0f;
  IconButtonVariant mVariant = IconButtonVariant::Standard;
  bool mPressed = false;
  bool mFocused = false;
  bool mHovered = false;
  int mActivations = 0;
};

} // namespace coral::ui

#endif // CORAL_UI_ICONBUTTON_H
