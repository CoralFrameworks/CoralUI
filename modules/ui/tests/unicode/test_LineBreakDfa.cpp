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
#include "unicode/LineBreakIterator.h"

#include <ui/RichText.h>
#include <ui/TextBidi.h>
#include <ui/TextLayout.h>

#include <graphics/Font.h>
#include <graphics/Typeface.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <numeric>
#include <memory>
#include <string>
#include <vector>

#ifndef CORAL_UNICODE_TEST_DATA_DIR
#error "CORAL_UNICODE_TEST_DATA_DIR must point at libs/ui/tests/unicode"
#endif

namespace {

using coral::ui::RichTextLayout;
using coral::ui::RichTextRun;
using coral::ui::TextLayout;
using coral::ui::unicode::computeLineBreakActions;
using coral::ui::unicode::testing::formatFailure;
using coral::ui::unicode::testing::LineBreakTestFile;
using coral::ui::unicode::testing::LineBreakTestReport;
using coral::ui::unicode::testing::parseLineBreakTestFile;
using coral::ui::unicode::testing::runLineBreakTest;

LineBreakTestFile loadOfficialFile() {
  LineBreakTestFile file;
  std::string error;
  EXPECT_TRUE(parseLineBreakTestFile(std::string(CORAL_UNICODE_TEST_DATA_DIR) + "/LineBreakTest.txt", file, error))
          << error;
  return file;
}

std::shared_ptr<const graphics::Font> testFont(const float size) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  return typeface != nullptr ? graphics::Font::create(typeface, size) : nullptr;
}


std::string makeAsciiText(const size_t target) {
  std::string text;
  text.reserve(target + 64);
  while (text.size() < target) text += "The quick brown fox jumps over the lazy dog. ";
  return text;
}

std::string makeMixedText(const size_t target) {
  static const char* const chunks[] = {
    "The quick brown fox jumps over the lazy dog. ",
    "\xE4\xB8\xAD\xE6\x96\x87\xE6\x96\xAD\xE8\xA1\x8C\xE6\xB5\x8B\xE8\xAF\x95\xEF\xBC\x8C"
    "\xE6\xA0\x87\xE7\x82\xB9\xE4\xB8\x8E\xE6\x95\xB0\xE5\xAD\x97 12345\xE3\x80\x82",
    "\xE3\x83\x86\xE3\x82\xAD\xE3\x82\xB9\xE3\x83\x88\xE3\x81\xAE\xE6\x8A\x98\xE3\x82\x8A"
    "\xE8\xBF\x94\xE3\x81\x97\xE3\x82\x92\xE7\xA2\xBA\xE8\xAA\x8D\xE3\x81\x97\xE3\x81\xBE"
    "\xE3\x81\x99\xE3\x80\x82",
    "\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D \xD7\xA2\xD7\x95\xD7\x9C\xD7\x9D \xD7\x96\xD7\x94"
    "\xD7\x95 \xD7\x9E\xD7\x91\xD7\x97\xD7\x9F ",
    "\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7 \xD8\xA8\xD8\xA7\xD9\x84\xD8\xB9\xD8\xA7"
    "\xD9\x84\xD9\x85 \xD9\x87\xD8\xB0\xD8\xA7 \xD8\xA7\xD8\xAE\xD8\xAA\xD8\xA8\xD8\xA7"
    "\xD8\xB1 ",
    "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7\xE2\x80\x8D"
    "\xF0\x9F\x91\xA6 \xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD emoji ",
  };
  std::string text;
  text.reserve(target + 256);
  size_t chunk = 0;
  while (text.size() < target) {
    text += chunks[chunk % (sizeof(chunks) / sizeof(chunks[0]))];
    ++chunk;
  }
  while (text.size() > target && (static_cast<unsigned char>(text.back()) & 0xC0u) == 0x80u) {
    text.pop_back();
  }
  return text;
}

double medianMilliseconds(const std::function<void()>& operation, const int iterations) {
  operation();
  std::vector<double> samples;
  samples.reserve(static_cast<size_t>(iterations));
  for (int index = 0; index < iterations; ++index) {
    const auto start = std::chrono::steady_clock::now();
    operation();
    samples.push_back(std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - start).count());
  }
  std::sort(samples.begin(), samples.end());
  return samples[samples.size() / 2];
}

void reportBenchmark(const char* phase, const size_t bytes, const double milliseconds,
    const size_t detail) {
  const double megabytes = static_cast<double>(bytes) / (1024.0 * 1024.0);
  const double throughput = milliseconds > 0.0 ? megabytes / (milliseconds / 1000.0) : 0.0;
  std::cout << "[bench] " << phase << " bytes=" << bytes << " detail=" << detail
            << " ms=" << milliseconds << " MiB/s=" << throughput << '\n';
}

} // namespace

TEST(UnicodeLineBreakConformance, OfficialLineBreakTestPassesEveryBoundary) {
  const LineBreakTestFile file = loadOfficialFile();
  ASSERT_EQ(file.cases.size(), 19346u);

  const LineBreakTestReport report = runLineBreakTest(file,
          [](const std::vector<uint32_t>& codePoints) { return computeLineBreakActions(codePoints); });

  EXPECT_EQ(report.cases, 19346u);
  EXPECT_EQ(report.boundaries, 80131u);
  EXPECT_EQ(report.failures, 0u);
  for (const auto& failure: report.firstFailures) {
    ADD_FAILURE() << formatFailure(failure, file.cases[failure.caseIndex]);
  }
}

