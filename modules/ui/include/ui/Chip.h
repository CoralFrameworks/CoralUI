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

#ifndef CORAL_UI_CHIP_H
#define CORAL_UI_CHIP_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

enum class ChipVariant {
  Neutral,
  Accent,
  Success,
  Warning,
  Danger,
};

/** A compact removable tag/chip, used for filters and selections. */
class Chip final : public Layer {
 public:
  Chip(std::string text, std::shared_ptr<const Font> font);

  void setText(std::string text);
  [[nodiscard]] const std::string& text() const noexcept {
    return mText;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground, Color accent);
  void setVariant(ChipVariant variant);
  [[nodiscard]] ChipVariant variant() const noexcept {
    return mVariant;
  }
  /** Optional leading icon; the chip does not own the image. */
  void setIcon(std::shared_ptr<const graphics::Image> icon);
  [[nodiscard]] const std::shared_ptr<const graphics::Image>& icon() const noexcept {
    return mIcon;
  }
  void setRemovable(bool removable);
  [[nodiscard]] bool removable() const noexcept {
    return mRemovable;
  }
  void setSelected(bool selected);
  [[nodiscard]] bool selected() const noexcept {
    return mSelected;
  }
  void setOnRemove(std::function<void()> callback) {
    mOnRemove = std::move(callback);
  }

  [[nodiscard]] bool focusable() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "chip";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void remove();

  std::string mText;
  std::shared_ptr<const Font> mFont;
  std::shared_ptr<const graphics::Image> mIcon;
  std::function<void()> mOnRemove;
  Color mBackground = Color::MakeRGBA8(52, 56, 66, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mAccent = Color::MakeRGBA8(86, 156, 255, 255);
  float mPadding = 9.0f;
  bool mRemovable = true;
  bool mSelected = false;
  ChipVariant mVariant = ChipVariant::Neutral;
};

} // namespace coral::ui

#endif // CORAL_UI_CHIP_H
