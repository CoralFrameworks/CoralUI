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

#include "unicode/BidiProperties.generated.h"
#include "unicode/BidiResolver.h"

#include <ui/TextBidi.h>

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef CORAL_UNICODE_TEST_DATA_DIR
#error "CORAL_UNICODE_TEST_DATA_DIR must point at libs/ui/tests/unicode"
#endif

namespace {

using coral::ui::unicode::BidiClass;
using coral::ui::unicode::BidiParagraphResult;
using coral::ui::unicode::resolveBidiParagraph;

struct CharacterCase {
  std::vector<uint32_t> codePoints;
  int direction = 2;
  uint8_t baseLevel = 0;
  std::vector<int> levels; // -1 for x
  std::vector<size_t> order;
  size_t line = 0;
};

std::vector<std::string> split(const std::string& text, const char delimiter) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (start <= text.size()) {
    const size_t end = text.find(delimiter, start);
    if (end == std::string::npos) {
      parts.push_back(text.substr(start));
      break;
    }
    parts.push_back(text.substr(start, end - start));
    start = end + 1;
  }
  return parts;
}

std::string trim(std::string value) {
  const size_t first = value.find_first_not_of(" \t\r");
  if (first == std::string::npos) return {};
  const size_t last = value.find_last_not_of(" \t\r");
  return value.substr(first, last - first + 1);
}

bool parseCharacterFile(const std::string& path, std::vector<CharacterCase>& cases, std::string& error) {
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
    const size_t comment = line.find('#');
    if (comment != std::string::npos) line.resize(comment);
    line = trim(line);
    if (line.empty()) continue;
    const std::vector<std::string> fields = split(line, ';');
    if (fields.size() != 5) {
      error = "line " + std::to_string(lineNumber) + ": expected 5 fields";
      return false;
    }
    CharacterCase testCase;
    testCase.line = lineNumber;
    std::istringstream codePoints(fields[0]);
    std::string token;
    while (codePoints >> token) {
      testCase.codePoints.push_back(static_cast<uint32_t>(std::stoul(token, nullptr, 16)));
    }
    testCase.direction = std::stoi(trim(fields[1]));
    testCase.baseLevel = static_cast<uint8_t>(std::stoi(trim(fields[2])));
    std::istringstream levels(fields[3]);
    while (levels >> token) {
      testCase.levels.push_back(token == "x" ? -1 : std::stoi(token));
    }
    std::istringstream order(fields[4]);
    while (order >> token) {
      testCase.order.push_back(static_cast<size_t>(std::stoul(token)));
    }
    if (testCase.levels.size() != testCase.codePoints.size()) {
      error = "line " + std::to_string(lineNumber) + ": level count mismatch";
      return false;
    }
    cases.push_back(std::move(testCase));
  }
  return true;
}

std::string formatCase(const CharacterCase& testCase, const BidiParagraphResult& result) {
  std::ostringstream output;
  output << "line " << testCase.line << "\n  cps:";
  for (const uint32_t codePoint: testCase.codePoints) {
    output << ' ' << std::hex << codePoint;
  }
  output << "\n  expected levels:";
  for (const int level: testCase.levels) {
    if (level < 0) output << " x";
    else
      output << ' ' << std::dec << level;
  }
  output << "\n  actual levels:  ";
  for (const uint8_t level: result.levels) {
    if (level == coral::ui::unicode::kBidiRemovedLevel) output << " x";
    else
      output << ' ' << std::dec << static_cast<int>(level);
  }
  output << "\n  expected order:";
  for (const size_t index: testCase.order) output << ' ' << index;
  output << "\n  actual order:  ";
  for (const size_t index: result.visualOrder) output << ' ' << index;
  return output.str();
}

} // namespace

TEST(BidiConformance, PublicParagraphApiResolvesVisualOrder) {
  // "abc " + Hebrew alef/bet/gimel
  const std::string text = "abc \xD7\x90\xD7\x91\xD7\x92";
  const coral::ui::BidiParagraph paragraph = coral::ui::resolveBidiParagraph(text);
  EXPECT_EQ(paragraph.baseLevel, 0u);
  ASSERT_EQ(paragraph.levels.size(), 7u);
  EXPECT_EQ(paragraph.levels[4], 1u);
  EXPECT_EQ(paragraph.levels[5], 1u);
  EXPECT_EQ(paragraph.levels[6], 1u);
  EXPECT_EQ(paragraph.visualOrder, (std::vector<size_t>{ 0, 1, 2, 3, 6, 5, 4 }));
}

