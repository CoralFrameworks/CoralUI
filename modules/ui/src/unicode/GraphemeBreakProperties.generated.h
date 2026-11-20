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
 * Generated from Unicode 18.0.0 GraphemeBreakProperty/emoji-data/DerivedCoreProperties.
 * DO NOT EDIT BY HAND; see tools/unicode/generate_grapheme_properties.py.
 */

#ifndef CORAL_UI_UNICODE_GRAPHEMEBREAKPROPERTIES_GENERATED_H
#define CORAL_UI_UNICODE_GRAPHEMEBREAKPROPERTIES_GENERATED_H

#include <cstddef>
#include <cstdint>

namespace coral::ui::unicode {

enum class GraphemeBreakClass : uint8_t {
  CR,
  LF,
  Control,
  Extend,
  ZWJ,
  Regional_Indicator,
  Prepend,
  SpacingMark,
  L,
  V,
  T,
  LV,
  LVT,
  Other,
  Count,
};

enum class GraphemeInCB : uint8_t {
  None,
  Linker,
  Consonant,
  Extend,
  Count,
};

struct GraphemeRange {
  uint32_t first;
  uint32_t last;
  uint8_t value;
};

[[nodiscard]] GraphemeBreakClass graphemeBreakClass(uint32_t codePoint) noexcept;
[[nodiscard]] bool isExtendedPictographic(uint32_t codePoint) noexcept;
[[nodiscard]] GraphemeInCB graphemeInCB(uint32_t codePoint) noexcept;
[[nodiscard]] const GraphemeRange* graphemeRanges() noexcept;
[[nodiscard]] size_t graphemeRangeCount() noexcept;

inline constexpr GraphemeBreakClass kAsciiGraphemeClasses[128] = {
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::LF,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::CR,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Control,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Other,
  GraphemeBreakClass::Control,
};

} // namespace coral::ui::unicode

#endif // CORAL_UI_UNICODE_GRAPHEMEBREAKPROPERTIES_GENERATED_H
