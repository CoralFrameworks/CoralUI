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

#include <gtest/gtest.h>

#include "unicode/BidiMirroring.generated.h"
#include "unicode/BidiProperties.generated.h"
#include "unicode/UnicodeVersion.generated.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef CORAL_UNICODE_DATA_DIR
#error "CORAL_UNICODE_DATA_DIR must point at tools/unicode/data"
#endif

namespace {

using coral::ui::unicode::BidiClass;
using coral::ui::unicode::BidiRange;

constexpr uint32_t kMaxCodePoint = 0x10FFFFu;
constexpr size_t kCodePointCount = 0x110000u;

struct UcdRange {
  uint32_t first = 0;
  uint32_t last = 0;
  std::string value;
};

const char* const kClassNames[] = {
  "L",
  "R",
  "AL",
  "EN",
  "ES",
  "ET",
  "AN",
  "CS",
  "NSM",
  "BN",
  "B",
  "S",
  "WS",
  "ON",
  "LRE",
  "LRO",
  "RLE",
  "RLO",
  "PDF",
  "LRI",
  "RLI",
  "FSI",
  "PDI",
};

std::string trim(const std::string& value) {
  const size_t first = value.find_first_not_of(" \t\r");
  if (first == std::string::npos) return {};
  const size_t last = value.find_last_not_of(" \t\r");
  return value.substr(first, last - first + 1);
}

bool parseHex(const std::string& token, uint32_t& value) {
  if (token.empty() || token.size() > 6) return false;
  uint32_t parsed = 0;
  for (const char character: token) {
    uint32_t digit = 0;
    if (character >= '0' && character <= '9') {
      digit = static_cast<uint32_t>(character - '0');
    } else if (character >= 'A' && character <= 'F') {
      digit = static_cast<uint32_t>(character - 'A' + 10);
    } else if (character >= 'a' && character <= 'f') {
      digit = static_cast<uint32_t>(character - 'a' + 10);
    } else {
      return false;
    }
    parsed = parsed * 16 + digit;
  }
  if (parsed > kMaxCodePoint) return false;
  value = parsed;
  return true;
}

bool parseRange(const std::string& token, uint32_t& first, uint32_t& last) {
  const size_t dots = token.find("..");
  if (dots == std::string::npos) {
    if (!parseHex(token, first)) return false;
    last = first;
    return true;
  }
  return parseHex(token.substr(0, dots), first) && parseHex(token.substr(dots + 2), last) && first <= last;
}

std::string normalizeClass(const std::string& name) {
  static const std::pair<const char*, const char*> kAliases[] = {
    { "Left_To_Right", "L" },
    { "Right_To_Left", "R" },
    { "Arabic_Letter", "AL" },
    { "European_Number", "EN" },
    { "European_Separator", "ES" },
    { "European_Terminator", "ET" },
    { "Arabic_Number", "AN" },
    { "Common_Separator", "CS" },
    { "Nonspacing_Mark", "NSM" },
    { "Boundary_Neutral", "BN" },
    { "Paragraph_Separator", "B" },
    { "Segment_Separator", "S" },
    { "White_Space", "WS" },
    { "Other_Neutral", "ON" },
    { "Left_To_Right_Embedding", "LRE" },
    { "Left_To_Right_Override", "LRO" },
    { "Right_To_Left_Embedding", "RLE" },
    { "Right_To_Left_Override", "RLO" },
    { "Pop_Directional_Format", "PDF" },
    { "Left_To_Right_Isolate", "LRI" },
    { "Right_To_Left_Isolate", "RLI" },
    { "First_Strong_Isolate", "FSI" },
    { "Pop_Directional_Isolate", "PDI" },
  };
  for (const auto& alias: kAliases) {
    if (name == alias.first) return alias.second;
  }
  return name;
}

int classId(const std::string& name) {
  for (size_t index = 0; index < std::size(kClassNames); ++index) {
    if (name == kClassNames[index]) return static_cast<int>(index);
  }
  return -1;
}

bool readBidiFile(const std::string& path, std::vector<UcdRange>& ranges, std::vector<UcdRange>& missing,
        std::string& error) {
  std::ifstream input(path);
  if (!input) {
    error = "cannot open " + path;
    return false;
  }
  std::string line;
  size_t lineNumber = 0;
  while (std::getline(input, line)) {
    ++lineNumber;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const size_t missingAt = line.find("@missing:");
    if (missingAt != std::string::npos) {
      const std::string body = trim(line.substr(missingAt + 9));
      const size_t semicolon = body.find(';');
      if (semicolon == std::string::npos) {
        error = path + ":" + std::to_string(lineNumber) + ": malformed @missing";
        return false;
      }
      UcdRange range;
      range.value = normalizeClass(trim(body.substr(semicolon + 1)));
      if (!parseRange(trim(body.substr(0, semicolon)), range.first, range.last)) {
        error = path + ":" + std::to_string(lineNumber) + ": malformed @missing range";
        return false;
      }
      missing.push_back(range);
      continue;
    }
    const size_t comment = line.find('#');
    if (comment != std::string::npos) line.resize(comment);
    line = trim(line);
    if (line.empty()) continue;
    const size_t semicolon = line.find(';');
    if (semicolon == std::string::npos) continue;
    UcdRange range;
    range.value = normalizeClass(trim(line.substr(semicolon + 1)));
    const size_t space = range.value.find_first_of(" \t");
    if (space != std::string::npos) range.value.resize(space);
    if (!parseRange(trim(line.substr(0, semicolon)), range.first, range.last)) {
      error = path + ":" + std::to_string(lineNumber) + ": malformed range";
      return false;
    }
    ranges.push_back(range);
  }
  return true;
}

} // namespace

