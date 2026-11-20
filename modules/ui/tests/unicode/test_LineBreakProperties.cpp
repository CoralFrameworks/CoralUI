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

#include "unicode/LineBreakProperties.generated.h"
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

using coral::ui::unicode::LineBreakClass;
using coral::ui::unicode::LineBreakRange;

constexpr uint32_t kMaxCodePoint = 0x10FFFFu;
constexpr size_t kCodePointCount = 0x110000u;

struct UcdRange {
  uint32_t first = 0;
  uint32_t last = 0;
  std::string value;
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

/**
 * A small independent reader for the UCD property files. It intentionally does
 * not share code with the generator: its job is to catch a generator that
 * mis-parses a range, drops the @missing default or resolves SA incorrectly.
 */
bool readUcdFile(const std::string& path, std::vector<UcdRange>& ranges, std::vector<UcdRange>& missing,
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
        error = path + ":" + std::to_string(lineNumber) + ": malformed @missing line";
        return false;
      }
      UcdRange range;
      range.value = trim(body.substr(semicolon + 1));
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
    range.value = trim(line.substr(semicolon + 1));
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

LineBreakClass expectedResolved(const LineBreakClass raw, const bool mark) {
  switch (raw) {
    case LineBreakClass::AI:
    case LineBreakClass::SG:
    case LineBreakClass::XX:
      return LineBreakClass::AL;
    case LineBreakClass::CJ:
      return LineBreakClass::NS;
    case LineBreakClass::SA:
      return mark ? LineBreakClass::CM : LineBreakClass::AL;
    default:
      return raw;
  }
}

std::string describeCodePoints(const std::vector<uint32_t>& examples) {
  std::ostringstream output;
  for (size_t index = 0; index < examples.size(); ++index) {
    if (index != 0) output << ' ';
    output << "U+" << std::hex << std::uppercase << examples[index];
  }
  return output.str();
}

void collectExample(std::vector<uint32_t>& examples, const uint32_t codePoint) {
  if (examples.size() < 16) examples.push_back(codePoint);
}

} // namespace

