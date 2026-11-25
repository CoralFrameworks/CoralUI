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

#ifndef CORAL_UI_NUMBER_FORMAT_H
#define CORAL_UI_NUMBER_FORMAT_H

#include <string>

#include <ui/Translation.h>

namespace coral::ui {

/**
 * Locale-aware decimal formatting without depending on the C++ locale machinery.
 *
 * The process locale is global and `std::format`/`std::to_chars` locale overloads are not portable
 * yet, so a UI that wants "1.234,56" in German must choose the separators explicitly. This helper
 * does exactly that and nothing else: grouping, a fixed decimal separator, and a trailing-digit
 * policy.
 */
struct NumberFormatOptions {
  /** Separate thousands with `groupSeparator`. */
  bool grouping = true;
  std::string groupSeparator = ",";
  std::string decimalSeparator = ".";
  /** Digits kept after rounding, between 0 and 9. */
  int maxFractionDigits = 3;
  /** Digits always shown, zero-padded, never more than `maxFractionDigits`. */
  int minFractionDigits = 0;
};

/** Convenience presets; a caller that needs another locale fills NumberFormatOptions directly. */
[[nodiscard]] NumberFormatOptions numberFormatEnUs();
[[nodiscard]] NumberFormatOptions numberFormatDeDe();
[[nodiscard]] NumberFormatOptions numberFormatFrFr();

/**
 * The separators and digit policy @p locale writes numbers with.
 *
 * Covers the conventions the shipped catalogs need (en/de/fr/ru/pl/es/it/pt/nl/sv/fi/da/tr/zh/ja/ko)
 * and falls back to the en-US form otherwise. Indian digit grouping (2,2,3) is not expressible with
 * `NumberFormatOptions` yet, so `en-IN` uses the standard three-digit grouping.
 */
[[nodiscard]] NumberFormatOptions numberFormatForLocale(const Locale& locale);

/** The separators the current OS locale reports (falling back to en-US). */
[[nodiscard]] NumberFormatOptions systemNumberFormat();

/**
 * Formats @p value with @p options.
 *
 * Non-finite values return "NaN", "Infinity" or "-Infinity" verbatim, because a UI that silently
 * prints "0" for a failed computation is worse than one that shows the failure. Grouping is applied
 * to the integral digits only, and a leading minus sign is kept outside the grouped digits.
 */
[[nodiscard]] std::string formatNumber(double value, const NumberFormatOptions& options = {});

} // namespace coral::ui

#endif // CORAL_UI_NUMBER_FORMAT_H
