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

#ifndef CORAL_UI_DATETIMEFIELD_H
#define CORAL_UI_DATETIMEFIELD_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <chrono>
#include <functional>
#include <memory>

namespace coral::ui {

struct Theme;
class DateField;
class TimeField;

/** A DateField and TimeField composed into one control. */
class DateTimeField final : public Layer {
 public:
  DateTimeField(std::shared_ptr<const Font> font,
    std::chrono::year_month_day date = std::chrono::year_month_day{},
    int hour = 0, int minute = 0);

  void setDate(std::chrono::year_month_day date);
  [[nodiscard]] std::chrono::year_month_day date() const;
  void setTime(int hour, int minute);
  [[nodiscard]] int hour() const;
  [[nodiscard]] int minute() const;
  void setFont(std::shared_ptr<const Font> font);
  void setOnChanged(std::function<void(std::chrono::year_month_day, int, int)> callback) {
    mOnChanged = std::move(callback);
  }

  Size measure(const Size& available) override;
  void layout() override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "datetimefield";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::shared_ptr<DateField> mDate;
  std::shared_ptr<TimeField> mTime;
  std::function<void(std::chrono::year_month_day, int, int)> mOnChanged;
  float mGap = 8.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_DATETIMEFIELD_H
