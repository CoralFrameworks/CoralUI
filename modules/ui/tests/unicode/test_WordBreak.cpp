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

#include "unicode/WordBreak.h"

#include <charconv>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef CORAL_UNICODE_TEST_DATA_DIR
#error "CORAL_UNICODE_TEST_DATA_DIR must point at libs/ui/tests/unicode"
#endif

namespace {

using coral::ui::unicode::WordBreakClass;
using coral::ui::unicode::computeWordBreaks;
using coral::ui::unicode::wordBreakClass;
using coral::ui::unicode::wordRangeAt;

void appendUtf8(std::string& text, const uint32_t codePoint) {
  if (codePoint < 0x80u) {
    text.push_back(static_cast<char>(codePoint));
  } else if (codePoint < 0x800u) {
    text.push_back(static_cast<char>(0xC0u | (codePoint >> 6)));
    text.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
  } else if (codePoint < 0x10000u) {
    text.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
    text.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
    text.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
  } else {
    text.push_back(static_cast<char>(0xF0u | (codePoint >> 18)));
    text.push_back(static_cast<char>(0x80u | ((codePoint >> 12) & 0x3Fu)));
    text.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
    text.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
  }
}

bool parseHex(const std::string& token, uint32_t& value) {
  if (token.empty() || token.size() > 6) return false;
  const auto result = std::from_chars(token.data(), token.data() + token.size(), value, 16);
  return result.ec == std::errc{} && result.ptr == token.data() + token.size();
}

} // namespace

TEST(WordBreakProperties, RepresentativeClasses) {
  EXPECT_EQ(wordBreakClass(0x000Du), WordBreakClass::CR);
  EXPECT_EQ(wordBreakClass(0x000Au), WordBreakClass::LF);
  EXPECT_EQ(wordBreakClass('a'), WordBreakClass::ALetter);
  EXPECT_EQ(wordBreakClass('5'), WordBreakClass::Numeric);
  EXPECT_EQ(wordBreakClass('_'), WordBreakClass::ExtendNumLet);
  EXPECT_EQ(wordBreakClass(0x05D0u), WordBreakClass::Hebrew_Letter);
  EXPECT_EQ(wordBreakClass(0x30A2u), WordBreakClass::Katakana);
  EXPECT_EQ(wordBreakClass(0x1F1E6u), WordBreakClass::Regional_Indicator);
  EXPECT_EQ(wordBreakClass(0x200Du), WordBreakClass::ZWJ);
  ASSERT_GT(coral::ui::unicode::wordBreakRangeCount(), 0u);
  EXPECT_EQ(coral::ui::unicode::wordBreakRangeAt(0).first, 0u);
  EXPECT_EQ(coral::ui::unicode::wordBreakRangeAt(
              coral::ui::unicode::wordBreakRangeCount() - 1).last, 0x10FFFFu);
}

TEST(WordBreak, WordRangeAtSelectsTheEnclosingSegment) {
  EXPECT_EQ(wordRangeAt("hello world", 6), (std::pair<size_t, size_t>{ 6, 11 }));
  EXPECT_EQ(wordRangeAt("hello world", 2), (std::pair<size_t, size_t>{ 0, 5 }));
  EXPECT_EQ(wordRangeAt("one.two", 4), (std::pair<size_t, size_t>{ 0, 7 }));
  EXPECT_EQ(wordRangeAt("a b", 1), (std::pair<size_t, size_t>{ 1, 2 }));
}

TEST(WordBreak, OfficialConformanceFile) {
  std::ifstream input(std::string(CORAL_UNICODE_TEST_DATA_DIR) + "/WordBreakTest.txt");
  ASSERT_TRUE(input.good()) << "cannot open WordBreakTest.txt";
  size_t cases = 0;
  size_t failures = 0;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') continue;
    if (const size_t comment = line.find('#'); comment != std::string::npos) {
      line.resize(comment);
    }
    std::istringstream stream(line);
    std::string token;
    std::string text;
    std::vector<size_t> expected;
    uint32_t codePoint = 0;
    while (stream >> token) {
      if (token == "\xC3\xB7") { // ÷
        expected.push_back(text.size());
      } else if (token == "\xC3\x97") { // ×
        continue;
      } else if (parseHex(token, codePoint)) {
        appendUtf8(text, codePoint);
      }
    }
    if (expected.empty()) continue;
    ++cases;
    const std::vector<size_t> actual = computeWordBreaks(text);
    if (actual != expected) {
      if (failures < 5) {
        std::ostringstream message;
        message << "case " << cases << " text bytes=" << text.size() << " expected:";
        for (const size_t value: expected) message << ' ' << value;
        message << " actual:";
        for (const size_t value: actual) message << ' ' << value;
        ADD_FAILURE() << message.str();
      }
      ++failures;
    }
  }
  EXPECT_EQ(failures, 0u) << cases << " cases";
  EXPECT_GT(cases, 1000u);
}
