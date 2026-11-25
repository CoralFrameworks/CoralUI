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

#ifndef CORAL_UI_TIMEPICKER_H
#define CORAL_UI_TIMEPICKER_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <utility>

namespace coral::ui {

struct Theme;
class Button;
class Stepper;

/** A cross-platform hour/minute picker, used by TimeField's popup. */
class TimePicker final : public Layer {
 public:
  TimePicker(std::shared_ptr<const Font> font, int hour, int minute,
    bool use24HourClock = true);

  void setTime(int hour, int minute);
  [[nodiscard]] int hour() const noexcept {
    return mHour;
  }
  [[nodiscard]] int minute() const noexcept {
    return mMinute;
  }
  void setUse24HourClock(bool use24Hour);
  [[nodiscard]] std::string valueText() const;
  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground, Color accent);
  void setOnChanged(std::function<void(int, int)> callback) {
    mOnChanged = std::move(callback);
  }
  void setOnCancel(std::function<void()> callback) {
    mOnCancel = std::move(callback);
  }
  void setOnDone(std::function<void()> callback) {
    mOnDone = std::move(callback);
  }

  [[nodiscard]] bool focusable() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  bool onKeyDown(const KeyEvent& event) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "timepicker";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void notify();

  std::shared_ptr<const Font> mFont;
  std::shared_ptr<Stepper> mHourStepper;
  std::shared_ptr<Stepper> mMinuteStepper;
  std::shared_ptr<Button> mDoneButton;
  std::function<void(int, int)> mOnChanged;
  std::function<void()> mOnCancel;
  std::function<void()> mOnDone;
  Color mBackground = Color::MakeRGBA8(45, 49, 59, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mAccent = Color::MakeRGBA8(86, 156, 255, 255);
  int mHour = 0;
  int mMinute = 0;
  bool mUse24HourClock = true;
};

} // namespace coral::ui

#endif // CORAL_UI_TIMEPICKER_H
