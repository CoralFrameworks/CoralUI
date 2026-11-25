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

#ifndef CORAL_UI_PROGRESSBAR_H
#define CORAL_UI_PROGRESSBAR_H

#include <ui/Types.h>

#include <ui/Layer.h>


#include <chrono>
#include <memory>

namespace coral::ui {

struct Theme;

/** A non-interactive progress bar in the 0..1 range. */
class ProgressBar final : public Layer {
 public:
  ProgressBar(std::shared_ptr<const Font> font, double value = 0.0);

  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground,
    Color accent);
  void setValue(double value);
  [[nodiscard]] double value() const noexcept {
    return mValue;
  }
  void setShowPercentage(bool show) {
    mShowPercentage = show;
    setNeedsPaint();
  }
  /** An indeterminate bar ignores the value and animates a moving band. */
  void setIndeterminate(bool indeterminate);
  [[nodiscard]] bool indeterminate() const noexcept {
    return mIndeterminate;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] std::optional<AccessibleRange> accessibleRange() const override;
  void onFrame(std::chrono::steady_clock::time_point now) override;
  [[nodiscard]] bool hasRunningAnimation() const override {
    return mIndeterminate;
  }
  [[nodiscard]] const char* defaultStyleName() const override {
    return "progress";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::shared_ptr<const Font> mFont;
  Color mBackground = Color::MakeRGBA8(35, 38, 46, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mAccent = Color::MakeRGBA8(86, 156, 255, 255);
  float mRadius = 4.0f;
  double mValue = 0.0;
  bool mShowPercentage = false;
  bool mIndeterminate = false;
  double mPhase = 0.0;
  std::chrono::steady_clock::time_point mLastFrame{};
};

} // namespace coral::ui

#endif // CORAL_UI_PROGRESSBAR_H
