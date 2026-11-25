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

#ifndef CORAL_UI_SYSTEMSETTINGS_H
#define CORAL_UI_SYSTEMSETTINGS_H

#include <array>
#include <cstdint>
#include <functional>
#include <string>

namespace coral::ui {

/** The locale/time-format settings a UI normally needs to format dates and numbers. */
struct SystemLocale {
  std::string identifier;
  std::string language;
  /** True when the user's short time format uses a 24-hour clock. */
  bool uses24HourClock = false;
  /** 1 = Sunday ... 7 = Saturday, matching NSCalendar. */
  int firstWeekday = 1;
  std::string decimalSeparator = ".";
  std::string groupingSeparator = ",";
  /** Sunday-first full month names; localisable while keeping the struct trivially copyable. */
  std::array<std::string, 12> monthNames = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
  };
  /** Sunday-first short weekday names, matching NSCalendar's index order. */
  std::array<std::string, 7> weekdayNames = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
  };
};

/** @return the process's current system locale settings. */
[[nodiscard]] SystemLocale currentSystemLocale();

/**
 * Registers a callback fired when the locale, time zone, or date/time preferences change.
 *
 * macOS observes `NSCurrentLocaleDidChangeNotification` and
 * `NSSystemTimeZoneDidChangeNotification`; callbacks are dispatched to the main queue.
 */
void setSystemLocaleChangedHandler(std::function<void()> handler);

[[nodiscard]] bool supportsSystemLocaleMonitoring() noexcept;

} // namespace coral::ui

#endif // CORAL_UI_SYSTEMSETTINGS_H
