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
 * Generated from Unicode 18.0.0 LineBreak.txt and DerivedGeneralCategory.txt.
 * DO NOT EDIT BY HAND. Regenerate with:
 *
 *   python tools/unicode/generate_linebreak_properties.py
 *
 * LineBreak.txt:           https://www.unicode.org/Public/18.0.0/ucd/LineBreak.txt
 *                          sha256 ae8cf1970c73f3f1a12d77852df96c4b2b1723ca8b388e36bb5739250c6849de
 * DerivedGeneralCategory:  https://www.unicode.org/Public/18.0.0/ucd/extracted/DerivedGeneralCategory.txt
 *                          sha256 d6b151d2d40ee9b1876d26f417980f45ffae47b6055ccf7203cb31f07a030f94
 * EastAsianWidth:           https://www.unicode.org/Public/18.0.0/ucd/EastAsianWidth.txt
 *                          sha256 a0cf29eacd00cfcaec4381c6b7c281685f18dbb4e7ff82b4076ccb342ca839aa
 * emoji-data:              https://www.unicode.org/Public/18.0.0/ucd/emoji/emoji-data.txt
 *                          sha256 80d00f8e616a0ef27fd6b8de3b758c06383b5d917e2977709578e68baf733bf1
 */

#ifndef CORAL_UI_UNICODE_LINEBREAKPROPERTIES_GENERATED_H
#define CORAL_UI_UNICODE_LINEBREAKPROPERTIES_GENERATED_H

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace coral::ui::unicode {

/** Line_Break classes of Unicode 18.0.0. The order is the UCD alphabetical order. */
enum class LineBreakClass : uint8_t {
  AI,
  AK,
  AL,
  AP,
  AS,
  B2,
  BA,
  BB,
  BK,
  CB,
  CJ,
  CL,
  CM,
  CP,
  CR,
  EB,
  EM,
  EX,
  GL,
  H2,
  H3,
  HH,
  HL,
  HY,
  ID,
  IN,
  IS,
  JL,
  JT,
  JV,
  LF,
  NL,
  NS,
  NU,
  OP,
  PO,
  PR,
  QU,
  RI,
  SA,
  SG,
  SP,
  SY,
  VF,
  VI,
  WJ,
  XX,
  ZW,
  ZWJ,
  Count,
};

inline constexpr size_t kLineBreakClassCount =
  static_cast<size_t>(LineBreakClass::Count);

/** One run of code points that share the raw and LB1-resolved Line_Break class. */
struct LineBreakRange {
  uint32_t first;
  uint32_t last;
  LineBreakClass value;
  LineBreakClass resolved;
};

/** @return the raw Line_Break property; XX for unassigned or invalid code points. */
[[nodiscard]] LineBreakClass lineBreakClass(uint32_t codePoint) noexcept;

/**
 * @return the class after UAX #14 LB1 (AI/SG/XX -> AL, CJ -> NS, SA -> CM or AL).
 * Invalid code points return XX; LB9/LB10 attachment is a runtime concern, not LB1.
 */
[[nodiscard]] LineBreakClass resolveLineBreakClass(uint32_t codePoint) noexcept;

/** @return the UCD name, e.g. "ID"; never null. */
[[nodiscard]] const char* lineBreakClassName(LineBreakClass value) noexcept;

/** @return true and sets @p value when @p name is a known class name. */
[[nodiscard]] bool lineBreakClassFromName(std::string_view name,
  LineBreakClass& value) noexcept;

/**
 * @return range @p index of the 3685 contiguous ranges, materialized on demand.
 * The generated table stores only end boundaries and class-pair ids to stay compact.
 */
[[nodiscard]] LineBreakRange lineBreakRangeAt(size_t index) noexcept;
[[nodiscard]] size_t lineBreakRangeCount() noexcept;

/** Context bits from properties other than Line_Break (UAX #14 LB15a/b, LB19a, LB30, LB30b). */
inline constexpr uint8_t kLineBreakFlagPi = 1u << 0;
inline constexpr uint8_t kLineBreakFlagPf = 1u << 1;
inline constexpr uint8_t kLineBreakFlagEastAsian = 1u << 2;
inline constexpr uint8_t kLineBreakFlagExtendedPictographicUnassigned = 1u << 3;
inline constexpr uint8_t kLineBreakFlagDottedCircle = 1u << 4;
inline constexpr uint8_t kLineBreakFlagZwjTail = 1u << 5;

struct LineBreakFlagRange {
  uint32_t first;
  uint32_t last;
  uint8_t flags;
};

/** @return the UAX #14 context bits for @p codePoint; 0 when none apply. */
[[nodiscard]] uint8_t lineBreakFlags(uint32_t codePoint) noexcept;

/** @return the 188 non-zero flag ranges. */
[[nodiscard]] const LineBreakFlagRange* lineBreakFlagRanges() noexcept;
[[nodiscard]] size_t lineBreakFlagRangeCount() noexcept;

/** Hot-path table for ASCII: resolved class and context flags without a range search. */
struct AsciiLineBreakProperty {
  LineBreakClass value;
  uint8_t flags;
};

inline constexpr AsciiLineBreakProperty kAsciiLineBreakProperties[128] = {
  { LineBreakClass::CM, 0x00u }, // U+0000
  { LineBreakClass::CM, 0x00u }, // U+0001
  { LineBreakClass::CM, 0x00u }, // U+0002
  { LineBreakClass::CM, 0x00u }, // U+0003
  { LineBreakClass::CM, 0x00u }, // U+0004
  { LineBreakClass::CM, 0x00u }, // U+0005
  { LineBreakClass::CM, 0x00u }, // U+0006
  { LineBreakClass::CM, 0x00u }, // U+0007
  { LineBreakClass::CM, 0x00u }, // U+0008
  { LineBreakClass::BA, 0x00u }, // U+0009
  { LineBreakClass::LF, 0x00u }, // U+000A
  { LineBreakClass::BK, 0x00u }, // U+000B
  { LineBreakClass::BK, 0x00u }, // U+000C
  { LineBreakClass::CR, 0x00u }, // U+000D
  { LineBreakClass::CM, 0x00u }, // U+000E
  { LineBreakClass::CM, 0x00u }, // U+000F
  { LineBreakClass::CM, 0x00u }, // U+0010
  { LineBreakClass::CM, 0x00u }, // U+0011
  { LineBreakClass::CM, 0x00u }, // U+0012
  { LineBreakClass::CM, 0x00u }, // U+0013
  { LineBreakClass::CM, 0x00u }, // U+0014
  { LineBreakClass::CM, 0x00u }, // U+0015
  { LineBreakClass::CM, 0x00u }, // U+0016
  { LineBreakClass::CM, 0x00u }, // U+0017
  { LineBreakClass::CM, 0x00u }, // U+0018
  { LineBreakClass::CM, 0x00u }, // U+0019
  { LineBreakClass::CM, 0x00u }, // U+001A
  { LineBreakClass::CM, 0x00u }, // U+001B
  { LineBreakClass::CM, 0x00u }, // U+001C
  { LineBreakClass::CM, 0x00u }, // U+001D
  { LineBreakClass::CM, 0x00u }, // U+001E
  { LineBreakClass::CM, 0x00u }, // U+001F
  { LineBreakClass::SP, 0x00u }, // U+0020
  { LineBreakClass::EX, 0x00u }, // U+0021
  { LineBreakClass::QU, 0x00u }, // U+0022
  { LineBreakClass::AL, 0x00u }, // U+0023
  { LineBreakClass::PR, 0x00u }, // U+0024
  { LineBreakClass::PO, 0x00u }, // U+0025
  { LineBreakClass::AL, 0x00u }, // U+0026
  { LineBreakClass::QU, 0x00u }, // U+0027
  { LineBreakClass::OP, 0x00u }, // U+0028
  { LineBreakClass::CP, 0x00u }, // U+0029
  { LineBreakClass::AL, 0x00u }, // U+002A
  { LineBreakClass::PR, 0x00u }, // U+002B
  { LineBreakClass::IS, 0x00u }, // U+002C
  { LineBreakClass::HY, 0x00u }, // U+002D
  { LineBreakClass::IS, 0x00u }, // U+002E
  { LineBreakClass::SY, 0x00u }, // U+002F
  { LineBreakClass::NU, 0x00u }, // U+0030
  { LineBreakClass::NU, 0x00u }, // U+0031
  { LineBreakClass::NU, 0x00u }, // U+0032
  { LineBreakClass::NU, 0x00u }, // U+0033
  { LineBreakClass::NU, 0x00u }, // U+0034
  { LineBreakClass::NU, 0x00u }, // U+0035
  { LineBreakClass::NU, 0x00u }, // U+0036
  { LineBreakClass::NU, 0x00u }, // U+0037
  { LineBreakClass::NU, 0x00u }, // U+0038
  { LineBreakClass::NU, 0x00u }, // U+0039
  { LineBreakClass::IS, 0x00u }, // U+003A
  { LineBreakClass::IS, 0x00u }, // U+003B
  { LineBreakClass::AL, 0x00u }, // U+003C
  { LineBreakClass::AL, 0x00u }, // U+003D
  { LineBreakClass::AL, 0x00u }, // U+003E
  { LineBreakClass::EX, 0x00u }, // U+003F
  { LineBreakClass::AL, 0x00u }, // U+0040
  { LineBreakClass::AL, 0x00u }, // U+0041
  { LineBreakClass::AL, 0x00u }, // U+0042
  { LineBreakClass::AL, 0x00u }, // U+0043
  { LineBreakClass::AL, 0x00u }, // U+0044
  { LineBreakClass::AL, 0x00u }, // U+0045
  { LineBreakClass::AL, 0x00u }, // U+0046
  { LineBreakClass::AL, 0x00u }, // U+0047
  { LineBreakClass::AL, 0x00u }, // U+0048
  { LineBreakClass::AL, 0x00u }, // U+0049
  { LineBreakClass::AL, 0x00u }, // U+004A
  { LineBreakClass::AL, 0x00u }, // U+004B
  { LineBreakClass::AL, 0x00u }, // U+004C
  { LineBreakClass::AL, 0x00u }, // U+004D
  { LineBreakClass::AL, 0x00u }, // U+004E
  { LineBreakClass::AL, 0x00u }, // U+004F
  { LineBreakClass::AL, 0x00u }, // U+0050
  { LineBreakClass::AL, 0x00u }, // U+0051
  { LineBreakClass::AL, 0x00u }, // U+0052
  { LineBreakClass::AL, 0x00u }, // U+0053
  { LineBreakClass::AL, 0x00u }, // U+0054
  { LineBreakClass::AL, 0x00u }, // U+0055
  { LineBreakClass::AL, 0x00u }, // U+0056
  { LineBreakClass::AL, 0x00u }, // U+0057
  { LineBreakClass::AL, 0x00u }, // U+0058
  { LineBreakClass::AL, 0x00u }, // U+0059
  { LineBreakClass::AL, 0x00u }, // U+005A
  { LineBreakClass::OP, 0x00u }, // U+005B
  { LineBreakClass::PR, 0x00u }, // U+005C
  { LineBreakClass::CP, 0x00u }, // U+005D
  { LineBreakClass::AL, 0x00u }, // U+005E
  { LineBreakClass::AL, 0x00u }, // U+005F
  { LineBreakClass::AL, 0x00u }, // U+0060
  { LineBreakClass::AL, 0x00u }, // U+0061
  { LineBreakClass::AL, 0x00u }, // U+0062
  { LineBreakClass::AL, 0x00u }, // U+0063
  { LineBreakClass::AL, 0x00u }, // U+0064
  { LineBreakClass::AL, 0x00u }, // U+0065
  { LineBreakClass::AL, 0x00u }, // U+0066
  { LineBreakClass::AL, 0x00u }, // U+0067
  { LineBreakClass::AL, 0x00u }, // U+0068
  { LineBreakClass::AL, 0x00u }, // U+0069
  { LineBreakClass::AL, 0x00u }, // U+006A
  { LineBreakClass::AL, 0x00u }, // U+006B
  { LineBreakClass::AL, 0x00u }, // U+006C
  { LineBreakClass::AL, 0x00u }, // U+006D
  { LineBreakClass::AL, 0x00u }, // U+006E
  { LineBreakClass::AL, 0x00u }, // U+006F
  { LineBreakClass::AL, 0x00u }, // U+0070
  { LineBreakClass::AL, 0x00u }, // U+0071
  { LineBreakClass::AL, 0x00u }, // U+0072
  { LineBreakClass::AL, 0x00u }, // U+0073
  { LineBreakClass::AL, 0x00u }, // U+0074
  { LineBreakClass::AL, 0x00u }, // U+0075
  { LineBreakClass::AL, 0x00u }, // U+0076
  { LineBreakClass::AL, 0x00u }, // U+0077
  { LineBreakClass::AL, 0x00u }, // U+0078
  { LineBreakClass::AL, 0x00u }, // U+0079
  { LineBreakClass::AL, 0x00u }, // U+007A
  { LineBreakClass::OP, 0x00u }, // U+007B
  { LineBreakClass::BA, 0x00u }, // U+007C
  { LineBreakClass::CL, 0x00u }, // U+007D
  { LineBreakClass::AL, 0x00u }, // U+007E
  { LineBreakClass::CM, 0x00u }, // U+007F
};

} // namespace coral::ui::unicode

#endif // CORAL_UI_UNICODE_LINEBREAKPROPERTIES_GENERATED_H
