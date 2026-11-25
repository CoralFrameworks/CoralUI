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

#ifndef CORAL_UI_RANGESLIDER_H
#define CORAL_UI_RANGESLIDER_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <optional>

namespace coral::ui {

struct Theme;

/** A two-handle numeric range slider. */
class RangeSlider final : public Layer {
 public:
  RangeSlider(double minimum = 0.0, double maximum = 1.0,
    double low = 0.25, double high = 0.75, double step = 0.01);

  void setRange(double minimum, double maximum);
  [[nodiscard]] double minimum() const noexcept {
    return mMinimum;
  }
  [[nodiscard]] double maximum() const noexcept {
    return mMaximum;
  }
  void setLow(double low);
  void setHigh(double high);
  [[nodiscard]] double low() const noexcept {
    return mLow;
  }
  [[nodiscard]] double high() const noexcept {
    return mHigh;
  }
  void setStep(double step);
  void setColors(Color track, Color range, Color handle, Color focus);
  void setOnChanged(std::function<void(double, double)> callback) {
    mOnChanged = std::move(callback);
  }

  [[nodiscard]] bool focusable() const override {
    return true;
  }
  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerUp(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  void onFocusChanged(bool focused) override;
  [[nodiscard]] std::optional<AccessibleRange> accessibleRange() const override;
  bool performAccessibilityAction(AccessibilityAction action,
    std::string_view value) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "rangeslider";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  enum class Handle { Low, High };
  [[nodiscard]] float xForValue(double value) const;
  [[nodiscard]] double valueForX(float x) const;
  void setHandle(Handle handle, double value, bool notify);
  void updateFromPoint(Point point);

  double mMinimum = 0.0;
  double mMaximum = 1.0;
  double mLow = 0.25;
  double mHigh = 0.75;
  double mStep = 0.01;
  std::function<void(double, double)> mOnChanged;
  Color mTrack = Color::MakeRGBA8(54, 60, 72, 255);
  Color mRange = Color::MakeRGBA8(86, 156, 255, 255);
  Color mHandle = Color::MakeRGBA8(226, 232, 240, 255);
  Color mFocus = Color::MakeRGBA8(120, 180, 255, 255);
  Handle mActive = Handle::Low;
  bool mDragging = false;
  bool mFocused = false;
};

} // namespace coral::ui

#endif // CORAL_UI_RANGESLIDER_H
