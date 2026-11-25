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

#ifndef CORAL_UI_DATE_FORMAT_H
#define CORAL_UI_DATE_FORMAT_H

#include <chrono>
#include <string>

#include <ui/Translation.h>

namespace coral::ui {

/** Component order of a numeric date. */
enum class DateOrder {
  YearMonthDay,
  DayMonthYear,
  MonthDayYear,
};

/**
 * Numeric date formatting with an explicit order and separator.
 *
 * Like NumberFormat this deliberately avoids the global C++ locale: the same process can render an
 * ISO log timestamp and a localised inspector row side by side. Month and day are always zero-padded,
 * because a column of dates that shifts width is unreadable.
 */
struct DateFormatOptions {
  DateOrder order = DateOrder::YearMonthDay;
  std::string separator = "-";
};

[[nodiscard]] DateFormatOptions dateFormatIso();
[[nodiscard]] DateFormatOptions dateFormatDeDe();
[[nodiscard]] DateFormatOptions dateFormatEnUs();

/**
 * The component order and separator @p locale writes dates with: month-first in the US, day-first in
 * most of Europe and the Commonwealth, year-first in zh/ja/ko and for the ISO fallback.
 */
[[nodiscard]] DateFormatOptions dateFormatForLocale(const Locale& locale);

/** The date order for the current OS locale (falling back to ISO). */
[[nodiscard]] DateFormatOptions systemDateFormat();

/**
 * Formats @p date (for example `std::chrono::year_month_day{ 2026y, November, 16d }`).
 *
 * An invalid calendar date is rendered as "invalid" rather than silently normalised, so a data bug
 * is visible instead of showing a plausible wrong day.
 */
[[nodiscard]] std::string formatDate(const std::chrono::year_month_day& date,
  const DateFormatOptions& options = {});

} // namespace coral::ui

#endif // CORAL_UI_DATE_FORMAT_H
