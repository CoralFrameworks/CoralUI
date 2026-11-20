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

/*
 * Generated from WordBreakProperty-18.0.0.txt (UAX #29).
 * DO NOT EDIT BY HAND; see tools/unicode/generate_wordbreak_properties.py.
 */

#ifndef CORAL_UI_UNICODE_WORDBREAKPROPERTIES_GENERATED_H
#define CORAL_UI_UNICODE_WORDBREAKPROPERTIES_GENERATED_H

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace coral::ui::unicode {

enum class WordBreakClass : uint8_t {
  ALetter,
  CR,
  Double_Quote,
  Extend,
  ExtendNumLet,
  Format,
  Hebrew_Letter,
  Katakana,
  LF,
  MidLetter,
  MidNum,
  MidNumLet,
  Newline,
  Numeric,
  Other,
  Regional_Indicator,
  Single_Quote,
  WSegSpace,
  ZWJ,
};

struct WordBreakRange {
  uint32_t first;
  uint32_t last;
  WordBreakClass value;
};

/** @return the UAX #29 Word_Break class; Other for unassigned code points. */
[[nodiscard]] WordBreakClass wordBreakClass(uint32_t codePoint) noexcept;

/** @return the UCD class name, e.g. "ALetter"; never null. */
[[nodiscard]] const char* wordBreakClassName(WordBreakClass value) noexcept;

/** One contiguous range, materialized on demand (table tests). */
[[nodiscard]] WordBreakRange wordBreakRangeAt(size_t index) noexcept;
[[nodiscard]] size_t wordBreakRangeCount() noexcept;

inline constexpr uint8_t kAsciiWordBreakClasses[128] = {
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::LF),
  static_cast<uint8_t>(WordBreakClass::Newline),
  static_cast<uint8_t>(WordBreakClass::Newline),
  static_cast<uint8_t>(WordBreakClass::CR),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::WSegSpace),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Double_Quote),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Single_Quote),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::MidNum),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::MidNumLet),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Numeric),
  static_cast<uint8_t>(WordBreakClass::Numeric),
  static_cast<uint8_t>(WordBreakClass::Numeric),
  static_cast<uint8_t>(WordBreakClass::Numeric),
  static_cast<uint8_t>(WordBreakClass::Numeric),
  static_cast<uint8_t>(WordBreakClass::Numeric),
  static_cast<uint8_t>(WordBreakClass::Numeric),
  static_cast<uint8_t>(WordBreakClass::Numeric),
  static_cast<uint8_t>(WordBreakClass::Numeric),
  static_cast<uint8_t>(WordBreakClass::Numeric),
  static_cast<uint8_t>(WordBreakClass::MidLetter),
  static_cast<uint8_t>(WordBreakClass::MidNum),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::ExtendNumLet),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::ALetter),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
  static_cast<uint8_t>(WordBreakClass::Other),
};

inline constexpr std::string_view kWordBreakDataSha256 = "8dbfa17063e11084201f33c3e76d485d3b9166930c71db8e39ed1c9234171aec";
inline constexpr std::string_view kWordBreakUnicodeVersion = "18.0.0";

} // namespace coral::ui::unicode

#endif // CORAL_UI_UNICODE_WORDBREAKPROPERTIES_GENERATED_H
