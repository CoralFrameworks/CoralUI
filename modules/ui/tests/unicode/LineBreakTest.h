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

#ifndef CORAL_UI_TESTS_UNICODE_LINEBREAKTEST_H
#define CORAL_UI_TESTS_UNICODE_LINEBREAKTEST_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace coral::ui::unicode::testing {

/**
 * One case from Unicode's LineBreakTest.txt.
 *
 * `boundaries[i]` is the break decision before `codePoints[i]`; the extra final
 * entry is the eot decision. A value of 1 means "break allowed" (the file's
 * division sign), 0 means "no break" (its multiplication sign).
 */
struct LineBreakTestCase {
  std::vector<uint32_t> codePoints;
  std::vector<uint8_t> boundaries;
  size_t sourceLine = 0;

  [[nodiscard]] size_t boundaryCount() const noexcept { return boundaries.size(); }
};

/** The parsed official test file plus the Unicode version from its header. */
struct LineBreakTestFile {
  std::string version;
  std::vector<LineBreakTestCase> cases;
  size_t boundaryCount = 0;
};

/**
 * Reads every case in @p path.
 *
 * The parser is deliberately strict: the file must use the official
 * `× cp × cp ÷` shape with one marker before each code point and one after the
 * last. A malformed line is reported with its one-based line number instead of
 * being skipped, so a truncated download cannot silently reduce coverage.
 */
[[nodiscard]] bool parseLineBreakTestFile(const std::string& path, LineBreakTestFile& file, std::string& error);

/** Same parser over an arbitrary stream; @p sourceName is used in diagnostics. */
[[nodiscard]] bool parseLineBreakTestStream(std::istream& input, std::string_view sourceName, LineBreakTestFile& file,
        std::string& error);

/** Returns one break decision per boundary, sized `codePoints.size() + 1`. */
using LineBreakTestSegmenter = std::function<std::vector<uint8_t>(const std::vector<uint32_t>&)>;

struct LineBreakTestFailure {
  size_t caseIndex = 0;
  size_t sourceLine = 0;
  size_t boundaryIndex = 0;
  uint32_t before = 0;
  uint32_t after = 0;
  bool expectedBreak = false;
  bool actualBreak = false;
  /** UAX #14 rule index that produced the actual decision, or 0xFFFF. */
  uint16_t rule = 0xFFFF;
  bool sizeMismatch = false;
  size_t actualBoundaryCount = 0;
  std::vector<uint8_t> actualBoundaries;
};

struct LineBreakTestReport {
  size_t cases = 0;
  size_t boundaries = 0;
  size_t failures = 0;
  std::vector<LineBreakTestFailure> firstFailures;
};

/**
 * Runs @p segmenter over every case and compares the boundary decisions.
 *
 * The runner keeps counting after the first failure but stores at most
 * @p maxFailures details, which is what a CI failure needs: a small set of
 * actionable cases instead of 80k lines.
 */
[[nodiscard]] LineBreakTestReport runLineBreakTest(const LineBreakTestFile& file,
        const LineBreakTestSegmenter& segmenter, size_t maxFailures = 25);

/** @return e.g. "U+0061 U+0301". */
[[nodiscard]] std::string formatCodePoints(const std::vector<uint32_t>& codePoints);

/** @return e.g. "×÷÷". */
[[nodiscard]] std::string formatBoundaries(const std::vector<uint8_t>& boundaries);

/** @return a one-paragraph diagnostic for a conformance failure. */
[[nodiscard]] std::string formatFailure(const LineBreakTestFailure& failure, const LineBreakTestCase& testCase);

} // namespace coral::ui::unicode::testing

#endif // CORAL_UI_TESTS_UNICODE_LINEBREAKTEST_H
