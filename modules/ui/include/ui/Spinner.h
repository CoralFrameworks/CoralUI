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

#ifndef CORAL_UI_SPINNER_H
#define CORAL_UI_SPINNER_H

#include <ui/Layer.h>

#include <chrono>

namespace coral::ui {

/**
 * An indeterminate busy indicator drawn as a ring of dots.
 *
 * The layer is a self-driving animation: while spinning it reports
 * `hasRunningAnimation()` so the window keeps producing frames, and each frame advances the dot
 * phase by wall-clock time (about one turn per second). Call `start()`/`setSpinning(true)` when a
 * task begins and stop when it finishes; a stopped spinner draws a static ring.
 */
class Spinner final : public Layer {
 public:
  Spinner();
  Spinner(bool spinning, Color color) : mColor(color), mSpinning(spinning) {
    setAccessibleRole(AccessibilityRole::ProgressIndicator);
    setAccessibleValue("Loading");
  }

  void setColor(Color color);
  [[nodiscard]] Color color() const noexcept {
    return mColor;
  }
  void setDotCount(int count);
  void setSpinning(bool spinning);
  [[nodiscard]] bool spinning() const noexcept {
    return mSpinning;
  }
  void start() {
    setSpinning(true);
  }
  void stop() {
    setSpinning(false);
  }

  /** Advances the phase from wall-clock time; exposed for tests and deterministic frames. */
  void advance(std::chrono::milliseconds delta);
  [[nodiscard]] float phase() const noexcept {
    return mPhase;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  void onFrame(std::chrono::steady_clock::time_point now) override;
  /** Theme switches repaint the dots with the theme's accent colour. */
  void applyTheme(const Theme& theme) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "spinner";
  }
  [[nodiscard]] bool hasRunningAnimation() const override {
    return mSpinning;
  }

 private:
  Color mColor = Color::MakeRGBA8(120, 170, 255, 255);
  float mPhase = 0.0f;
  int mDotCount = 8;
  bool mSpinning = false;
  std::chrono::steady_clock::time_point mLastFrame{};
};

} // namespace coral::ui

#endif // CORAL_UI_SPINNER_H
