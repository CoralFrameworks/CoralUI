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

#ifndef CORAL_UI_STATUSINDICATOR_H
#define CORAL_UI_STATUSINDICATOR_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

enum class StatusKind {
  Online,
  Away,
  Busy,
  Offline,
};

/** A colored status dot followed by a short label. */
class StatusIndicator final : public Layer {
 public:
  StatusIndicator(std::string label, std::shared_ptr<const Font> font,
    StatusKind status = StatusKind::Online);

  void setLabel(std::string label);
  [[nodiscard]] const std::string& label() const noexcept {
    return mLabel;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setStatus(StatusKind status);
  [[nodiscard]] StatusKind status() const noexcept {
    return mStatus;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "status";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::string mLabel;
  std::shared_ptr<const Font> mFont;
  StatusKind mStatus = StatusKind::Online;
  Color mText = Color::MakeRGBA8(226, 232, 240, 255);
  Color mOnline = Color::MakeRGBA8(80, 200, 140, 255);
  Color mAway = Color::MakeRGBA8(230, 180, 80, 255);
  Color mBusy = Color::MakeRGBA8(230, 100, 100, 255);
  Color mOffline = Color::MakeRGBA8(130, 140, 155, 255);
  float mDot = 8.0f;
  float mGap = 6.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_STATUSINDICATOR_H
