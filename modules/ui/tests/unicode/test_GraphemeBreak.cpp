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

#include "unicode/GraphemeBreak.h"
#include "unicode/GraphemeBreakProperties.generated.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef CORAL_UNICODE_TEST_DATA_DIR
#error "CORAL_UNICODE_TEST_DATA_DIR must point at libs/ui/tests/unicode"
#endif

namespace {

using coral::ui::unicode::GraphemeBreakClass;
using coral::ui::unicode::GraphemeInCB;

std::string encodeUtf8(const uint32_t codePoint) {
  std::string out;
  if (codePoint < 0x80u) {
    out.push_back(static_cast<char>(codePoint));
  } else if (codePoint < 0x800u) {
    out.push_back(static_cast<char>(0xC0u | (codePoint >> 6)));
    out.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
  } else if (codePoint < 0x10000u) {
    out.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
    out.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
  } else {
    out.push_back(static_cast<char>(0xF0u | (codePoint >> 18)));
    out.push_back(static_cast<char>(0x80u | ((codePoint >> 12) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
  }
  return out;
}

std::string formatBoundaries(const std::vector<size_t>& boundaries) {
  std::ostringstream out;
  for (const size_t boundary: boundaries) out << ' ' << boundary;
  return out.str();
}

} // namespace

TEST(GraphemeBreak, OfficialTestPassesEveryBoundary) {
  const std::string path = std::string(CORAL_UNICODE_TEST_DATA_DIR) + "/GraphemeBreakTest.txt";
  std::ifstream input(path);
  ASSERT_TRUE(input) << "cannot open " << path;

  size_t lineNumber = 0;
  size_t cases = 0;
  size_t failures = 0;
  std::string line;
  while (std::getline(input, line)) {
    ++lineNumber;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const size_t comment = line.find('#');
    if (comment != std::string::npos) line.resize(comment);
    std::istringstream tokens(line);
    std::vector<std::string> fields;
    for (std::string token; tokens >> token;) fields.push_back(token);
    if (fields.size() < 3) continue;

    std::string text;
    std::vector<size_t> expected;
    std::vector<size_t> offsets;
    offsets.push_back(0);
    bool malformed = false;
    size_t codePointCount = 0;
    for (size_t index = 0; index + 1 < fields.size(); index += 2) {
      const std::string& symbol = fields[index];
      if (symbol != "\xC3\xB7" && symbol != "\xC3\x97") {
        malformed = true;
        break;
      }
      if (index == 0 && symbol == "\xC3\xB7") expected.push_back(0);
      const std::string& value = fields[index + 1];
      const uint32_t codePoint = static_cast<uint32_t>(std::stoul(value, nullptr, 16));
      text += encodeUtf8(codePoint);
      offsets.push_back(text.size());
      ++codePointCount;
    }
    if (malformed || codePointCount == 0) continue;
    const std::string& finalSymbol = fields.back();
    if (finalSymbol != "\xC3\xB7" && finalSymbol != "\xC3\x97") continue;
    for (size_t index = 1; index < codePointCount; ++index) {
      if (fields[index * 2] == "\xC3\xB7") expected.push_back(offsets[index]);
    }
    if (finalSymbol == "\xC3\xB7") expected.push_back(offsets.back());
    ++cases;

    const std::vector<size_t> actual = coral::ui::unicode::computeGraphemeBreaks(text);
    if (actual != expected) {
      ++failures;
      if (failures <= 8) {
        ADD_FAILURE() << "line " << lineNumber << "\n  expected:" << formatBoundaries(expected)
                      << "\n  actual:  " << formatBoundaries(actual);
      }
    }
  }
  EXPECT_EQ(cases, 853u);
  EXPECT_EQ(failures, 0u) << failures << " grapheme cases failed";
}

TEST(GraphemeBreak, BasicClassesAndContextRules) {
  using coral::ui::unicode::graphemeBreakClass;
  EXPECT_EQ(graphemeBreakClass('\r'), GraphemeBreakClass::CR);
  EXPECT_EQ(graphemeBreakClass('\n'), GraphemeBreakClass::LF);
  EXPECT_EQ(graphemeBreakClass(0x0308u), GraphemeBreakClass::Extend);
  EXPECT_EQ(graphemeBreakClass(0x200Du), GraphemeBreakClass::ZWJ);
  EXPECT_EQ(graphemeBreakClass(0x1F1E6u), GraphemeBreakClass::Regional_Indicator);
  EXPECT_TRUE(coral::ui::unicode::isExtendedPictographic(0x1F600u));
  const std::string combining = "a\xCC\x88";
  EXPECT_TRUE(coral::ui::unicode::isGraphemeBoundary(combining, 0));
  EXPECT_FALSE(coral::ui::unicode::isGraphemeBoundary(combining, 1));
  EXPECT_TRUE(coral::ui::unicode::isGraphemeBoundary(combining, combining.size()));
  EXPECT_EQ(coral::ui::unicode::graphemeInCB(0x094Du), GraphemeInCB::Linker);
  EXPECT_EQ(coral::ui::unicode::graphemeInCB(0x0915u), GraphemeInCB::Consonant);
}