TEST(UnicodeLineBreakConformance, Utf8BoundariesUseByteOffsets) {
  // "中文。" should break between the ideographs and keep the closing punctuation
  // with the preceding character.
  const std::string text = "\xE4\xB8\xAD\xE6\x96\x87\xE3\x80\x82";
  const auto boundaries = coral::ui::unicode::computeLineBreaks(text);
  ASSERT_EQ(boundaries.size(), 4u); // sot + three code points (the eot boundary is the last)
  EXPECT_EQ(boundaries.front().byteOffset, 0u);
  EXPECT_EQ(boundaries.back().byteOffset, text.size());
  EXPECT_EQ(boundaries.back().action, coral::ui::unicode::BreakAction::MandatoryBreak);
  bool foundBreak = false;
  for (const auto& boundary: boundaries) {
    if (boundary.action == coral::ui::unicode::BreakAction::AllowBreak && boundary.byteOffset == 3u) {
      foundBreak = true;
    }
  }
  EXPECT_TRUE(foundBreak);
}
TEST(UnicodeLineBreakConformance, CompactBoundaryTableMatchesTheStructResult) {
  const std::string text = "alpha beta\n\xE4\xB8\xAD\xE6\x96\x87\xE3\x80\x82 gamm\x61";
  const auto table = coral::ui::unicode::computeLineBreakTable(text);
  const auto boundaries = coral::ui::unicode::computeLineBreaks(text);
  ASSERT_EQ(table.size(), boundaries.size());
  for (size_t index = 0; index < table.size(); ++index) {
    EXPECT_EQ(table.offsets[index], boundaries[index].byteOffset);
    EXPECT_EQ(table.actions[index], static_cast<uint8_t>(boundaries[index].action));
  }
}

TEST(UnicodeLineBreakLayout, EmergencyBreakKeepsGraphemeClustersTogether) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const TextLayout combining = coral::ui::layoutText("e\xCC\x81", *font, 1.0f);
  ASSERT_EQ(combining.lines.size(), 1u);
  EXPECT_EQ(combining.lines.front().text, "e\xCC\x81");

  const std::string family = "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D"
                             "\xF0\x9F\x91\xA7\xE2\x80\x8D\xF0\x9F\x91\xA6";
  const TextLayout emoji = coral::ui::layoutText(family, *font, 1.0f);
  EXPECT_EQ(emoji.lines.size(), 1u);
}

TEST(UnicodeLineBreakLayout, TextLayoutDropsCrLfSeparators) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const TextLayout layout = coral::ui::layoutText("a\r\nb", *font, 0.0f);
  ASSERT_EQ(layout.lines.size(), 2u);
  EXPECT_EQ(layout.lines[0].text, "a");
  EXPECT_EQ(layout.lines[1].text, "b");
}

