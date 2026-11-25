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

#ifndef CORAL_UI_DATERANGEFIELD_H
#define CORAL_UI_DATERANGEFIELD_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <chrono>
#include <functional>
#include <memory>

namespace coral::ui {

struct Theme;
class DateField;
class TextLayer;

/** Two DateFields with a visual separator; start is clamped to end. */
class DateRangeField final : public Layer {
 public:
  DateRangeField(std::shared_ptr<const Font> font,
    std::chrono::year_month_day start = std::chrono::year_month_day{},
    std::chrono::year_month_day end = std::chrono::year_month_day{});

  void setStart(std::chrono::year_month_day date);
  void setEnd(std::chrono::year_month_day date);
  [[nodiscard]] std::chrono::year_month_day start() const;
  [[nodiscard]] std::chrono::year_month_day end() const;
  void setFont(std::shared_ptr<const Font> font);
  void setOnChanged(std::function<void(std::chrono::year_month_day,
    std::chrono::year_month_day)> callback) {
    mOnChanged = std::move(callback);
  }

  Size measure(const Size& available) override;
  void layout() override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "daterangefield";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::shared_ptr<DateField> mStart;
  std::shared_ptr<DateField> mEnd;
  std::shared_ptr<TextLayer> mSeparator;
  std::function<void(std::chrono::year_month_day, std::chrono::year_month_day)> mOnChanged;
  float mGap = 8.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_DATERANGEFIELD_H
