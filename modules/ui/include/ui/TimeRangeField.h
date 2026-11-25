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

#ifndef CORAL_UI_TIMERANGEFIELD_H
#define CORAL_UI_TIMERANGEFIELD_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <memory>

namespace coral::ui {

struct Theme;
class TimeField;
class TextLayer;

/** Two TimeFields with a separator; the end is clamped to the start. */
class TimeRangeField final : public Layer {
 public:
  TimeRangeField(std::shared_ptr<const Font> font,
    int startHour = 0, int startMinute = 0, int endHour = 1, int endMinute = 0);

  void setStart(int hour, int minute);
  void setEnd(int hour, int minute);
  [[nodiscard]] int startHour() const;
  [[nodiscard]] int startMinute() const;
  [[nodiscard]] int endHour() const;
  [[nodiscard]] int endMinute() const;
  void setFont(std::shared_ptr<const Font> font);
  void setOnChanged(std::function<void(int, int, int, int)> callback) {
    mOnChanged = std::move(callback);
  }

  Size measure(const Size& available) override;
  void layout() override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "timerangefield";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::shared_ptr<TimeField> mStart;
  std::shared_ptr<TimeField> mEnd;
  std::shared_ptr<TextLayer> mSeparator;
  std::function<void(int, int, int, int)> mOnChanged;
  float mGap = 8.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_TIMERANGEFIELD_H