TEST(UnicodeLineBreakLayout, RichTextUsesUax14ForCjk) {
  const auto font = testFont(14.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RichTextRun run;
  run.text = "\xE4\xB8\xAD\xE6\x96\x87\xE3\x80\x82\xE6\xB5\x8B\xE8\xAF\x95";
  run.font = font;
  const float width = font->measureWidth("\xE4\xB8\xAD\xE6\x96\x87") + 1.0f;
  const RichTextLayout layout = coral::ui::layoutRichText({ run }, width);
  EXPECT_GE(layout.lines.size(), 2u);
  for (const auto& line: layout.lines) {
    ASSERT_FALSE(line.segments.empty());
    EXPECT_NE(line.segments.front().text.substr(0, 3), "\xE3\x80\x82")
            << "closing punctuation must not start a wrapped rich-text line";
  }
}

TEST(UnicodeLineBreakPerformanceGate, RelativeBudgetsStayWithinBaselineRatio) {
#ifndef NDEBUG
  GTEST_SKIP() << "performance gate is Release-only";
#else
  const std::string text = makeAsciiText(1024 * 1024);
  std::vector<uint32_t> codePoints(1024 * 1024, static_cast<uint32_t>('a'));
  std::vector<uint32_t> mixedCodePoints;
  mixedCodePoints.reserve(text.size());
  for (const char character: text) mixedCodePoints.push_back(static_cast<unsigned char>(character));
  const double actionsMs = medianMilliseconds(
    [&codePoints] { (void)coral::ui::unicode::computeLineBreakActions(codePoints); }, 5);
  const double mixedActionsMs = medianMilliseconds(
    [&mixedCodePoints] { (void)coral::ui::unicode::computeLineBreakActions(mixedCodePoints); }, 5);
  coral::ui::unicode::LineBreakActionBuffer buffer;
  const auto expectedActions = coral::ui::unicode::computeLineBreakActions(mixedCodePoints);
  const auto bufferedActions = buffer.compute(mixedCodePoints);
  ASSERT_EQ(bufferedActions.size(), expectedActions.size());
  EXPECT_TRUE(std::equal(bufferedActions.begin(), bufferedActions.end(), expectedActions.begin()));
  std::vector<uint8_t> collectedActions;
  collectedActions.reserve(expectedActions.size());
  coral::ui::unicode::forEachLineBreakAction(mixedCodePoints,
    [&collectedActions](const size_t, const coral::ui::unicode::BreakAction action) {
      collectedActions.push_back(static_cast<uint8_t>(action));
    });
  EXPECT_EQ(collectedActions, expectedActions);
  size_t streamedActions = 0;
  const double streamActionsMs = medianMilliseconds([&mixedCodePoints, &streamedActions] {
    streamedActions = 0;
    coral::ui::unicode::forEachLineBreakAction(mixedCodePoints,
      [&streamedActions](const size_t, const coral::ui::unicode::BreakAction) {
        ++streamedActions;
      });
  }, 5);
  EXPECT_EQ(streamedActions, expectedActions.size());
  const std::vector<uint32_t> nonProseCodePoints{ 'a', '#', 'b', '\n', 0x4E2D, 'c' };
  const auto nonProseExpected = coral::ui::unicode::computeLineBreakActions(nonProseCodePoints);
  std::vector<uint8_t> nonProseStreamed;
  coral::ui::unicode::forEachLineBreakAction(nonProseCodePoints,
    [&nonProseStreamed](const size_t, const coral::ui::unicode::BreakAction action) {
      nonProseStreamed.push_back(static_cast<uint8_t>(action));
    });
  EXPECT_EQ(nonProseStreamed, nonProseExpected);
  std::vector<uint8_t> blockCollected;
  blockCollected.reserve(expectedActions.size());
  struct BlockCollectState {
    std::vector<uint8_t>* out = nullptr;
    size_t calls = 0;
  } blockState{ &blockCollected, 0 };
  coral::ui::unicode::forEachLineBreakActionBlock(mixedCodePoints,
    [](void* context, const size_t, const uint8_t* actions, const size_t count) {
      auto* state = static_cast<BlockCollectState*>(context);
      state->out->insert(state->out->end(), actions, actions + count);
      ++state->calls;
    }, &blockState);
  EXPECT_EQ(blockCollected, expectedActions);
  EXPECT_GT(blockState.calls, 0u);
  size_t blockCalls = 0;
  const double blockMs = medianMilliseconds([&mixedCodePoints, &blockCalls] {
    blockCalls = 0;
    coral::ui::unicode::forEachLineBreakActionBlock(mixedCodePoints,
      [](void* context, const size_t, const uint8_t*, const size_t) {
        ++*static_cast<size_t*>(context);
      }, &blockCalls);
  }, 5);
  EXPECT_GT(blockCalls, 0u);
  const double bufferMs = medianMilliseconds(
    [&buffer, &mixedCodePoints] { (void)buffer.compute(mixedCodePoints); }, 5);
  std::cout << "[ perf-actions ] alphabetic=" << actionsMs
            << " ms mixed=" << mixedActionsMs << " ms buffer=" << bufferMs
            << " ms stream=" << streamActionsMs << " ms block=" << blockMs << " ms\n";
  size_t streamed = 0;
  const double streamMs = medianMilliseconds([&text, &streamed] {
    streamed = coral::ui::unicode::forEachLineBreakOpportunity(text,
      [&streamed](size_t, coral::ui::unicode::BreakAction) { ++streamed; });
  }, 5);
  const double tableMs = medianMilliseconds(
    [&text] { (void)coral::ui::unicode::computeLineBreakTable(text); }, 5);
  ASSERT_GT(actionsMs, 0.0);
  ASSERT_GT(streamed, 0u);
  // Ratio-based budgets: absolute milliseconds vary with the machine, but the
  // streaming and materialized paths must stay within a stable multiple of the
  // core DFA. A regression in any single layer trips this gate.
  // The UTF-8 streaming path also decodes each code point, so its multiple over the vector DFA is
  // higher on a paravirtual/slow CI CPU than on a workstation. Keep a bounded ratio (10x) rather
  // than the old 5x, which made a shared runner's scheduler/decoder variance a false regression.
  EXPECT_LT(streamMs, mixedActionsMs * 10.0 + 5.0)
    << "mixed actions=" << mixedActionsMs << "ms stream=" << streamMs << "ms";
  EXPECT_LT(actionsMs, 2.0)
    << "the 2.0 ms/1 MiB absolute target for core actions";
  EXPECT_LT(tableMs, mixedActionsMs * 12.0 + 4.0)
    << "mixed actions=" << mixedActionsMs << "ms table=" << tableMs << "ms";
#endif
}

TEST(UnicodeLineBreakPerformance, DISABLED_OneMegabyteBoundaryScan) {
  std::string text;
  text.reserve(1024 * 1024);
  while (text.size() < 1024 * 1024) text += "The quick brown fox jumps over the lazy dog. ";
  const auto start = std::chrono::steady_clock::now();
  const auto boundaries = coral::ui::unicode::computeLineBreaks(text);
  const auto cost = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  std::cout << "[ perf ] " << text.size() << " bytes, " << boundaries.size() << " boundaries, " << cost << " ms\n";
  EXPECT_EQ(boundaries.back().byteOffset, text.size());
}
TEST(UnicodeLineBreakPerformance, DISABLED_OneMegabyteActionsOnly) {
  std::vector<uint32_t> codePoints(1024 * 1024, static_cast<uint32_t>('a'));
  const auto start = std::chrono::steady_clock::now();
  const auto actions = coral::ui::unicode::computeLineBreakActions(codePoints);
  const auto cost = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  std::cout << "[ perf-actions ] " << codePoints.size() << " code points, " << actions.size() << " boundaries, " << cost
            << " ms\n";
  EXPECT_EQ(actions.size(), codePoints.size() + 1);
}
TEST(UnicodeLineBreakPerformance, DISABLED_OneMegabyteOpportunities) {
  std::string text;
  text.reserve(1024 * 1024);
  while (text.size() < 1024 * 1024) text += "The quick brown fox jumps over the lazy dog. ";
  const auto start = std::chrono::steady_clock::now();
  const auto opportunities = coral::ui::unicode::computeLineBreakOpportunities(text);
  const auto cost = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  std::cout << "[ perf-opps ] " << text.size() << " bytes, " << opportunities.size() << " opportunities, " << cost
            << " ms\n";
  EXPECT_FALSE(opportunities.empty());
}
TEST(UnicodeLineBreakPerformance, DISABLED_MixedOneMegabyteHotspots) {
  constexpr size_t target = 1024 * 1024;
  const std::string text = makeMixedText(target);
  ASSERT_GE(text.size(), target);

  const auto boundaries = coral::ui::unicode::computeLineBreaks(text);
  const double boundaryMs = medianMilliseconds(
    [&text] { (void)coral::ui::unicode::computeLineBreaks(text); }, 5);
  reportBenchmark("boundaries-mixed", text.size(), boundaryMs, boundaries.size());

  const auto opportunities = coral::ui::unicode::computeLineBreakOpportunities(text);
  const double opportunityMs = medianMilliseconds(
    [&text] { (void)coral::ui::unicode::computeLineBreakOpportunities(text); }, 5);
  reportBenchmark("opportunities-mixed", text.size(), opportunityMs, opportunities.size());
  size_t streamedMixed = 0;
  const double streamMixedMs = medianMilliseconds([&text, &streamedMixed] {
    streamedMixed = coral::ui::unicode::forEachLineBreakOpportunity(text,
      [&streamedMixed](size_t, coral::ui::unicode::BreakAction) { ++streamedMixed; });
  }, 5);
  reportBenchmark("opportunities-stream-mixed", text.size(), streamMixedMs, streamedMixed);

  const auto coldStart = std::chrono::steady_clock::now();
  const coral::ui::BidiParagraph cold = coral::ui::resolveBidiParagraph(text);
  const double coldMs = std::chrono::duration<double, std::milli>(
    std::chrono::steady_clock::now() - coldStart).count();
  reportBenchmark("bidi-cold-mixed", text.size(), coldMs, cold.levels.size());
  const double warmMs = medianMilliseconds(
    [&text] { (void)coral::ui::resolveBidiParagraph(text); }, 10);
  reportBenchmark("bidi-warm-mixed", text.size(), warmMs, cold.levels.size());

  const auto font = testFont(16.0f);
  if (font != nullptr) {
    const auto coldStart = std::chrono::steady_clock::now();
    const auto layout = coral::ui::layoutText(text, *font, coral::ui::TextLayoutOptions{ 320.0f, 0, false });
    const double coldLayoutMs = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - coldStart).count();
    reportBenchmark("layout-mixed-cold", text.size(), coldLayoutMs, layout.lines.size());
    const double layoutMs = medianMilliseconds([&text, &font] {
      (void)coral::ui::layoutText(text, *font, coral::ui::TextLayoutOptions{ 320.0f, 0, false });
    }, 3);
    reportBenchmark("layout-mixed-warm", text.size(), layoutMs, layout.lines.size());

    coral::ui::RichTextRun run;
    run.text = text;
    run.font = font;
    const auto richStart = std::chrono::steady_clock::now();
    const auto rich = coral::ui::layoutRichText({ run }, 320.0f);
    const double richColdMs = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - richStart).count();
    reportBenchmark("richtext-mixed-cold", text.size(), richColdMs, rich.lines.size());
    const double richWarmMs = medianMilliseconds(
      [&run] { (void)coral::ui::layoutRichText({ run }, 320.0f); }, 3);
    reportBenchmark("richtext-mixed-warm", text.size(), richWarmMs, rich.lines.size());
  }
}