TEST(BidiProperties, VersionRangesAndClassNameRoundTrip) {
  EXPECT_EQ(std::string(coral::ui::unicode::kUnicodeVersion), "18.0.0");
  EXPECT_EQ(coral::ui::unicode::kBidiClassCount, 23u);
  const BidiRange* ranges = coral::ui::unicode::bidiRanges();
  const size_t count = coral::ui::unicode::bidiRangeCount();
  ASSERT_NE(ranges, nullptr);
  ASSERT_GT(count, 0u);
  EXPECT_EQ(ranges[0].first, 0u);
  EXPECT_EQ(ranges[count - 1].last, kMaxCodePoint);
  for (size_t index = 0; index < count; ++index) {
    EXPECT_LE(ranges[index].first, ranges[index].last);
    if (index != 0) EXPECT_EQ(ranges[index - 1].last + 1, ranges[index].first);
  }
  for (size_t index = 0; index < coral::ui::unicode::kBidiClassCount; ++index) {
    const auto value = static_cast<BidiClass>(index);
    const char* name = coral::ui::unicode::bidiClassName(value);
    BidiClass parsed = BidiClass::L;
    EXPECT_TRUE(coral::ui::unicode::bidiClassFromName(name, parsed)) << name;
    EXPECT_EQ(parsed, value);
  }
}

TEST(BidiProperties, RepresentativeClasses) {
  using coral::ui::unicode::bidiClass;
  EXPECT_EQ(bidiClass('A'), BidiClass::L);
  EXPECT_EQ(bidiClass(0x05D0u), BidiClass::R);   // HEBREW ALEF
  EXPECT_EQ(bidiClass(0x0627u), BidiClass::AL);  // ARABIC ALEF
  EXPECT_EQ(bidiClass(0x0301u), BidiClass::NSM); // COMBINING ACUTE
  EXPECT_EQ(bidiClass(0x0660u), BidiClass::AN);  // ARABIC-INDIC DIGIT ZERO
  EXPECT_EQ(bidiClass('0'), BidiClass::EN);
  EXPECT_EQ(bidiClass(' '), BidiClass::WS);
  EXPECT_EQ(bidiClass(0x2066u), BidiClass::LRI);
  EXPECT_EQ(bidiClass(0x110000u), BidiClass::L);
}

TEST(BidiProperties, EveryMirrorPairMatchesSourceData) {
  const std::string path = std::string(CORAL_UNICODE_DATA_DIR) + "/BidiMirroring.txt";
  std::ifstream input(path);
  ASSERT_TRUE(input) << "cannot open " << path;
  size_t count = 0;
  size_t mismatches = 0;
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const size_t comment = line.find('#');
    if (comment != std::string::npos) line.resize(comment);
    line = trim(line);
    if (line.empty()) continue;
    const size_t semicolon = line.find(';');
    if (semicolon == std::string::npos) continue;
    const uint32_t codePoint = static_cast<uint32_t>(std::stoul(trim(line.substr(0, semicolon)), nullptr, 16));
    const uint32_t mirror = static_cast<uint32_t>(std::stoul(trim(line.substr(semicolon + 1)), nullptr, 16));
    ++count;
    if (coral::ui::unicode::bidiMirror(codePoint) != mirror) ++mismatches;
  }
  EXPECT_EQ(count, 438u);
  EXPECT_EQ(mismatches, 0u);
  EXPECT_EQ(coral::ui::unicode::bidiMirror('('), static_cast<uint32_t>(')'));
  EXPECT_EQ(coral::ui::unicode::bidiMirror('A'), 0u);
}

TEST(BidiProperties, EveryCodePointMatchesUcdSourceData) {
  const std::string path = std::string(CORAL_UNICODE_DATA_DIR) + "/DerivedBidiClass.txt";
  std::vector<UcdRange> ranges;
  std::vector<UcdRange> missing;
  std::string error;
  ASSERT_TRUE(readBidiFile(path, ranges, missing, error)) << error;

  std::vector<uint8_t> expected(kCodePointCount, static_cast<uint8_t>(classId("L")));
  for (const UcdRange& range: missing) {
    const int value = classId(range.value);
    ASSERT_GE(value, 0) << range.value;
    std::fill(expected.begin() + range.first, expected.begin() + range.last + 1, static_cast<uint8_t>(value));
  }
  for (const UcdRange& range: ranges) {
    const int value = classId(range.value);
    ASSERT_GE(value, 0) << range.value;
    std::fill(expected.begin() + range.first, expected.begin() + range.last + 1, static_cast<uint8_t>(value));
  }

  size_t mismatches = 0;
  std::vector<uint32_t> examples;
  for (uint32_t codePoint = 0; codePoint <= kMaxCodePoint; ++codePoint) {
    if (coral::ui::unicode::bidiClass(codePoint) != static_cast<BidiClass>(expected[codePoint])) {
      ++mismatches;
      if (examples.size() < 16) examples.push_back(codePoint);
    }
  }
  std::ostringstream output;
  for (const uint32_t codePoint: examples) output << " U+" << std::hex << codePoint;
  EXPECT_EQ(mismatches, 0u) << "first mismatches:" << output.str();
}
