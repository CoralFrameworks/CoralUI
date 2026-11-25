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

#include <ui/TimeRangeField.h>

#include <ui/TextLayer.h>
#include <ui/Theme.h>
#include <ui/TimeField.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

namespace {
int minutes(const int hour, const int minute) {
  return hour * 60 + minute;
}
} // namespace

TimeRangeField::TimeRangeField(std::shared_ptr<const Font> font, const int startHour,
    const int startMinute, const int endHour, const int endMinute) {
  mStart = std::make_shared<TimeField>(font, startHour, startMinute);
  mEnd = std::make_shared<TimeField>(font, endHour, endMinute);
  mSeparator = std::make_shared<TextLayer>("–", font);
  addChild(mStart);
  addChild(mSeparator);
  addChild(mEnd);
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName("Time range");
  mStart->setOnChanged([this](int hour, int minute) { setStart(hour, minute); });
  mEnd->setOnChanged([this](int hour, int minute) { setEnd(hour, minute); });
}

void TimeRangeField::setStart(const int hour, const int minute) {
  mStart->setTime(hour, minute);
  if (minutes(mEnd->hour(), mEnd->minute()) < minutes(hour, minute)) {
    mEnd->setTime(hour, minute);
  }
  if (mOnChanged) {
    mOnChanged(mStart->hour(), mStart->minute(), mEnd->hour(), mEnd->minute());
  }
}

void TimeRangeField::setEnd(const int hour, const int minute) {
  mEnd->setTime(hour, minute);
  if (minutes(mStart->hour(), mStart->minute()) > minutes(hour, minute)) {
    mStart->setTime(hour, minute);
  }
  if (mOnChanged) {
    mOnChanged(mStart->hour(), mStart->minute(), mEnd->hour(), mEnd->minute());
  }
}

int TimeRangeField::startHour() const { return mStart->hour(); }
int TimeRangeField::startMinute() const { return mStart->minute(); }
int TimeRangeField::endHour() const { return mEnd->hour(); }
int TimeRangeField::endMinute() const { return mEnd->minute(); }

void TimeRangeField::setFont(std::shared_ptr<const Font> font) {
  mStart->setFont(font);
  mEnd->setFont(font);
  mSeparator->setFont(font);
  setNeedsLayout();
}

Size TimeRangeField::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ 300.0f, mStart->measure({}).height });
  return measuredSize();
}

void TimeRangeField::layout() {
  const float separatorWidth = 12.0f;
  const float fieldWidth = std::max(40.0f,
    (bounds().getWidth() - separatorWidth - mGap * 2.0f) * 0.5f);
  mStart->setBounds(Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), fieldWidth,
    bounds().getHeight()));
  mSeparator->setBounds(Rect::MakeXYWH(bounds().getLeft() + fieldWidth + mGap,
    bounds().getTop(), separatorWidth, bounds().getHeight()));
  mEnd->setBounds(Rect::MakeXYWH(bounds().getLeft() + fieldWidth + mGap + separatorWidth + mGap,
    bounds().getTop(), fieldWidth, bounds().getHeight()));
  mStart->layout();
  mSeparator->layout();
  mEnd->layout();
  clearNeedsLayout();
}

void TimeRangeField::applyTheme(const Theme& theme) {
  mSeparator->setColor(theme.textMuted);
  setNeedsLayout();
}

} // namespace coral::ui