TEST(UnicodeLineBreakPerformance, DISABLED_AsciiOneMegabyteHotspots) {
  constexpr size_t target = 1024 * 1024;
  const std::string text = makeAsciiText(target);
  const double boundaryMs = medianMilliseconds(
    [&text] { (void)coral::ui::unicode::computeLineBreaks(text); }, 5);
  reportBenchmark("boundaries-ascii", text.size(), boundaryMs, text.size());
  const auto compact = coral::ui::unicode::computeLineBreakTable(text);
  const double compactMs = medianMilliseconds(
    [&text] { (void)coral::ui::unicode::computeLineBreakTable(text); }, 5);
  reportBenchmark("boundaries-compact-ascii", text.size(), compactMs, compact.size());

  const auto asciiOpportunities = coral::ui::unicode::computeLineBreakOpportunities(text);
  const double opportunityMs = medianMilliseconds(
    [&text] { (void)coral::ui::unicode::computeLineBreakOpportunities(text); }, 5);
  reportBenchmark("opportunities-ascii", text.size(), opportunityMs, asciiOpportunities.size());
  size_t streamedAscii = 0;
  const double streamAsciiMs = medianMilliseconds([&text, &streamedAscii] {
    streamedAscii = coral::ui::unicode::forEachLineBreakOpportunity(text,
      [&streamedAscii](size_t, coral::ui::unicode::BreakAction) { ++streamedAscii; });
  }, 5);
  reportBenchmark("opportunities-stream-ascii", text.size(), streamAsciiMs, streamedAscii);

  const auto ltrStart = std::chrono::steady_clock::now();
  const coral::ui::BidiParagraph ltr = coral::ui::resolveBidiParagraph(text);
  const double ltrMs = std::chrono::duration<double, std::milli>(
    std::chrono::steady_clock::now() - ltrStart).count();
  reportBenchmark("bidi-ltr-fast", text.size(), ltrMs, ltr.levels.size());

  const auto font = testFont(16.0f);
  if (font != nullptr) {
    const auto coldStart = std::chrono::steady_clock::now();
    const auto layout = coral::ui::layoutText(text, *font, coral::ui::TextLayoutOptions{ 320.0f, 0, false });
    const double coldLayoutMs = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - coldStart).count();
    reportBenchmark("layout-ascii-cold", text.size(), coldLayoutMs, layout.lines.size());
    const double layoutMs = medianMilliseconds([&text, &font] {
      (void)coral::ui::layoutText(text, *font, coral::ui::TextLayoutOptions{ 320.0f, 0, false });
    }, 3);
    reportBenchmark("layout-ascii-warm", text.size(), layoutMs, layout.lines.size());
    const std::string changed = text + " tail";
    const double incrementalMs = medianMilliseconds([&layout, &changed, &font] {
      (void)coral::ui::updateTextLayout(layout, changed, *font,
        coral::ui::TextLayoutOptions{ 320.0f, 0, false });
    }, 3);
    reportBenchmark("layout-ascii-incremental-tail", changed.size(), incrementalMs, layout.lines.size());
    const coral::ui::TextEditRange tailEdit{ text.size(), text.size(), changed.size() };
    const double hintMs = medianMilliseconds([&layout, &changed, &tailEdit, &font] {
      (void)coral::ui::updateTextLayout(layout, changed, tailEdit, *font,
        coral::ui::TextLayoutOptions{ 320.0f, 0, false });
    }, 3);
    reportBenchmark("layout-ascii-incremental-hint", changed.size(), hintMs, layout.lines.size());
    std::string middle = text;
    middle.insert(middle.size() / 2, "x");
    const double middleMs = medianMilliseconds([&layout, &middle, &font] {
      (void)coral::ui::updateTextLayout(layout, middle, *font,
        coral::ui::TextLayoutOptions{ 320.0f, 0, false });
    }, 3);
    reportBenchmark("layout-ascii-incremental-middle", middle.size(), middleMs, layout.lines.size());
    double moveTailMs = 1e9;
    for (int iteration = 0; iteration < 3; ++iteration) {
      TextLayout moved = layout;
      const auto start = std::chrono::steady_clock::now();
      (void)coral::ui::updateTextLayout(std::move(moved), changed, *font,
        coral::ui::TextLayoutOptions{ 320.0f, 0, false });
      moveTailMs = std::min(moveTailMs, std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count());
    }
    reportBenchmark("layout-ascii-incremental-move-tail", changed.size(), moveTailMs, layout.lines.size());

    coral::ui::RichTextRun run;
    run.text = text;
    run.font = font;
    const auto richStart = std::chrono::steady_clock::now();
    const auto rich = coral::ui::layoutRichText({ run }, 320.0f);
    const double richColdMs = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - richStart).count();
    reportBenchmark("richtext-ascii-cold", text.size(), richColdMs, rich.lines.size());
    const double richWarmMs = medianMilliseconds(
      [&run] { (void)coral::ui::layoutRichText({ run }, 320.0f); }, 3);
    reportBenchmark("richtext-ascii-warm", text.size(), richWarmMs, rich.lines.size());
    coral::ui::RichTextRun changedRun = run;
    changedRun.text = text + " tail";
    const std::vector<coral::ui::RichTextRun> changedRuns{ changedRun };
    const double richIncrementalMs = medianMilliseconds(
      [&rich, &changedRuns] {
        (void)coral::ui::updateRichTextLayout(rich, changedRuns,
          coral::ui::RichTextLayoutOptions{ 320.0f });
      }, 3);
    reportBenchmark("richtext-ascii-incremental-tail", changedRun.text.size(), richIncrementalMs,
      rich.lines.size());

    std::string middleText = text;
    middleText.insert(middleText.size() / 2, " inserted ");
    coral::ui::RichTextRun middleRun = run;
    middleRun.text = middleText;
    const std::vector<coral::ui::RichTextRun> middleRuns{ middleRun };
    const double richMiddleMs = medianMilliseconds(
      [&rich, &middleRuns] {
        (void)coral::ui::updateRichTextLayout(rich, middleRuns,
          coral::ui::RichTextLayoutOptions{ 320.0f });
      }, 3);
    reportBenchmark("richtext-ascii-incremental-middle", middleRun.text.size(), richMiddleMs,
      rich.lines.size());
  }
}

