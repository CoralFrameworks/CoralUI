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

#include "LineBreakTest.h"
#include "unicode/UnicodeVersion.generated.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifndef CORAL_UNICODE_TEST_DATA_DIR
#error "CORAL_UNICODE_TEST_DATA_DIR must point at libs/ui/tests/unicode"
#endif

namespace {

using coral::ui::unicode::testing::LineBreakTestCase;
using coral::ui::unicode::testing::LineBreakTestFile;
using coral::ui::unicode::testing::LineBreakTestReport;
using coral::ui::unicode::testing::parseLineBreakTestFile;
using coral::ui::unicode::testing::parseLineBreakTestStream;
using coral::ui::unicode::testing::runLineBreakTest;

std::string testFilePath() { return std::string(CORAL_UNICODE_TEST_DATA_DIR) + "/LineBreakTest.txt"; }

bool loadOfficialFile(LineBreakTestFile& file) {
  std::string error;
  if (parseLineBreakTestFile(testFilePath(), file, error)) return true;
  ADD_FAILURE() << error;
  return false;
}

} // namespace

TEST(UnicodeLineBreakTestHarness, ParsesEveryOfficialCase) {
  LineBreakTestFile file;
  ASSERT_TRUE(loadOfficialFile(file));
  EXPECT_EQ(file.version, std::string(coral::ui::unicode::kUnicodeVersion));
  EXPECT_EQ(file.cases.size(), 19346u);
  EXPECT_EQ(file.boundaryCount, 80131u);

  size_t minimumCodePoints = 0;
  size_t maximumCodePoints = 0;
  for (const LineBreakTestCase& testCase: file.cases) {
    ASSERT_FALSE(testCase.codePoints.empty());
    ASSERT_EQ(testCase.boundaries.size(), testCase.codePoints.size() + 1);
    EXPECT_EQ(testCase.boundaries.front(), 0u);
    EXPECT_EQ(testCase.boundaries.back(), 1u);
    EXPECT_GT(testCase.sourceLine, 0u);
    for (const uint32_t codePoint: testCase.codePoints) {
      EXPECT_LE(codePoint, 0x10FFFFu);
    }
    minimumCodePoints = minimumCodePoints == 0 ? testCase.codePoints.size()
                                               : std::min(minimumCodePoints, testCase.codePoints.size());
    maximumCodePoints = std::max(maximumCodePoints, testCase.codePoints.size());
  }
  EXPECT_EQ(minimumCodePoints, 2u);
  EXPECT_EQ(maximumCodePoints, 199u);
}

TEST(UnicodeLineBreakTestHarness, AConformingSegmenterPassesEveryBoundary) {
  LineBreakTestFile file;
  ASSERT_TRUE(loadOfficialFile(file));

  // The runner only hands code points to the segmenter, so the self-test looks
  // the expected answer up by sequence. Duplicate sequences in the official file
  // are consistent, which the test asserts while building the table.
  std::map<std::vector<uint32_t>, std::vector<uint8_t>> expected;
  for (const LineBreakTestCase& testCase: file.cases) {
    const auto inserted = expected.emplace(testCase.codePoints, testCase.boundaries);
    if (!inserted.second) {
      ASSERT_EQ(inserted.first->second, testCase.boundaries)
              << "duplicate code point sequence with conflicting expectations";
    }
  }

  const LineBreakTestReport report = runLineBreakTest(file, [&expected](const std::vector<uint32_t>& codePoints) {
    const auto found = expected.find(codePoints);
    return found == expected.end() ? std::vector<uint8_t>{} : found->second;
  });
  EXPECT_EQ(report.cases, 19346u);
  EXPECT_EQ(report.boundaries, 80131u);
  EXPECT_EQ(report.failures, 0u);
  EXPECT_TRUE(report.firstFailures.empty());
}

