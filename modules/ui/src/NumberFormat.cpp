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

#include <ui/NumberFormat.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace coral::ui {

NumberFormatOptions numberFormatEnUs() {
  NumberFormatOptions options;
  options.grouping = true;
  options.groupSeparator = ",";
  options.decimalSeparator = ".";
  options.maxFractionDigits = 3;
  options.minFractionDigits = 0;
  return options;
}

NumberFormatOptions numberFormatDeDe() {
  NumberFormatOptions options;
  options.grouping = true;
  options.groupSeparator = ".";
  options.decimalSeparator = ",";
  options.maxFractionDigits = 3;
  options.minFractionDigits = 0;
  return options;
}

NumberFormatOptions numberFormatFrFr() {
  NumberFormatOptions options;
  options.grouping = true;
  options.groupSeparator = "\xE2\x80\xAF"; // U+202F narrow no-break space, the French group separator.
  options.decimalSeparator = ",";
  options.maxFractionDigits = 3;
  options.minFractionDigits = 0;
  return options;
}

NumberFormatOptions numberFormatForLocale(const Locale& locale) {
  const std::string& language = locale.language;
  const std::string& region = locale.region;
  const auto withSeparators = [](std::string group, std::string decimal) {
    NumberFormatOptions options;
    options.grouping = true;
    options.groupSeparator = std::move(group);
    options.decimalSeparator = std::move(decimal);
    return options;
  };
  // Space-grouped, comma-decimal family: the non-breaking space keeps a group from wrapping.
  const std::string spaceGroup = "\xC2\xA0"; // U+00A0
  const std::string narrowSpaceGroup = "\xE2\x80\xAF"; // U+202F

  if (language == "de" || language == "at" || (language == "nl" && region.empty())) {
    return numberFormatDeDe();
  }
  if (language == "fr" || language == "be" || language == "lb") {
    if (language == "fr") return numberFormatFrFr();
    return withSeparators(narrowSpaceGroup, ",");
  }
  if (language == "ru" || language == "uk" || language == "pl" || language == "cs" ||
      language == "sk" || language == "hu" || language == "sv" || language == "fi" ||
      language == "nb" || language == "da") {
    return withSeparators(spaceGroup, ",");
  }
  if (language == "es" || language == "it" || language == "pt" || language == "tr" ||
      language == "nl") {
    return withSeparators(".", ",");
  }
  if (language == "zh" || language == "ja" || language == "ko" || language == "th" ||
      language == "vi" || language == "id") {
    return numberFormatEnUs();
  }
  // en and every language without a preset above.
  return numberFormatEnUs();
}

std::string formatNumber(const double value, const NumberFormatOptions& options) {
  if (std::isnan(value)) return "NaN";
  if (std::isinf(value)) return value < 0.0 ? "-Infinity" : "Infinity";

  const int maxDigits = std::clamp(options.maxFractionDigits, 0, 9);
  const int minDigits = std::clamp(options.minFractionDigits, 0, maxDigits);

  char buffer[64]{};
  std::snprintf(buffer, sizeof(buffer), "%.*f", maxDigits, value);
  std::string text(buffer);

  // Trim trailing fraction zeros down to the minimum, then drop a bare decimal separator.
  const size_t dot = text.find('.');
  if (dot != std::string::npos) {
    while (static_cast<int>(text.size() - dot - 1) > minDigits && text.back() == '0') {
      text.pop_back();
    }
    if (!text.empty() && text.back() == '.') text.pop_back();
  }

  // Split the sign, the integral digits and the fraction.
  bool negative = false;
  size_t start = 0;
  if (!text.empty() && (text[0] == '-' || text[0] == '+')) {
    negative = text[0] == '-';
    start = 1;
  }
  const size_t fractionDot = text.find('.', start);
  const size_t integralEnd = fractionDot == std::string::npos ? text.size() : fractionDot;
  std::string integral = text.substr(start, integralEnd - start);
  std::string fraction;
  if (fractionDot != std::string::npos) fraction = text.substr(fractionDot + 1);

  // Pad to the minimum fraction digits (snprintf already rounded to the maximum).
  while (static_cast<int>(fraction.size()) < minDigits) fraction.push_back('0');

  std::string grouped;
  if (options.grouping && integral.size() > 3) {
    const size_t first = integral.size() % 3 == 0 ? 3 : integral.size() % 3;
    grouped = integral.substr(0, first);
    for (size_t index = first; index < integral.size(); index += 3) {
      grouped += options.groupSeparator;
      grouped += integral.substr(index, 3);
    }
  } else {
    grouped = integral;
  }

  std::string result;
  if (negative) result.push_back('-');
  result += grouped;
  if (!fraction.empty()) {
    result += options.decimalSeparator;
    result += fraction;
  }
  return result;
}

} // namespace coral::ui