TEST(UnicodeLineBreakPerformance, DISABLED_ParagraphCacheColdWarm) {
  std::string text = makeMixedText(64 * 1024);
  text += "\n#";
  text += std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  const auto start = std::chrono::steady_clock::now();
  const coral::ui::BidiParagraph cold = coral::ui::resolveBidiParagraph(text);
  const double coldMs = std::chrono::duration<double, std::milli>(
    std::chrono::steady_clock::now() - start).count();
  const double warmMs = medianMilliseconds(
    [&text] { (void)coral::ui::resolveBidiParagraph(text); }, 10);
  reportBenchmark("bidi-cold-64k", text.size(), coldMs, cold.levels.size());
  reportBenchmark("bidi-warm-64k", text.size(), warmMs, cold.levels.size());
  EXPECT_LT(warmMs, coldMs);
}

TEST(UnicodeLineBreakConformance, AsciiPrefixPostfixRunsKeepTheirBreak) {
  for (const std::string text: { std::string("%%"), std::string("\x24\x24") }) {
    bool found = false;
    coral::ui::unicode::forEachLineBreakOpportunity(text,
      [&found](const size_t offset, const coral::ui::unicode::BreakAction action) {
        if (offset == 1 && action == coral::ui::unicode::BreakAction::AllowBreak) found = true;
      });
    EXPECT_TRUE(found) << text << " must break between adjacent prefix/postfix characters";
  }
}

TEST(UnicodeLineBreakConformance, StreamingOpportunitiesMatchVector) {
  const std::string text = "one two \xE4\xB8\xAD\xE6\x96\x87 three\nfour";
  const auto expected = coral::ui::unicode::computeLineBreakOpportunities(text);
  std::vector<coral::ui::unicode::LineBreakOpportunity> streamed;
  const size_t count = coral::ui::unicode::forEachLineBreakOpportunity(text,
    [&streamed](const size_t offset, const coral::ui::unicode::BreakAction action) {
      streamed.push_back({ offset, action });
    });
  EXPECT_EQ(count, expected.size());
  ASSERT_EQ(streamed.size(), expected.size());
  for (size_t index = 0; index < expected.size(); ++index) {
    EXPECT_EQ(streamed[index].byteOffset, expected[index].byteOffset);
    EXPECT_EQ(streamed[index].action, expected[index].action);
  }
}

TEST(UnicodeLineBreakConformance, OpportunitiesExposeOnlyRealBreaks) {
  const auto opportunities = coral::ui::unicode::computeLineBreakOpportunities("one two three");
  ASSERT_FALSE(opportunities.empty());
  size_t previous = 0;
  for (const auto& opportunity: opportunities) {
    EXPECT_NE(opportunity.action, coral::ui::unicode::BreakAction::NoBreak);
    EXPECT_GE(opportunity.byteOffset, previous);
    EXPECT_LT(opportunity.byteOffset, 13u);
    previous = opportunity.byteOffset;
  }
  EXPECT_NE(std::find_if(opportunities.begin(), opportunities.end(),
                    [](const auto& value) { return value.byteOffset == 4u; }),
          opportunities.end());
}
TEST(UnicodeLineBreakLayout, RichTextEmergencyKeepsGraphemeClusterTogether) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RichTextRun run;
  run.text = "e\xCC\x81";
  run.font = font;
  const RichTextLayout layout = coral::ui::layoutRichText({ run }, 1.0f);
  ASSERT_EQ(layout.lines.size(), 1u);
  ASSERT_FALSE(layout.lines.front().segments.empty());
  EXPECT_EQ(layout.lines.front().segments.front().text, "e\xCC\x81");
}
TEST(UnicodeLineBreakLayout, EmergencyBreakUsesFullGraphemeRules) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";

  // Devanagari KA + VIRAMA + SSA: GB9c keeps the conjunct together.
  const std::string conjunct = "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7";
  const TextLayout conjunctLayout = coral::ui::layoutText(conjunct, *font, 1.0f);
  ASSERT_EQ(conjunctLayout.lines.size(), 1u);
  EXPECT_EQ(conjunctLayout.lines.front().text, conjunct);

  // A regional-indicator pair is one grapheme cluster (GB12/GB13).
  const std::string flag = "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8";
  const TextLayout flagLayout = coral::ui::layoutText(flag, *font, 1.0f);
  ASSERT_EQ(flagLayout.lines.size(), 1u);
  EXPECT_EQ(flagLayout.lines.front().text, flag);
}