TEST(UnicodeLineBreakTestHarness, TheRunnerReportsMismatchesAndCapsDetails) {
  LineBreakTestFile file;
  ASSERT_TRUE(loadOfficialFile(file));

  std::map<std::vector<uint32_t>, std::vector<uint8_t>> expected;
  for (const LineBreakTestCase& testCase: file.cases) {
    expected.emplace(testCase.codePoints, testCase.boundaries);
  }
  const std::vector<uint32_t> mutated = file.cases.front().codePoints;
  const auto mutant = [&expected, &mutated](const std::vector<uint32_t>& codePoints) {
    const auto found = expected.find(codePoints);
    std::vector<uint8_t> actual = found == expected.end() ? std::vector<uint8_t>{} : found->second;
    if (codePoints == mutated && actual.size() > 1) {
      actual[1] = actual[1] == 0 ? 1 : 0;
    }
    return actual;
  };
  const LineBreakTestReport report = runLineBreakTest(file, mutant, 3);
  EXPECT_EQ(report.cases, 19346u);
  EXPECT_EQ(report.boundaries, 80131u);
  EXPECT_EQ(report.failures, 1u);
  ASSERT_EQ(report.firstFailures.size(), 1u);
  const auto& failure = report.firstFailures.front();
  EXPECT_EQ(failure.caseIndex, 0u);
  EXPECT_EQ(failure.boundaryIndex, 1u);
  EXPECT_EQ(failure.before, file.cases.front().codePoints[0]);
  EXPECT_EQ(failure.after, file.cases.front().codePoints[1]);
  EXPECT_FALSE(failure.sizeMismatch);
  EXPECT_NE(failure.expectedBreak, failure.actualBreak);

  const std::string text = coral::ui::unicode::testing::formatFailure(failure, file.cases.front());
  EXPECT_NE(text.find("expected"), std::string::npos);
  EXPECT_NE(text.find("got"), std::string::npos);
}

TEST(UnicodeLineBreakTestHarness, TheRunnerRejectsSizeMismatches) {
  LineBreakTestFile file;
  ASSERT_TRUE(loadOfficialFile(file));
  const LineBreakTestReport report =
          runLineBreakTest(file, [](const std::vector<uint32_t>&) { return std::vector<uint8_t>{ 0 }; });
  EXPECT_EQ(report.failures, file.cases.size());
  EXPECT_EQ(report.boundaries, 0u);
  ASSERT_FALSE(report.firstFailures.empty());
  EXPECT_TRUE(report.firstFailures.front().sizeMismatch);
  EXPECT_EQ(report.firstFailures.front().actualBoundaryCount, 1u);
}

TEST(UnicodeLineBreakTestHarness, FormattingUsesTheOfficialMarkers) {
  using namespace coral::ui::unicode::testing;
  EXPECT_EQ(formatBoundaries({ 0, 1, 0, 1 }), "\xC3\x97\xC3\xB7\xC3\x97\xC3\xB7");
  EXPECT_EQ(formatCodePoints({ 0x41, 0x1F600 }), "U+0041 U+1F600");
}

TEST(UnicodeLineBreakTestHarness, RejectsMalformedInputWithLineNumbers) {
  const std::string header = "# LineBreakTest-18.0.0.txt\n#\n";
  const std::string missingMarker = header + "\xC3\x97 0041 0042 \xC3\xB7\n";
  const std::string noEndBreak = header + "\xC3\x97 0041 \xC3\x97\n";
  const std::string badToken = header + "\xC3\x97 00ZZ \xC3\xB7\n";

  LineBreakTestFile file;
  std::string error;
  std::istringstream missingMarkerStream(missingMarker);
  EXPECT_FALSE(parseLineBreakTestStream(missingMarkerStream, "<missing-marker>", file, error));
  EXPECT_NE(error.find("<missing-marker>:3"), std::string::npos) << error;

  std::istringstream noEndBreakStream(noEndBreak);
  EXPECT_FALSE(parseLineBreakTestStream(noEndBreakStream, "<no-end-break>", file, error));
  EXPECT_NE(error.find("<no-end-break>:3"), std::string::npos) << error;

  std::istringstream badTokenStream(badToken);
  EXPECT_FALSE(parseLineBreakTestStream(badTokenStream, "<bad-token>", file, error));
  EXPECT_NE(error.find("<bad-token>:3"), std::string::npos) << error;
}

TEST(UnicodeLineBreakTestHarness, ParsesASmallWellFormedInput) {
  std::istringstream input("# LineBreakTest-18.0.0.txt\n"
                           "# a comment\n"
                           "\xC3\x97 0041 \xC3\xB7 0042 \xC3\xB7\n");
  LineBreakTestFile file;
  std::string error;
  ASSERT_TRUE(parseLineBreakTestStream(input, "<small>", file, error)) << error;
  ASSERT_EQ(file.cases.size(), 1u);
  EXPECT_EQ(file.version, "18.0.0");
  EXPECT_EQ(file.cases.front().codePoints, (std::vector<uint32_t>{ 0x41, 0x42 }));
  EXPECT_EQ(file.cases.front().boundaries, (std::vector<uint8_t>{ 0, 1, 1 }));
}
