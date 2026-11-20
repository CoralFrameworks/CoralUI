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
 * Generated from Unicode 18.0.0 DerivedBidiClass.txt.
 * Source: https://www.unicode.org/Public/18.0.0/ucd/extracted/DerivedBidiClass.txt
 * DO NOT EDIT BY HAND; see tools/unicode/generate_bidi_properties.py.
 */

#ifndef CORAL_UI_UNICODE_BIDIPROPERTIES_GENERATED_H
#define CORAL_UI_UNICODE_BIDIPROPERTIES_GENERATED_H

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace coral::ui::unicode {

enum class BidiClass : uint8_t {
  L,
  R,
  AL,
  EN,
  ES,
  ET,
  AN,
  CS,
  NSM,
  BN,
  B,
  S,
  WS,
  ON,
  LRE,
  LRO,
  RLE,
  RLO,
  PDF,
  LRI,
  RLI,
  FSI,
  PDI,
  Count,
};

inline constexpr size_t kBidiClassCount = static_cast<size_t>(BidiClass::Count);

struct BidiRange {
  uint32_t first;
  uint32_t last;
  BidiClass value;
};

[[nodiscard]] BidiClass bidiClass(uint32_t codePoint) noexcept;
[[nodiscard]] const char* bidiClassName(BidiClass value) noexcept;
[[nodiscard]] bool bidiClassFromName(std::string_view name, BidiClass& value) noexcept;
[[nodiscard]] const BidiRange* bidiRanges() noexcept;
[[nodiscard]] size_t bidiRangeCount() noexcept;

/** ASCII fast path: class without a range search. */
inline constexpr BidiClass kAsciiBidiClasses[128] = {
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::S,
  BidiClass::B,
  BidiClass::S,
  BidiClass::WS,
  BidiClass::B,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::BN,
  BidiClass::B,
  BidiClass::B,
  BidiClass::B,
  BidiClass::S,
  BidiClass::WS,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::ET,
  BidiClass::ET,
  BidiClass::ET,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::ES,
  BidiClass::CS,
  BidiClass::ES,
  BidiClass::CS,
  BidiClass::CS,
  BidiClass::EN,
  BidiClass::EN,
  BidiClass::EN,
  BidiClass::EN,
  BidiClass::EN,
  BidiClass::EN,
  BidiClass::EN,
  BidiClass::EN,
  BidiClass::EN,
  BidiClass::EN,
  BidiClass::CS,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::L,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::ON,
  BidiClass::BN,
};

} // namespace coral::ui::unicode

#endif // CORAL_UI_UNICODE_BIDIPROPERTIES_GENERATED_H
