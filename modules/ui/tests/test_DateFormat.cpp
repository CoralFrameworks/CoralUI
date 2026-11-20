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

#include <ui/DateFormat.h>

#include <gtest/gtest.h>

namespace coral::ui {
namespace {

TEST(UiDateFormat, FormatsEachComponentOrder) {
  const std::chrono::year_month_day date{ std::chrono::year{ 2026 }, std::chrono::November,
    std::chrono::day{ 16 } };
  EXPECT_EQ(formatDate(date, dateFormatIso()), "2026-11-16");
  EXPECT_EQ(formatDate(date, dateFormatDeDe()), "16.11.2026");
  EXPECT_EQ(formatDate(date, dateFormatEnUs()), "11/16/2026");
}

TEST(UiDateFormat, PadsMonthAndDay) {
  const std::chrono::year_month_day date{ std::chrono::year{ 2026 }, std::chrono::January,
    std::chrono::day{ 5 } };
  EXPECT_EQ(formatDate(date, dateFormatIso()), "2026-01-05");
}

TEST(UiDateFormat, RejectsInvalidCalendarDates) {
  const std::chrono::year_month_day date{ std::chrono::year{ 2026 }, std::chrono::February,
    std::chrono::day{ 30 } };
  EXPECT_EQ(formatDate(date), "invalid") << "a bad date stays visible instead of normalising";
}

TEST(UiDateFormat, KeepsTheSeparatorExplicit) {
  DateFormatOptions custom;
  custom.order = DateOrder::DayMonthYear;
  custom.separator = " ";
  const std::chrono::year_month_day date{ std::chrono::year{ 2026 }, std::chrono::March,
    std::chrono::day{ 2 } };
  EXPECT_EQ(formatDate(date, custom), "02 03 2026");
}

} // namespace
} // namespace coral::ui