TEST(UnicodeLineBreakLayout, OverflowWrapPolicyControlsEmergencyBreaks) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";

  coral::ui::TextLayoutOptions defaultOptions;
  defaultOptions.maxWidth = 1.0f;
  const TextLayout defaultLayout = coral::ui::layoutText("abcdef", *font, defaultOptions);
  ASSERT_EQ(defaultLayout.lines.size(), 1u)
          << "the library default is CSS overflow-wrap: normal";
  EXPECT_EQ(defaultLayout.lines.front().text, "abcdef");

  coral::ui::TextLayoutOptions normal;
  normal.maxWidth = 1.0f;
  normal.policy.overflowWrap = coral::ui::OverflowWrap::Normal;
  const TextLayout overflow = coral::ui::layoutText("abcdef", *font, normal);
  ASSERT_EQ(overflow.lines.size(), 1u);
  EXPECT_EQ(overflow.lines.front().text, "abcdef");

  coral::ui::TextLayoutOptions anywhere;
  anywhere.maxWidth = 1.0f;
  anywhere.policy.overflowWrap = coral::ui::OverflowWrap::Anywhere;
  const TextLayout broken = coral::ui::layoutText("abcdef", *font, anywhere);
  EXPECT_GT(broken.lines.size(), 1u);
}

TEST(UnicodeLineBreakLayout, IncrementalReflowMatchesFullLayout) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string base = "alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu "
                           "nu xi omicron pi rho sigma tau upsilon phi chi psi omega";
  const TextLayout original = coral::ui::layoutText(base, *font, 120.0f);
  ASSERT_GT(original.lines.size(), 1u);

  const auto verify = [&](const std::string& changed, const char* label) {
    SCOPED_TRACE(label);
    const TextLayout incremental =
      coral::ui::updateTextLayout(original, changed, *font, coral::ui::TextLayoutOptions{ 120.0f, 0, false });
    const TextLayout full = coral::ui::layoutText(changed, *font, 120.0f);
    ASSERT_EQ(incremental.lines.size(), full.lines.size());
    for (size_t index = 0; index < full.lines.size(); ++index) {
      EXPECT_EQ(incremental.lines[index].text, full.lines[index].text) << "line " << index;
      EXPECT_EQ(incremental.lines[index].start, full.lines[index].start) << "line " << index;
      EXPECT_EQ(incremental.lines[index].end, full.lines[index].end) << "line " << index;
      EXPECT_FLOAT_EQ(incremental.lines[index].width, full.lines[index].width) << "line " << index;
    }
  };

  verify(base + " tail", "append");
  verify(base.substr(0, 30) + " inserted " + base.substr(30), "insert");
  verify(base.substr(0, 40) + base.substr(60), "delete");
}

TEST(UnicodeLineBreakLayout, LazyRunsAreInterned) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const auto first = coral::ui::LazyTextBlob::create(font, "same run",
    graphics::Font::Direction::LeftToRight, false);
  const auto second = coral::ui::LazyTextBlob::create(font, "same run",
    graphics::Font::Direction::LeftToRight, false);
  const auto different = coral::ui::LazyTextBlob::create(font, "other run",
    graphics::Font::Direction::LeftToRight, false);
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first, second);
  EXPECT_NE(first, different);
  EXPECT_NE(first->get(), nullptr);
}

TEST(UnicodeLineBreakLayout, LineRefSharesVisualRunBlock) {
  auto block = std::make_shared<coral::ui::VisualRunBlock>();
  block->runs = { coral::ui::VisualRunEntry{ coral::ui::TextVisualRun{ 10, 12, 1 }, nullptr } };
  coral::ui::TextLine line;
  line.text = "a";
  line.width = 1.0f;
  line.start = 10;
  line.end = 11;
  line.visualRuns = coral::ui::VisualRunList(block);
  coral::ui::LineList source;
  source.push_back(line);
  coral::ui::LineList shifted;
  shifted.appendSharedShifted(source, 0, 1, 5);
  ASSERT_EQ(shifted.size(), 1u);
  ASSERT_EQ(shifted[0].visualRuns.size(), 1u);
  EXPECT_EQ(shifted[0].visualRuns[0].start, 15u);
  EXPECT_EQ(shifted[0].visualRuns[0].end, 17u);
  EXPECT_EQ(shifted[0].visualRuns.block().get(), block.get());
}

TEST(UnicodeLineBreakLayout, LineListShiftsAndSharesOffsetBlock) {
  coral::ui::LineList source;
  coral::ui::TextLine line;
  line.text = "a";
  line.width = 1.0f;
  line.start = 10;
  line.end = 11;
  source.push_back(line);
  coral::ui::LineList proxy;
  proxy.appendSharedShifted(source, 0, 1, 5);
  ASSERT_EQ(proxy.size(), 1u);
  EXPECT_EQ(source[0].start, 10u);
  EXPECT_EQ(proxy[0].start, 15u);
  const coral::ui::TextLine materialized = proxy[0];
  EXPECT_EQ(materialized.start, 15u);
}

