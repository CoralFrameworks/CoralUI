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

#include "LineBreakTest.h"

#include "unicode/LineBreakIterator.h"
#include "unicode/LineBreakProperties.generated.h"

#include <charconv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

namespace coral::ui::unicode::testing {

namespace {

constexpr std::string_view kNoBreakMarker = "\xC3\x97"; // U+00D7 MULTIPLICATION SIGN
constexpr std::string_view kBreakMarker = "\xC3\xB7";   // U+00F7 DIVISION SIGN
constexpr std::string_view kVersionPrefix = "# LineBreakTest-";

std::string_view trim(const std::string_view value) {
  size_t first = 0;
  while (first < value.size() && (value[first] == ' ' || value[first] == '\t')) {
    ++first;
  }
  size_t last = value.size();
  while (last > first && (value[last - 1] == ' ' || value[last - 1] == '\t')) {
    --last;
  }
  return value.substr(first, last - first);
}

bool parseCodePoint(const std::string_view token, uint32_t& value) {
  if (token.empty() || token.size() > 6) return false;
  for (const char character: token) {
    const bool hex = (character >= '0' && character <= '9') || (character >= 'A' && character <= 'F') ||
                     (character >= 'a' && character <= 'f');
    if (!hex) return false;
  }
  uint32_t parsed = 0;
  const auto result = std::from_chars(token.data(), token.data() + token.size(), parsed, 16);
  if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) return false;
  if (parsed > 0x10FFFFu) return false;
  value = parsed;
  return true;
}

bool parseVersion(const std::string_view firstLine, std::string& version) {
  if (firstLine.size() <= kVersionPrefix.size() || firstLine.substr(0, kVersionPrefix.size()) != kVersionPrefix) {
    return false;
  }
  const std::string_view rest = firstLine.substr(kVersionPrefix.size());
  if (rest.size() <= 4 || rest.substr(rest.size() - 4) != ".txt") return false;
  const std::string_view value = rest.substr(0, rest.size() - 4);
  if (value.empty()) return false;
  version.assign(value);
  return true;
}

std::string codePointName(const uint32_t codePoint) {
  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "U+%04X", static_cast<unsigned>(codePoint));
  return buffer;
}

std::string classSequence(const std::vector<uint32_t>& codePoints) {
  std::string text;
  for (const uint32_t codePoint: codePoints) {
    if (!text.empty()) text += ' ';
    const auto raw = lineBreakClass(codePoint);
    text += lineBreakClassName(raw);
    const auto resolved = resolveLineBreakClass(codePoint);
    if (resolved != raw) {
      text += '(';
      text += lineBreakClassName(resolved);
      text += ')';
    }
  }
  return text;
}

} // namespace

bool parseLineBreakTestFile(const std::string& path, LineBreakTestFile& file, std::string& error) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    error = "cannot open " + path;
    return false;
  }
  return parseLineBreakTestStream(input, path, file, error);
}

bool parseLineBreakTestStream(std::istream& input, const std::string_view sourceName, LineBreakTestFile& file,
        std::string& error) {
  file = LineBreakTestFile{};
  const std::string source(sourceName);
  std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF &&
          static_cast<unsigned char>(content[1]) == 0xBB && static_cast<unsigned char>(content[2]) == 0xBF) {
    content.erase(0, 3);
  }

  bool sawHeader = false;
  size_t lineNumber = 0;
  size_t offset = 0;
  while (offset <= content.size()) {
    size_t end = content.find('\n', offset);
    if (end == std::string::npos) end = content.size();
    std::string_view line(content.data() + offset, end - offset);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    ++lineNumber;
    offset = end + 1;

    if (!sawHeader) {
      if (trim(line).empty()) continue;
      if (!parseVersion(line, file.version)) {
        error = source + ":" + std::to_string(lineNumber) + ": expected the '# LineBreakTest-<version>.txt' header";
        return false;
      }
      sawHeader = true;
      continue;
    }

    const size_t comment = line.find('#');
    if (comment != std::string_view::npos) line = line.substr(0, comment);
    line = trim(line);
    if (line.empty()) continue;

    std::vector<uint32_t> codePoints;
    std::vector<uint8_t> boundaries;
    std::istringstream tokens{ std::string(line) };
    std::string token;
    size_t tokenIndex = 0;
    while (tokens >> token) {
      const std::string_view view(token);
      if (tokenIndex % 2 == 0) {
        if (view == kNoBreakMarker) {
          boundaries.push_back(0);
        } else if (view == kBreakMarker) {
          boundaries.push_back(1);
        } else {
          error = source + ":" + std::to_string(lineNumber) + ": expected a break marker, got '" + token + "'";
          return false;
        }
      } else {
        uint32_t codePoint = 0;
        if (!parseCodePoint(view, codePoint)) {
          error = source + ":" + std::to_string(lineNumber) + ": invalid code point '" + token + "'";
          return false;
        }
        codePoints.push_back(codePoint);
      }
      ++tokenIndex;
    }

    if (tokenIndex == 0) continue;
    if (tokenIndex % 2 != 1 || boundaries.size() != codePoints.size() + 1) {
      error = source + ":" + std::to_string(lineNumber) +
              ": expected alternating markers and code points ending with a marker";
      return false;
    }
    if (codePoints.empty()) {
      error = source + ":" + std::to_string(lineNumber) + ": case has no code points";
      return false;
    }
    if (boundaries.front() != 0 || boundaries.back() != 1) {
      error = source + ":" + std::to_string(lineNumber) +
              ": official cases must start with 'x' and end with the eot division sign";
      return false;
    }

    LineBreakTestCase testCase;
    testCase.codePoints = std::move(codePoints);
    testCase.boundaries = std::move(boundaries);
    testCase.sourceLine = lineNumber;
    file.boundaryCount += testCase.boundaries.size();
    file.cases.push_back(std::move(testCase));
  }

  if (!sawHeader) {
    error = source + ": empty test file";
    return false;
  }
  if (file.cases.empty()) {
    error = source + ": no test cases";
    return false;
  }
  return true;
}

