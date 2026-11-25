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

#ifndef CORAL_UI_METER_H
#define CORAL_UI_METER_H

#include <ui/Types.h>

#include <ui/Layer.h>

namespace coral::ui {

struct Theme;

/** A threshold-colored meter, distinct from a task ProgressBar. */
class Meter final : public Layer {
 public:
  Meter(double minimum = 0.0, double maximum = 1.0, double value = 0.0);

  void setRange(double minimum, double maximum);
  void setValue(double value);
  [[nodiscard]] double value() const noexcept {
    return mValue;
  }
  /** Low/high warning thresholds as normalized fractions in [0,1]. */
  void setThresholds(double low, double high);
  void setColors(Color normal, Color warning, Color danger, Color track);
  void setThickness(float thickness);

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] std::optional<AccessibleRange> accessibleRange() const override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "meter";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  Color mNormal = Color::MakeRGBA8(86, 200, 140, 255);
  Color mWarning = Color::MakeRGBA8(230, 180, 80, 255);
  Color mDanger = Color::MakeRGBA8(230, 100, 100, 255);
  Color mTrack = Color::MakeRGBA8(54, 60, 72, 255);
  double mMinimum = 0.0;
  double mMaximum = 1.0;
  double mValue = 0.0;
  double mLow = 0.6;
  double mHigh = 0.85;
  float mThickness = 10.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_METER_H
