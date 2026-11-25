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

#include <ui/DateRangeField.h>

#include <ui/DateField.h>
#include <ui/TextLayer.h>
#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

DateRangeField::DateRangeField(std::shared_ptr<const Font> font,
    std::chrono::year_month_day start, std::chrono::year_month_day end) {
  mStart = std::make_shared<DateField>(font, start);
  mEnd = std::make_shared<DateField>(font, end);
  mSeparator = std::make_shared<TextLayer>("–", font);
  addChild(mStart);
  addChild(mSeparator);
  addChild(mEnd);
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName("Date range");
  mStart->setOnChanged([this](std::chrono::year_month_day date) { setStart(date); });
  mEnd->setOnChanged([this](std::chrono::year_month_day date) { setEnd(date); });
}

void DateRangeField::setStart(const std::chrono::year_month_day date) {
  mStart->setDate(date);
  if (mEnd->date() < date) mEnd->setDate(date);
  if (mOnChanged) mOnChanged(mStart->date(), mEnd->date());
}

void DateRangeField::setEnd(const std::chrono::year_month_day date) {
  mEnd->setDate(date);
  if (mStart->date() > date) mStart->setDate(date);
  if (mOnChanged) mOnChanged(mStart->date(), mEnd->date());
}

std::chrono::year_month_day DateRangeField::start() const {
  return mStart->date();
}

std::chrono::year_month_day DateRangeField::end() const {
  return mEnd->date();
}

void DateRangeField::setFont(std::shared_ptr<const Font> font) {
  mStart->setFont(font);
  mEnd->setFont(font);
  mSeparator->setFont(font);
  setNeedsLayout();
}

Size DateRangeField::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ 380.0f, mStart->measure({}).height });
  return measuredSize();
}

void DateRangeField::layout() {
  const float separatorWidth = 12.0f;
  const float fieldWidth = std::max(40.0f,
    (bounds().getWidth() - separatorWidth - mGap * 2.0f) * 0.5f);
  const float height = bounds().getHeight();
  mStart->setBounds(Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), fieldWidth, height));
  mSeparator->setBounds(Rect::MakeXYWH(bounds().getLeft() + fieldWidth + mGap,
    bounds().getTop(), separatorWidth, height));
  mEnd->setBounds(Rect::MakeXYWH(bounds().getLeft() + fieldWidth + mGap + separatorWidth + mGap,
    bounds().getTop(), fieldWidth, height));
  mStart->layout();
  mSeparator->layout();
  mEnd->layout();
  clearNeedsLayout();
}

void DateRangeField::applyTheme(const Theme& theme) {
  mSeparator->setColor(theme.textMuted);
  setNeedsLayout();
}

} // namespace coral::ui