LineBreakTestReport runLineBreakTest(const LineBreakTestFile& file, const LineBreakTestSegmenter& segmenter,
        const size_t maxFailures) {
  LineBreakTestReport report;
  report.cases = file.cases.size();
  for (size_t caseIndex = 0; caseIndex < file.cases.size(); ++caseIndex) {
    const LineBreakTestCase& testCase = file.cases[caseIndex];
    const std::vector<uint8_t> actual = segmenter(testCase.codePoints);
    if (actual.size() != testCase.boundaries.size()) {
      ++report.failures;
      if (report.firstFailures.size() < maxFailures) {
        LineBreakTestFailure failure;
        failure.caseIndex = caseIndex;
        failure.sourceLine = testCase.sourceLine;
        failure.boundaryIndex = testCase.boundaries.size();
        failure.sizeMismatch = true;
        failure.actualBoundaryCount = actual.size();
        failure.actualBoundaries = actual;
        report.firstFailures.push_back(failure);
      }
      continue;
    }
    std::vector<uint16_t> rules;
    for (size_t boundaryIndex = 0; boundaryIndex < actual.size(); ++boundaryIndex) {
      ++report.boundaries;
      if ((actual[boundaryIndex] != 0) == (testCase.boundaries[boundaryIndex] != 0)) continue;
      ++report.failures;
      if (report.firstFailures.size() >= maxFailures) continue;
      LineBreakTestFailure failure;
      failure.caseIndex = caseIndex;
      failure.sourceLine = testCase.sourceLine;
      failure.boundaryIndex = boundaryIndex;
      failure.before = boundaryIndex == 0 ? 0 : testCase.codePoints[boundaryIndex - 1];
      failure.after = boundaryIndex == testCase.codePoints.size() ? 0 : testCase.codePoints[boundaryIndex];
      failure.expectedBreak = testCase.boundaries[boundaryIndex] != 0;
      failure.actualBreak = actual[boundaryIndex] != 0;
      if (rules.empty()) rules = computeLineBreakRules(testCase.codePoints);
      if (boundaryIndex < rules.size()) failure.rule = rules[boundaryIndex];
      failure.actualBoundaries = actual;
      report.firstFailures.push_back(failure);
    }
  }
  return report;
}

std::string formatCodePoints(const std::vector<uint32_t>& codePoints) {
  std::string text;
  for (const uint32_t codePoint: codePoints) {
    if (!text.empty()) text += ' ';
    text += codePointName(codePoint);
  }
  return text;
}

std::string formatBoundaries(const std::vector<uint8_t>& boundaries) {
  std::string text;
  for (const uint8_t boundary: boundaries) {
    text += boundary != 0 ? "\xC3\xB7" : "\xC3\x97";
  }
  return text;
}

std::string formatFailure(const LineBreakTestFailure& failure, const LineBreakTestCase& testCase) {
  std::ostringstream output;
  output << "case " << (failure.caseIndex + 1) << " (line " << failure.sourceLine << ")";
  if (failure.sizeMismatch) {
    output << ": segmenter returned " << failure.actualBoundaryCount << " boundaries, expected "
           << testCase.boundaries.size();
  } else if (failure.boundaryIndex == 0) {
    output << ": sot boundary expected " << (failure.expectedBreak ? "break" : "no break") << ", got "
           << (failure.actualBreak ? "break" : "no break");
  } else if (failure.boundaryIndex == testCase.codePoints.size()) {
    output << ": eot boundary expected " << (failure.expectedBreak ? "break" : "no break") << ", got "
           << (failure.actualBreak ? "break" : "no break");
  } else {
    output << ": boundary " << failure.boundaryIndex << " after " << codePointName(failure.before) << " before "
           << codePointName(failure.after) << " expected " << (failure.expectedBreak ? "break" : "no break") << ", got "
           << (failure.actualBreak ? "break" : "no break");
  }
  output << "\n  text:     " << formatCodePoints(testCase.codePoints);
  output << "\n  classes:  " << classSequence(testCase.codePoints);
  if (failure.rule != 0xFFFFu) {
    output << "\n  rule:     " << lineBreakRuleName(failure.rule) << " (" << failure.rule << ")";
  }
  output << "\n  expected: " << formatBoundaries(testCase.boundaries);
  output << "\n  actual:   " << formatBoundaries(failure.actualBoundaries);
  return output.str();
}

} // namespace coral::ui::unicode::testing