TEST(BidiConformance, PublicParagraphApiSplitsParagraphSeparators) {
  // "abc\n" + Hebrew alef/bet/gimel
  const std::string text = "abc\n\xD7\x90\xD7\x91\xD7\x92";
  const coral::ui::BidiParagraph paragraph = coral::ui::resolveBidiParagraph(text);
  ASSERT_EQ(paragraph.levels.size(), 7u);
  EXPECT_EQ(paragraph.levels[3], 0u);
  EXPECT_EQ(paragraph.levels[4], 1u);
  EXPECT_EQ(paragraph.levels[5], 1u);
  EXPECT_EQ(paragraph.levels[6], 1u);
  EXPECT_EQ(paragraph.visualOrder, (std::vector<size_t>{ 0, 1, 2, 3, 6, 5, 4 }));
}

TEST(BidiConformance, LtrOnlyParagraphSkipsFullUba) {
  // Latin letters, CJK (Bidi_Class L) and emoji (ON) without R/AL/EN/AN.
  const std::string text = "abc \xE4\xB8\xAD\xE6\x96\x87 \xF0\x9F\x99\x82";
  const coral::ui::BidiParagraph paragraph = coral::ui::resolveBidiParagraph(text);
  EXPECT_EQ(paragraph.baseLevel, 0u);
  ASSERT_EQ(paragraph.levels.size(), 8u);
  for (const uint8_t level: paragraph.levels) EXPECT_EQ(level, 0u);
  EXPECT_EQ(paragraph.visualOrder, (std::vector<size_t>{ 0, 1, 2, 3, 4, 5, 6, 7 }));

  // Digits are EN and must still take the full UBA (I1 gives them level 2).
  // Arabic-Indic digit one (AN) is not in the fast-path class set and takes
  // the full UBA, where I1 raises it to level 2.
  const coral::ui::BidiParagraph withArabicNumber =
          coral::ui::resolveBidiParagraph("abc \xD9\xA1");
  ASSERT_EQ(withArabicNumber.levels.size(), 5u);
  EXPECT_EQ(withArabicNumber.levels[4], 2u);
}

TEST(BidiConformance, BidiCharacterTestPasses) {
  std::vector<CharacterCase> cases;
  std::string error;
  ASSERT_TRUE(parseCharacterFile(std::string(CORAL_UNICODE_TEST_DATA_DIR) + "/BidiCharacterTest.txt", cases, error))
          << error;
  EXPECT_FALSE(cases.empty());
  EXPECT_EQ(cases.size(), 91707u);

  size_t failures = 0;
  size_t shown = 0;
  for (const CharacterCase& testCase: cases) {
    const BidiParagraphResult result =
            resolveBidiParagraph(testCase.codePoints, testCase.direction == 2 ? -1 : testCase.direction);
    bool ok = result.baseLevel == testCase.baseLevel && result.visualOrder == testCase.order;
    if (ok) {
      for (size_t index = 0; index < testCase.levels.size(); ++index) {
        const bool removed = testCase.levels[index] < 0;
        const uint8_t actual = result.levels[index];
        if (removed ? actual != coral::ui::unicode::kBidiRemovedLevel
                    : actual != static_cast<uint8_t>(testCase.levels[index])) {
          ok = false;
          break;
        }
      }
    }
    if (!ok) {
      ++failures;
      if (shown < 12) {
        ADD_FAILURE() << formatCase(testCase, result);
        ++shown;
      }
    }
  }
  EXPECT_EQ(failures, 0u) << failures << " of " << cases.size() << " cases failed";
}
namespace {

struct BidiTestGroup {
  std::vector<int> levels;
  std::vector<size_t> order;
};

const std::pair<const char*, uint32_t>& representativeFor(const std::string& name) {
  static const std::pair<const char*, uint32_t> kRepresentatives[] = {
    { "L", 0x0041u },
    { "R", 0x05D0u },
    { "AL", 0x0627u },
    { "EN", 0x0030u },
    { "ES", 0x002Bu },
    { "ET", 0x0024u },
    { "AN", 0x0660u },
    { "CS", 0x002Cu },
    { "NSM", 0x0300u },
    { "BN", 0x00ADu },
    { "B", 0x2029u },
    { "S", 0x0009u },
    { "WS", 0x0020u },
    { "ON", 0x0021u },
    { "LRE", 0x202Au },
    { "LRO", 0x202Du },
    { "RLE", 0x202Bu },
    { "RLO", 0x202Eu },
    { "PDF", 0x202Cu },
    { "LRI", 0x2066u },
    { "RLI", 0x2067u },
    { "FSI", 0x2068u },
    { "PDI", 0x2069u },
  };
  for (const auto& entry: kRepresentatives) {
    if (name == entry.first) return entry;
  }
  static const std::pair<const char*, uint32_t> kUnknown{ "", 0 };
  return kUnknown;
}

} // namespace

