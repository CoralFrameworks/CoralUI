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
#include <ui/SystemSettings.h>
#include <ui/Translation.h>

#include <cstdio>

namespace coral::ui {

DateFormatOptions dateFormatIso() {
  return DateFormatOptions{ DateOrder::YearMonthDay, "-" };
}

DateFormatOptions dateFormatDeDe() {
  return DateFormatOptions{ DateOrder::DayMonthYear, "." };
}

DateFormatOptions dateFormatEnUs() {
  return DateFormatOptions{ DateOrder::MonthDayYear, "/" };
}

DateFormatOptions dateFormatForLocale(const Locale& locale) {
  const std::string& language = locale.language;
  const std::string& region = locale.region;
  if (region == "US") return dateFormatEnUs();
  if (language == "zh" || language == "ja" || language == "ko") return dateFormatIso();
  if (language == "de" || language == "ru" || language == "uk" || language == "pl" ||
      language == "cs" || language == "sk" || language == "hu" || language == "tr" ||
      language == "fi" || language == "sv" || language == "nb" || language == "da") {
    return DateFormatOptions{ DateOrder::DayMonthYear, "." };
  }
  if (language == "fr" || language == "es" || language == "it" || language == "pt" ||
      language == "nl" || language == "en") {
    // en outside the US (GB/AU/IN/...) and the rest of Europe write the day first.
    return DateFormatOptions{ DateOrder::DayMonthYear, "/" };
  }
  return dateFormatIso();
}

DateFormatOptions systemDateFormat() {
  return dateFormatForLocale(Locale::parse(currentSystemLocale().identifier));
}

std::string formatDate(const std::chrono::year_month_day& date,
    const DateFormatOptions& options) {
  if (!date.ok()) return "invalid";
  const int year = static_cast<int>(date.year());
  const unsigned month = static_cast<unsigned>(date.month());
  const unsigned day = static_cast<unsigned>(date.day());
  char buffer[32]{};
  switch (options.order) {
    case DateOrder::YearMonthDay:
      std::snprintf(buffer, sizeof(buffer), "%04d%s%02u%s%02u", year,
        options.separator.c_str(), month, options.separator.c_str(), day);
      break;
    case DateOrder::DayMonthYear:
      std::snprintf(buffer, sizeof(buffer), "%02u%s%02u%s%04d", day,
        options.separator.c_str(), month, options.separator.c_str(), year);
      break;
    case DateOrder::MonthDayYear:
      std::snprintf(buffer, sizeof(buffer), "%02u%s%02u%s%04d", month,
        options.separator.c_str(), day, options.separator.c_str(), year);
      break;
  }
  return std::string(buffer);
}

} // namespace coral::ui
