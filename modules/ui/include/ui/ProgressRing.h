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

#ifndef CORAL_UI_PROGRESSRING_H
#define CORAL_UI_PROGRESSRING_H

#include <ui/Types.h>

#include <chrono>
#include <ui/Layer.h>

namespace coral::ui {

struct Theme;

/** A circular determinate progress indicator. */
class ProgressRing final : public Layer {
 public:
  explicit ProgressRing(float size = 36.0f, double value = 0.0);

  void setSize(float size);
  [[nodiscard]] float size() const noexcept {
    return mSide;
  }
  void setValue(double value);
  [[nodiscard]] double value() const noexcept {
    return mValue;
  }
  void setThickness(float thickness);
  void setColors(Color track, Color progress);
  void setIndeterminate(bool indeterminate);
  [[nodiscard]] bool indeterminate() const noexcept {
    return mIndeterminate;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  void onFrame(std::chrono::steady_clock::time_point now) override;
  [[nodiscard]] bool hasRunningAnimation() const override {
    return mIndeterminate;
  }
  [[nodiscard]] std::optional<AccessibleRange> accessibleRange() const override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "progressring";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  Color mTrack = Color::MakeRGBA8(54, 60, 72, 255);
  Color mProgress = Color::MakeRGBA8(86, 156, 255, 255);
  float mSide = 36.0f;
  float mThickness = 4.0f;
  double mValue = 0.0;
  float mPhase = 0.0f;
  bool mIndeterminate = false;
};

} // namespace coral::ui

#endif // CORAL_UI_PROGRESSRING_H
