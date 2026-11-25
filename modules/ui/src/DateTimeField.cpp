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

#include <ui/DateTimeField.h>

#include <ui/DateField.h>
#include <ui/Theme.h>
#include <ui/TimeField.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

DateTimeField::DateTimeField(std::shared_ptr<const Font> font,
    std::chrono::year_month_day date, const int hour, const int minute) {
  mDate = std::make_shared<DateField>(font, date);
  mTime = std::make_shared<TimeField>(font, hour, minute);
  addChild(mDate);
  addChild(mTime);
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName("Date and time");
  mDate->setOnChanged([this](std::chrono::year_month_day next) {
    if (mOnChanged) mOnChanged(next, mTime->hour(), mTime->minute());
  });
  mTime->setOnChanged([this](int nextHour, int nextMinute) {
    if (mOnChanged) mOnChanged(mDate->date(), nextHour, nextMinute);
  });
}

void DateTimeField::setDate(std::chrono::year_month_day date) {
  mDate->setDate(date);
  if (mOnChanged) mOnChanged(mDate->date(), mTime->hour(), mTime->minute());
}

std::chrono::year_month_day DateTimeField::date() const { return mDate->date(); }

void DateTimeField::setTime(const int hour, const int minute) {
  mTime->setTime(hour, minute);
  if (mOnChanged) mOnChanged(mDate->date(), mTime->hour(), mTime->minute());
}

int DateTimeField::hour() const { return mTime->hour(); }
int DateTimeField::minute() const { return mTime->minute(); }

void DateTimeField::setFont(std::shared_ptr<const Font> font) {
  mDate->setFont(font);
  mTime->setFont(font);
  setNeedsLayout();
}

Size DateTimeField::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ 300.0f, std::max(mDate->measure({}).height, mTime->measure({}).height) });
  return measuredSize();
}

void DateTimeField::layout() {
  const float fieldWidth = std::max(60.0f, bounds().getWidth() - mGap - 120.0f);
  mDate->setBounds(Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), fieldWidth,
    bounds().getHeight()));
  mTime->setBounds(Rect::MakeXYWH(bounds().getLeft() + fieldWidth + mGap, bounds().getTop(),
    std::max(60.0f, bounds().getWidth() - fieldWidth - mGap), bounds().getHeight()));
  mDate->layout();
  mTime->layout();
  clearNeedsLayout();
}

void DateTimeField::applyTheme(const Theme& theme) {
  (void)theme;
  setNeedsLayout();
}

} // namespace coral::ui