TEST(UnicodeLineBreakLayout, BidiLineRunBlocksAreReused) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::resetBidiLineCacheStats();
  const TextLayout first = coral::ui::layoutText("abc \xD7\x90\xD7\x91\xD7\x92 xyz",
    *font, 60.0f);
  const TextLayout second = coral::ui::layoutText("abc \xD7\x90\xD7\x91\xD7\x92 uvw",
    *font, 60.0f);
  ASSERT_FALSE(first.lines.empty());
  ASSERT_FALSE(second.lines.empty());
  ASSERT_GE(first.lines.size(), 2u);
  ASSERT_GE(second.lines.size(), 2u);
  EXPECT_EQ(first.lines[1].text, second.lines[1].text);
  ASSERT_NE(first.lines[1].visualRuns.block(), nullptr);
  ASSERT_NE(second.lines[1].visualRuns.block(), nullptr);
  EXPECT_EQ(first.lines[1].visualRuns.block(), second.lines[1].visualRuns.block());
  const coral::ui::BidiLineCacheStats stats = coral::ui::bidiLineCacheStats();
  EXPECT_GT(stats.hits, 0u);
  EXPECT_GT(stats.stores, 0u);
  EXPECT_GT(stats.entries, 0u);
}

TEST(UnicodeLineBreakLayout, BidiParagraphLevelsAreShared) {
  const std::string text = "abc \xD7\x90\xD7\x91\xD7\x92";
  const auto ownerA = std::make_shared<const std::string>(text);
  const auto ownerB = std::make_shared<const std::string>(text);
  const auto first = coral::ui::resolveBidiParagraphShared(
    text, std::nullopt, ownerA, 0);
  const auto second = coral::ui::resolveBidiParagraphShared(
    text, std::nullopt, ownerB, 0);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(first, second);
  EXPECT_EQ(first->levels.size(), first->byteOffsets.size());
}

TEST(UnicodeLineBreakLayout, IncrementalBidiReflowsOneParagraph) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string oldText = "abc \xD7\x90\xD7\x91\xD7\x92\n"
                              "\xD7\x93\xD7\x94\xD7\x95 xyz\nrst";
  const std::string newText = "abc \xD7\x90\xD7\x91\xD7\x92\n"
                              "\xD7\x93\xD7\x94\xD7\x95 XYZ\nrst";
  const TextLayout original = coral::ui::layoutText(oldText, *font, 0.0f);
  const size_t editStart = oldText.find("xyz");
  const coral::ui::TextEditRange edit{ editStart, editStart + 3, editStart + 3 };
  const TextLayout incremental = coral::ui::updateTextLayout(
    original, newText, edit, *font, coral::ui::TextLayoutOptions{ 0.0f, 0, false });
  const TextLayout full = coral::ui::layoutText(newText, *font, 0.0f);
  ASSERT_EQ(incremental.lines.size(), full.lines.size());
  for (size_t index = 0; index < full.lines.size(); ++index) {
    EXPECT_EQ(incremental.lines[index].text, full.lines[index].text) << "line " << index;
    EXPECT_EQ(incremental.lines[index].start, full.lines[index].start) << "line " << index;
    EXPECT_EQ(incremental.lines[index].end, full.lines[index].end) << "line " << index;
    ASSERT_EQ(incremental.lines[index].visualRuns.size(),
              full.lines[index].visualRuns.size()) << "line " << index;
    for (size_t run = 0; run < full.lines[index].visualRuns.size(); ++run) {
      EXPECT_EQ(incremental.lines[index].visualRuns[run].level,
                full.lines[index].visualRuns[run].level);
    }
  }
}

TEST(UnicodeLineBreakLayout, IncrementalReflowAcceptsEditHint) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const TextLayout original = coral::ui::layoutText(
    "alpha beta gamma delta epsilon zeta eta theta", *font, 100.0f);
  const std::string inserted = "alpha XX beta gamma delta epsilon zeta eta theta";
  const coral::ui::TextEditRange insertRange{ 6, 6, 9 };
  const TextLayout incremental = coral::ui::updateTextLayout(
    original, inserted, insertRange, *font, coral::ui::TextLayoutOptions{ 100.0f, 0, false });
  const TextLayout full = coral::ui::layoutText(inserted, *font, 100.0f);
  ASSERT_EQ(incremental.lines.size(), full.lines.size());
  for (size_t index = 0; index < full.lines.size(); ++index) {
    EXPECT_EQ(incremental.lines[index].text, full.lines[index].text) << "line " << index;
    EXPECT_EQ(incremental.lines[index].start, full.lines[index].start) << "line " << index;
    EXPECT_EQ(incremental.lines[index].end, full.lines[index].end) << "line " << index;
  }

  const std::string deleted = "alpha beta gamma zeta eta theta";
  const coral::ui::TextEditRange deleteRange{ 16, 30, 16 };
  const TextLayout deletedLayout = coral::ui::updateTextLayout(
    original, deleted, deleteRange, *font, coral::ui::TextLayoutOptions{ 100.0f, 0, false });
  const TextLayout deletedFull = coral::ui::layoutText(deleted, *font, 100.0f);
  ASSERT_EQ(deletedLayout.lines.size(), deletedFull.lines.size());
  for (size_t index = 0; index < deletedFull.lines.size(); ++index) {
    EXPECT_EQ(deletedLayout.lines[index].text, deletedFull.lines[index].text) << "line " << index;
  }
}

TEST(UnicodeLineBreakLayout, IncrementalMoveSharesPrefixSource) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  TextLayout original = coral::ui::layoutText(
    "alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu", *font, 100.0f);
  ASSERT_GT(original.lines.size(), 1u);
  const std::shared_ptr<const coral::ui::TextSource> oldSource = original.source;
  const std::string text = "alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu tail";
  const TextLayout updated = coral::ui::updateTextLayout(std::move(original), text, *font,
    coral::ui::TextLayoutOptions{ 100.0f, 0, false });
  ASSERT_FALSE(updated.lines.empty());
  EXPECT_TRUE(std::find(updated.borrowedSources.begin(), updated.borrowedSources.end(), oldSource) !=
              updated.borrowedSources.end());
  EXPECT_EQ(updated.lines.front().text, "alpha beta");
  EXPECT_FALSE(updated.lines.front().text.empty());
  ASSERT_NE(updated.source, nullptr);
  EXPECT_TRUE(updated.source->equals(text));
  EXPECT_EQ(updated.source->toString(), text);
}