TEST(UnicodeLineBreakProperties, VersionAndClassVocabulary) {
  using namespace coral::ui::unicode;
  EXPECT_EQ(std::string(kUnicodeVersion), "18.0.0");
  EXPECT_FALSE(kUnicodeDataGeneratorVersion.empty());
  EXPECT_EQ(kLineBreakClassCount, 49u);

  const std::string_view hashes[] = {
    kLineBreakDataSha256,
    kGeneralCategoryDataSha256,
    kEastAsianWidthDataSha256,
    kEmojiDataSha256,
    kLineBreakTestSha256,
  };
  for (const std::string_view hash: hashes) {
    EXPECT_EQ(hash.size(), 64u);
    for (const char character: hash) {
      EXPECT_TRUE((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')) << hash;
    }
  }

  for (size_t index = 0; index < kLineBreakClassCount; ++index) {
    const auto value = static_cast<LineBreakClass>(index);
    const char* name = lineBreakClassName(value);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
    LineBreakClass parsed = LineBreakClass::XX;
    EXPECT_TRUE(lineBreakClassFromName(name, parsed)) << name;
    EXPECT_EQ(parsed, value);
  }
  LineBreakClass unknown = LineBreakClass::XX;
  EXPECT_FALSE(lineBreakClassFromName("nope", unknown));
}

TEST(UnicodeLineBreakProperties, RepresentativeClassesAndLb1Resolution) {
  using namespace coral::ui::unicode;
  struct Expectation {
    uint32_t codePoint;
    LineBreakClass raw;
    LineBreakClass resolved;
  };
  const Expectation expectations[] = {
    { 0x000A, LineBreakClass::LF, LineBreakClass::LF },
    { 0x0020, LineBreakClass::SP, LineBreakClass::SP },
    { 0x00A0, LineBreakClass::GL, LineBreakClass::GL },
    { 0x0E01, LineBreakClass::SA, LineBreakClass::AL }, // Thai base letter
    { 0x0E31, LineBreakClass::SA, LineBreakClass::CM }, // Thai Mn mark, LB1 -> CM
    { 0x1100, LineBreakClass::JL, LineBreakClass::JL },
    { 0x1160, LineBreakClass::JV, LineBreakClass::JV },
    { 0x11A8, LineBreakClass::JT, LineBreakClass::JT },
    { 0x200B, LineBreakClass::ZW, LineBreakClass::ZW },
    { 0x200D, LineBreakClass::ZWJ, LineBreakClass::ZWJ },
    { 0x3000, LineBreakClass::BA, LineBreakClass::BA },
    { 0x4E00, LineBreakClass::ID, LineBreakClass::ID },
    { 0xAC00, LineBreakClass::H2, LineBreakClass::H2 },
    { 0x1F1E6, LineBreakClass::RI, LineBreakClass::RI },
    { 0x1F3FB, LineBreakClass::EM, LineBreakClass::EM },
    { 0x1F600, LineBreakClass::ID, LineBreakClass::ID },
  };
  for (const Expectation& expectation: expectations) {
    EXPECT_EQ(lineBreakClass(expectation.codePoint), expectation.raw) << "U+" << std::hex << expectation.codePoint;
    EXPECT_EQ(resolveLineBreakClass(expectation.codePoint), expectation.resolved)
            << "U+" << std::hex << expectation.codePoint;
  }

  // Invalid input is rejected as XX rather than silently wrapped into the table.
  EXPECT_EQ(lineBreakClass(0x110000u), LineBreakClass::XX);
  EXPECT_EQ(resolveLineBreakClass(0xFFFFFFFFu), LineBreakClass::XX);

  // Context flags come from properties other than Line_Break.
  EXPECT_EQ(lineBreakFlags(0x25CCu), kLineBreakFlagDottedCircle);
  EXPECT_EQ(lineBreakFlags(0xFF08u), kLineBreakFlagEastAsian);
  EXPECT_EQ(lineBreakFlags(0x201Cu), kLineBreakFlagPi);
  EXPECT_EQ(lineBreakFlags(0x201Du), kLineBreakFlagPf);
  EXPECT_EQ(lineBreakFlags(0x2010u), 0u);
  EXPECT_EQ(lineBreakFlags(0x1F02Cu), static_cast<uint8_t>(kLineBreakFlagExtendedPictographicUnassigned));
}

TEST(UnicodeLineBreakProperties, RangeTableIsSortedDisjointAndComplete) {
  using namespace coral::ui::unicode;
  const size_t count = lineBreakRangeCount();
  ASSERT_GT(count, 0u);

  EXPECT_EQ(lineBreakRangeAt(0).first, 0u);
  EXPECT_EQ(lineBreakRangeAt(count - 1).last, kMaxCodePoint);
  for (size_t index = 0; index < count; ++index) {
    const LineBreakRange range = lineBreakRangeAt(index);
    EXPECT_LE(range.first, range.last) << index;
    if (index != 0) {
      EXPECT_EQ(lineBreakRangeAt(index - 1).last + 1, range.first) << "gap or overlap before range " << index;
    }
    EXPECT_NE(range.value, LineBreakClass::Count) << index;
    EXPECT_NE(range.resolved, LineBreakClass::Count) << index;
  }
}

TEST(UnicodeLineBreakProperties, EveryCodePointMatchesUcdSourceData) {
  using namespace coral::ui::unicode;
  const std::string dataDirectory = CORAL_UNICODE_DATA_DIR;
  ASSERT_FALSE(dataDirectory.empty());

  std::vector<UcdRange> sourceLineBreakRanges;
  std::vector<UcdRange> lineBreakMissing;
  std::vector<UcdRange> categoryRanges;
  std::vector<UcdRange> categoryMissing;
  std::string error;
  ASSERT_TRUE(readUcdFile(dataDirectory + "/LineBreak.txt", sourceLineBreakRanges, lineBreakMissing, error)) << error;
  ASSERT_TRUE(readUcdFile(dataDirectory + "/DerivedGeneralCategory.txt", categoryRanges, categoryMissing, error))
          << error;

  std::vector<int32_t> expectedRaw(kCodePointCount, -1);
  std::vector<uint8_t> expectedMark(kCodePointCount, 0);
  const auto applyMissing = [](const std::vector<UcdRange>& missing, std::vector<int32_t>& table) {
    for (const UcdRange& range: missing) {
      LineBreakClass value = LineBreakClass::XX;
      if (!lineBreakClassFromName(range.value, value)) return false;
      std::fill(table.begin() + range.first, table.begin() + range.last + 1, static_cast<int32_t>(value));
    }
    return true;
  };
  ASSERT_TRUE(applyMissing(lineBreakMissing, expectedRaw));
  for (const UcdRange& range: sourceLineBreakRanges) {
    LineBreakClass value = LineBreakClass::XX;
    ASSERT_TRUE(lineBreakClassFromName(range.value, value)) << range.value;
    std::fill(expectedRaw.begin() + range.first, expectedRaw.begin() + range.last + 1, static_cast<int32_t>(value));
  }

  // DerivedGeneralCategory has no @missing directive in Unicode 18; defaults to
  // "not Mn/Mc" if a future release ever leaves an unlisted code point.
  for (const UcdRange& range: categoryMissing) {
    const bool mark = range.value == "Mn" || range.value == "Mc";
    std::fill(expectedMark.begin() + range.first, expectedMark.begin() + range.last + 1, static_cast<uint8_t>(mark));
  }
  for (const UcdRange& range: categoryRanges) {
    const bool mark = range.value == "Mn" || range.value == "Mc";
    std::fill(expectedMark.begin() + range.first, expectedMark.begin() + range.last + 1, static_cast<uint8_t>(mark));
  }

  size_t rawMismatch = 0;
  size_t resolvedMismatch = 0;
  std::vector<uint32_t> rawExamples;
  std::vector<uint32_t> resolvedExamples;
  for (uint32_t codePoint = 0; codePoint <= kMaxCodePoint; ++codePoint) {
    const int32_t expected = expectedRaw[codePoint];
    if (expected < 0) {
      collectExample(rawExamples, codePoint);
      ++rawMismatch;
      continue;
    }
    const auto raw = static_cast<LineBreakClass>(expected);
    if (lineBreakClass(codePoint) != raw) {
      collectExample(rawExamples, codePoint);
      ++rawMismatch;
    }
    const LineBreakClass resolved = expectedResolved(raw, expectedMark[codePoint] != 0);
    if (resolveLineBreakClass(codePoint) != resolved) {
      collectExample(resolvedExamples, codePoint);
      ++resolvedMismatch;
    }
  }

  EXPECT_EQ(rawMismatch, 0u) << "first mismatches: " << describeCodePoints(rawExamples);
  EXPECT_EQ(resolvedMismatch, 0u) << "first mismatches: " << describeCodePoints(resolvedExamples);

  // Every generated range must agree with the source data at both ends.
  for (size_t index = 0; index < lineBreakRangeCount(); ++index) {
    const LineBreakRange range = lineBreakRangeAt(index);
    for (const uint32_t codePoint: { range.first, range.last }) {
      const int32_t expected = expectedRaw[codePoint];
      ASSERT_GE(expected, 0) << "range " << index << " U+" << std::hex << codePoint;
      EXPECT_EQ(range.value, static_cast<LineBreakClass>(expected)) << "U+" << std::hex << codePoint;
      EXPECT_EQ(range.resolved, expectedResolved(static_cast<LineBreakClass>(expected), expectedMark[codePoint] != 0))
              << "U+" << std::hex << codePoint;
    }
  }
}
TEST(UnicodeLineBreakProperties, FlagRangesAreSortedAndDisjoint) {
  using namespace coral::ui::unicode;
  const LineBreakFlagRange* ranges = lineBreakFlagRanges();
  const size_t count = lineBreakFlagRangeCount();
  ASSERT_NE(ranges, nullptr);
  EXPECT_GT(count, 0u);
  for (size_t index = 0; index < count; ++index) {
    const LineBreakFlagRange& range = ranges[index];
    EXPECT_LE(range.first, range.last) << index;
    EXPECT_NE(range.flags, 0u) << index;
    EXPECT_LE(range.last, kMaxCodePoint) << index;
    if (index != 0) {
      EXPECT_LT(ranges[index - 1].last, range.first) << "overlap at range " << index;
    }
  }
}

TEST(UnicodeLineBreakProperties, EveryCodePointMatchesFlagSourceData) {
  using namespace coral::ui::unicode;
  const std::string dataDirectory = CORAL_UNICODE_DATA_DIR;
  ASSERT_FALSE(dataDirectory.empty());

  std::vector<UcdRange> categoryRanges;
  std::vector<UcdRange> categoryMissing;
  std::vector<UcdRange> widthRanges;
  std::vector<UcdRange> widthMissing;
  std::vector<UcdRange> emojiRanges;
  std::vector<UcdRange> emojiMissing;
  std::string error;
  ASSERT_TRUE(readUcdFile(dataDirectory + "/DerivedGeneralCategory.txt", categoryRanges, categoryMissing, error))
          << error;
  ASSERT_TRUE(readUcdFile(dataDirectory + "/EastAsianWidth.txt", widthRanges, widthMissing, error)) << error;
  ASSERT_TRUE(readUcdFile(dataDirectory + "/emoji-data.txt", emojiRanges, emojiMissing, error)) << error;

  static const char* const kCategories[] = {
    "Cc",
    "Cf",
    "Cn",
    "Co",
    "Cs",
    "Ll",
    "Lm",
    "Lo",
    "Lt",
    "Lu",
    "Mc",
    "Me",
    "Mn",
    "Nd",
    "Nl",
    "No",
    "Pc",
    "Pd",
    "Pe",
    "Pf",
    "Pi",
    "Po",
    "Ps",
    "Sc",
    "Sk",
    "Sm",
    "So",
    "Zl",
    "Zp",
    "Zs",
  };
  const auto categoryId = [](const std::string& name) -> int {
    for (size_t index = 0; index < std::size(kCategories); ++index) {
      if (name == kCategories[index]) return static_cast<int>(index);
    }
    return -1;
  };

  const int cn = categoryId("Cn");
  ASSERT_GE(cn, 0);
  std::vector<uint8_t> categories(kCodePointCount, static_cast<uint8_t>(cn));
  for (const UcdRange& range: categoryMissing) {
    const int value = categoryId(range.value);
    ASSERT_GE(value, 0) << range.value;
    std::fill(categories.begin() + range.first, categories.begin() + range.last + 1, static_cast<uint8_t>(value));
  }
  for (const UcdRange& range: categoryRanges) {
    const int value = categoryId(range.value);
    ASSERT_GE(value, 0) << range.value;
    std::fill(categories.begin() + range.first, categories.begin() + range.last + 1, static_cast<uint8_t>(value));
  }

  std::vector<uint8_t> eastAsian(kCodePointCount, 0);
  for (const UcdRange& range: widthMissing) {
    const bool value = range.value == "F" || range.value == "W" || range.value == "H";
    std::fill(eastAsian.begin() + range.first, eastAsian.begin() + range.last + 1, static_cast<uint8_t>(value));
  }
  for (const UcdRange& range: widthRanges) {
    const bool value = range.value == "F" || range.value == "W" || range.value == "H";
    std::fill(eastAsian.begin() + range.first, eastAsian.begin() + range.last + 1, static_cast<uint8_t>(value));
  }

  std::vector<uint8_t> pictographic(kCodePointCount, 0);
  for (const UcdRange& range: emojiRanges) {
    if (range.value != "Extended_Pictographic") continue;
    std::fill(pictographic.begin() + range.first, pictographic.begin() + range.last + 1, 1);
  }

  size_t mismatches = 0;
  std::vector<uint32_t> examples;
  for (uint32_t codePoint = 0; codePoint <= kMaxCodePoint; ++codePoint) {
    const std::string category = kCategories[categories[codePoint]];
    uint8_t expected = 0;
    if (category == "Pi") expected |= kLineBreakFlagPi;
    if (category == "Pf") expected |= kLineBreakFlagPf;
    if (eastAsian[codePoint]) expected |= kLineBreakFlagEastAsian;
    if (pictographic[codePoint] && category == "Cn") {
      expected |= kLineBreakFlagExtendedPictographicUnassigned;
    }
    if (codePoint == 0x25CCu) expected |= kLineBreakFlagDottedCircle;
    if (lineBreakFlags(codePoint) != expected) {
      ++mismatches;
      collectExample(examples, codePoint);
    }
  }
  EXPECT_EQ(mismatches, 0u) << "first mismatches: " << describeCodePoints(examples);
}