TEST(BidiConformance, BidiTestPasses) {
  for (const auto& entry: { std::pair<const char*, uint32_t>{ "L", 0x0041u }, { "BN", 0x00ADu }, { "B", 0x2029u },
           { "S", 0x0009u }, { "ON", 0x0021u } }) {
    ASSERT_STREQ(coral::ui::unicode::bidiClassName(coral::ui::unicode::bidiClass(entry.second)), entry.first)
            << "bad representative for " << entry.first;
  }

  std::ifstream input(std::string(CORAL_UNICODE_TEST_DATA_DIR) + "/BidiTest.txt");
  ASSERT_TRUE(input) << "cannot open BidiTest.txt";
  BidiTestGroup group;
  size_t lineNumber = 0;
  size_t cases = 0;
  size_t directionCases = 0;
  size_t failures = 0;
  size_t shown = 0;
  std::string line;
  while (std::getline(input, line)) {
    ++lineNumber;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const size_t comment = line.find('#');
    if (comment != std::string::npos) line.resize(comment);
    line = trim(line);
    if (line.empty()) continue;
    if (line.rfind("@Levels:", 0) == 0) {
      group.levels.clear();
      std::istringstream stream(line.substr(8));
      std::string token;
      while (stream >> token) {
        group.levels.push_back(token == "x" ? -1 : std::stoi(token));
      }
      continue;
    }
    if (line.rfind("@Reorder:", 0) == 0) {
      group.order.clear();
      std::istringstream stream(line.substr(9));
      std::string token;
      while (stream >> token) group.order.push_back(static_cast<size_t>(std::stoul(token)));
      continue;
    }

    const size_t semicolon = line.find(';');
    ASSERT_NE(semicolon, std::string::npos) << "line " << lineNumber;
    std::vector<uint32_t> codePoints;
    std::istringstream classes(line.substr(0, semicolon));
    std::string name;
    while (classes >> name) {
      const auto& representative = representativeFor(name);
      ASSERT_NE(representative.second, 0u) << "line " << lineNumber << ": unknown class " << name;
      codePoints.push_back(representative.second);
    }
    const int bitset = std::stoi(trim(line.substr(semicolon + 1)), nullptr, 16);
    ASSERT_EQ(group.levels.size(), codePoints.size()) << "line " << lineNumber;
    ++cases;

    for (const int bit: { 1, 2, 4 }) {
      if ((bitset & bit) == 0) continue;
      ++directionCases;
      const int direction = bit == 1 ? -1 : (bit == 2 ? 0 : 1);
      const BidiParagraphResult result = resolveBidiParagraph(codePoints, direction);
      bool ok = result.visualOrder == group.order;
      if (ok) {
        for (size_t index = 0; index < group.levels.size(); ++index) {
          const bool removed = group.levels[index] < 0;
          const uint8_t actual = result.levels[index];
          if (removed ? actual != coral::ui::unicode::kBidiRemovedLevel
                      : actual != static_cast<uint8_t>(group.levels[index])) {
            ok = false;
            break;
          }
        }
      }
      if (!ok) {
        ++failures;
        if (shown < 12) {
          std::ostringstream detail;
          detail << "line " << lineNumber << " direction " << direction << "\n  expected levels:";
          for (const int value: group.levels) detail << ' ' << value;
          detail << "\n  actual levels:  ";
          for (const uint8_t value: result.levels) detail << ' ' << static_cast<int>(value);
          detail << "\n  expected order:";
          for (const size_t value: group.order) detail << ' ' << value;
          detail << "\n  actual order:  ";
          for (const size_t value: result.visualOrder) detail << ' ' << value;
          ADD_FAILURE() << detail.str();
          ++shown;
        }
      }
    }
  }
  EXPECT_EQ(cases, 490846u);
  EXPECT_EQ(directionCases, 770241u);
  EXPECT_EQ(failures, 0u) << failures << " direction-cases failed";
}