TEST(UnicodeLineBreakLayout, IncrementalReflowKeepsTrailingWhitespaceLine) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const TextLayout original = coral::ui::layoutText("a \nb", *font, 120.0f);
  const TextLayout incremental = coral::ui::updateTextLayout(
    original, "a x\nb", *font, coral::ui::TextLayoutOptions{ 120.0f, 0, false });
  const TextLayout full = coral::ui::layoutText("a x\nb", *font, 120.0f);
  ASSERT_EQ(incremental.lines.size(), full.lines.size());
  for (size_t index = 0; index < full.lines.size(); ++index) {
    EXPECT_EQ(incremental.lines[index].text, full.lines[index].text) << "line " << index;
    EXPECT_EQ(incremental.lines[index].start, full.lines[index].start) << "line " << index;
    EXPECT_EQ(incremental.lines[index].end, full.lines[index].end) << "line " << index;
  }
}

TEST(UnicodeLineBreakLayout, LinesShareShapedBlobs) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  TextLayout layout = coral::ui::layoutText(
    "alpha beta gamma delta epsilon zeta eta", *font, 80.0f);
  ASSERT_GT(layout.lines.size(), 1u);
  const auto shaped = layout.lines.front().shaped;
  ASSERT_NE(shaped, nullptr);
  TextLayout moved = coral::ui::updateTextLayout(std::move(layout),
    "alpha beta gamma delta epsilon zeta eta tail", *font,
    coral::ui::TextLayoutOptions{ 80.0f, 0, false });
  ASSERT_FALSE(moved.lines.empty());
  EXPECT_EQ(moved.lines.front().shaped, shaped);
  const coral::ui::TextLine materialized = moved.lines.front();
  EXPECT_EQ(materialized.shaped, shaped);
}

TEST(UnicodeLineBreakLayout, LinesViewLayoutOwnedSource) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  TextLayout layout = coral::ui::layoutText(std::string("owned source"), *font, 0.0f);
  ASSERT_FALSE(layout.lines.empty());
  EXPECT_EQ(layout.lines[0].text, "owned source");
  TextLayout copy = layout;
  EXPECT_EQ(copy.lines[0].text, "owned source");
  const coral::ui::TextLine line = copy.lines[0];
  EXPECT_EQ(line.text, "owned source");
}

TEST(UnicodeLineBreakLayout, ParagraphSegmentsPreserveOffsetsAndEmptyLines) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const TextLayout layout = coral::ui::layoutText("a\n\nb", *font, 0.0f);
  ASSERT_EQ(layout.lines.size(), 3u);
  EXPECT_EQ(layout.lines[0].text, "a");
  EXPECT_EQ(layout.lines[0].start, 0u);
  EXPECT_EQ(layout.lines[1].text, "");
  EXPECT_EQ(layout.lines[1].start, 2u);
  EXPECT_EQ(layout.lines[2].text, "b");
  EXPECT_EQ(layout.lines[2].start, 3u);
}

TEST(UnicodeLineBreakLayout, TextLayoutExposesBidiVisualRuns) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";

  const std::string ltrFirst = "abc \xD7\x90\xD7\x91\xD7\x92";
  const TextLayout ltr = coral::ui::layoutText(ltrFirst, *font, 0.0f);
  ASSERT_EQ(ltr.lines.size(), 1u);
  const auto ltrRuns = ltr.lines.front().visualRuns;
  ASSERT_EQ(ltrRuns.size(), 2u);
  EXPECT_EQ(ltrRuns[0].level, 0u);
  EXPECT_EQ(ltrRuns[1].level, 1u);
  EXPECT_EQ(coral::ui::visualRunOrder(ltr.lines.front()), (std::vector<size_t>{ 0, 1 }));
  EXPECT_NE(ltrRuns[0].lazyShaped, nullptr);
  EXPECT_NE(ltrRuns[1].lazyShaped, nullptr);
  EXPECT_NE(ltrRuns[0].lazyShaped->get(), nullptr);

  const std::string rtlFirst = "\xD7\x90\xD7\x91\xD7\x92 abc";
  const TextLayout rtl = coral::ui::layoutText(rtlFirst, *font, 0.0f);
  ASSERT_EQ(rtl.lines.size(), 1u);
  const auto rtlRuns = rtl.lines.front().visualRuns;
  ASSERT_GE(rtlRuns.size(), 2u);
  const std::vector<size_t> order = coral::ui::visualRunOrder(rtl.lines.front());
  ASSERT_EQ(order.size(), rtlRuns.size());
  EXPECT_GT(rtlRuns[order.front()].start, rtlRuns[order.back()].start);
  EXPECT_EQ(rtlRuns[order.front()].level % 2, 0u);
}

TEST(UnicodeLineBreakLayout, BidiMirroringSwapsPairedCharacters) {
  EXPECT_EQ(coral::ui::mirrorBidiText("(a)"), ")a(");
  EXPECT_EQ(coral::ui::mirrorBidiText("abc"), "abc");
}
TEST(UnicodeLineBreakLayout, RichTextExposesBidiVisualRuns) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RichTextRun run;
  run.text = "abc \xD7\x90\xD7\x91\xD7\x92";
  run.font = font;
  const RichTextLayout layout = coral::ui::layoutRichText({ run }, 0.0f);
  ASSERT_EQ(layout.lines.size(), 1u);
  const auto& runs = layout.lines.front().visualRuns;
  ASSERT_EQ(runs.size(), 2u);
  EXPECT_EQ(runs[0].level, 0u);
  EXPECT_EQ(runs[1].level, 1u);
  EXPECT_EQ(coral::ui::visualRunOrder(runs), (std::vector<size_t>{ 0, 1 }));
}
