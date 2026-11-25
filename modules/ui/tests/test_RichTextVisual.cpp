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

#include <ui/RichTextLayer.h>
#include <ui/SystemFonts.h>

#include <graphics/Font.h>
#include <graphics/Typeface.h>

#include <string>
#include <vector>

namespace {

std::shared_ptr<const graphics::Font> testFont(const float size) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  return typeface != nullptr ? graphics::Font::create(typeface, size) : nullptr;
}

} // namespace

TEST(UiRichTextVisual, HyphenationMarksTheLineAndAddsHyphenWidth) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "hyphenation";
  run.font = font;
  coral::ui::LineBreakPolicy policy;
  policy.language = "en";
  policy.hyphenation = true;
  const float hyphenWidth = font->measureWidth("\xE2\x80\x90");
  const float width = font->measureWidth("hyphen") + 1.0f;
  const coral::ui::RichTextLayout layout =
    coral::ui::layoutRichText({ run }, width, policy);
  ASSERT_GE(layout.lines.size(), 2u);
  EXPECT_TRUE(layout.lines.front().hyphenated);
  ASSERT_FALSE(layout.lines.front().segments.empty());
  EXPECT_EQ(layout.lines.front().segments.front().text, "hyphen");
  EXPECT_NEAR(layout.lines.front().width,
    font->measureWidth("hyphen") + hyphenWidth, 1.0f);
  EXPECT_FALSE(layout.lines.back().hyphenated);
}

TEST(UiRichTextVisual, HitTestingUsesVisualRunOrder) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "abc \xD7\x90\xD7\x91\xD7\x92";
  run.font = font;
  coral::ui::RichTextLayer layer({ run });
  layer.setBounds(coral::ui::Rect::MakeWH(220.0f, 40.0f));
  layer.measure({ 220.0f, 40.0f });
  ASSERT_EQ(layer.layout().lines.size(), 1u);
  ASSERT_EQ(layer.layout().lines.front().visualRuns.size(), 2u);
  const float latinWidth = font->measureWidth("abc ");
  const float hebrewWidth = font->measureWidth("\xD7\x90\xD7\x91\xD7\x92");
  EXPECT_EQ(layer.indexAt({ latinWidth + 1.0f, 4.0f }), run.text.size());
  EXPECT_EQ(layer.indexAt({ latinWidth + hebrewWidth - 1.0f, 4.0f }), 4u);
}

TEST(UiRichTextVisual, VerticalRtlRichRunsAreReorderedInsideTheColumn) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string hebrew = "\xD7\x90\xD7\x91\xD7\x92";
  coral::ui::RichTextRun run;
  run.text = hebrew;
  run.font = font;
  coral::ui::RichTextLayoutOptions options;
  options.maxWidth = 120.0f;
  options.lineHeight = 20.0f;
  options.orientation = coral::ui::TextOrientation::Vertical;
  const coral::ui::RichTextLayout layout = coral::ui::layoutRichText({ run }, options);
  ASSERT_EQ(layout.lines.size(), 1u);
  ASSERT_FALSE(layout.lines[0].visualRuns.empty());

  coral::ui::RichTextLayer layer({ run });
  layer.setLineHeight(20.0f);
  layer.setOrientation(coral::ui::TextOrientation::Vertical);
  layer.setBounds(coral::ui::Rect::MakeWH(20.0f, 120.0f));
  layer.measure({ 20.0f, 120.0f });
  const float left = layer.bounds().getLeft();
  const float top = layer.bounds().getTop();
  EXPECT_EQ(layer.indexAt({ left + 5.0f, top + 1.0f }), hebrew.size());
  EXPECT_EQ(layer.indexAt({ left + 5.0f, top + 100.0f }), 0u);
}

TEST(UiRichTextVisual, BidiSpansApplyToRichTextRuns) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "abc";
  run.font = font;
  coral::ui::RichTextLayoutOptions options;
  options.maxWidth = 300.0f;
  options.bidiSpans = { { 0, 3, coral::ui::UnicodeBidi::BidiOverride, coral::ui::TextDirection::Rtl } };
  const coral::ui::RichTextLayout layout = coral::ui::layoutRichText({ run }, options);
  ASSERT_EQ(layout.lines.size(), 1u);
  ASSERT_FALSE(layout.lines[0].visualRuns.empty());
  for (const coral::ui::TextVisualRun& visual: layout.lines[0].visualRuns) {
    EXPECT_EQ(visual.level % 2, 1);
  }
}

TEST(UiRichTextVisual, EllipsisLineHitTestingUsesTheSyntheticRun) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "\xD7\x90\xD7\x91\xD7\x92 \xD7\x90\xD7\x91\xD7\x92 \xD7\x90\xD7\x91\xD7\x92";
  run.font = font;
  coral::ui::RichTextLayoutOptions options;
  options.maxWidth = font->measureWidth("\xD7\x90\xD7\x91");
  options.maxLines = 1;
  options.ellipsis = true;
  options.direction = coral::ui::TextDirectionMode::Rtl;
  const coral::ui::RichTextLayout layout = coral::ui::layoutRichText({ run }, options);
  ASSERT_FALSE(layout.lines.empty());
  const coral::ui::RichTextLine& line = layout.lines.back();
  ASSERT_EQ(line.visualRuns.size(), 1u);

  coral::ui::RichTextLayer layer({ run });
  layer.setWrapWidth(options.maxWidth);
  layer.setMaxLines(1);
  layer.setEllipsis(true);
  layer.setDirection(coral::ui::TextDirectionMode::Rtl);
  layer.setBounds(coral::ui::Rect::MakeWH(options.maxWidth, 64.0f));
  layer.measure({ options.maxWidth, 64.0f });
  const float top = layer.bounds().getTop();
  const float left = layer.bounds().getLeft();
  const float right = layer.bounds().getRight();
  EXPECT_EQ(layer.indexAt({ right - 1.0f, top + 1.0f }), line.start)
          << "the top-right of an RTL ellipsis line is its logical start";
  EXPECT_EQ(layer.indexAt({ left + 1.0f, top + 1.0f }), line.end)
          << "the top-left of an RTL ellipsis line is its logical end";
}

TEST(UiRichTextVisual, DirectionSelectsTheParagraphBaseLevel) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "abc \xD7\x90\xD7\x91\xD7\x92";
  run.font = font;
  coral::ui::RichTextLayoutOptions options;
  options.maxWidth = 300.0f;
  options.direction = coral::ui::TextDirectionMode::Rtl;
  const coral::ui::RichTextLayout rtl = coral::ui::layoutRichText({ run }, options);
  ASSERT_FALSE(rtl.lines.empty());
  EXPECT_TRUE(rtl.lines[0].rtl);
  EXPECT_FALSE(rtl.lines[0].visualRuns.empty());
  EXPECT_EQ(rtl.direction, coral::ui::TextDirectionMode::Rtl);

  options.direction = coral::ui::TextDirectionMode::Ltr;
  const coral::ui::RichTextLayout ltr = coral::ui::layoutRichText({ run }, options);
  ASSERT_FALSE(ltr.lines.empty());
  EXPECT_FALSE(ltr.lines[0].rtl);
  EXPECT_EQ(ltr.direction, coral::ui::TextDirectionMode::Ltr);
}

TEST(UiRichTextVisual, EllipsisLineGetsASyntheticVisualRun) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "\xD7\x90\xD7\x91\xD7\x92 \xD7\x90\xD7\x91\xD7\x92 \xD7\x90\xD7\x91\xD7\x92";
  run.font = font;
  coral::ui::RichTextLayoutOptions options;
  options.maxWidth = font->measureWidth("\xD7\x90\xD7\x91");
  options.maxLines = 1;
  options.ellipsis = true;
  options.direction = coral::ui::TextDirectionMode::Rtl;
  const coral::ui::RichTextLayout layout = coral::ui::layoutRichText({ run }, options);
  ASSERT_FALSE(layout.lines.empty());
  const coral::ui::RichTextLine& line = layout.lines.back();
  EXPECT_TRUE(line.ellipsis);
  ASSERT_EQ(line.visualRuns.size(), 1u);
  ASSERT_EQ(line.visualOrder.size(), 1u);
  EXPECT_EQ(line.visualOrder[0], 0u);
  EXPECT_EQ(line.visualRuns[0].level % 2, 1u);
}

TEST(UiRichTextVisual, HyphenateZoneAndLineLimitsApplyToRichText) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "hyphenation";
  run.font = font;
  coral::ui::RichTextLayoutOptions options;
  options.maxWidth = font->measureWidth("hyphen") + font->measureWidth("\xE2\x80\x90") + 2.0f;
  options.policy.language = "en";
  options.policy.hyphenation = true;
  const coral::ui::RichTextLayout allowed = coral::ui::layoutRichText({ run }, options);
  ASSERT_GE(allowed.lines.size(), 2u);
  EXPECT_TRUE(allowed.lines.front().hyphenated);

  options.policy.hyphenateLimitZone = 1.0f;
  const coral::ui::RichTextLayout rejected = coral::ui::layoutRichText({ run }, options);
  EXPECT_EQ(rejected.lines.size(), 1u) << "slack exceeds the zone";
  EXPECT_FALSE(rejected.lines.front().hyphenated);

  options.policy.hyphenateLimitZone = 0.0f;
  options.policy.hyphenateLimitLines = 1;
  run.text = "hyphenation hyphenation";
  const coral::ui::RichTextLayout limited = coral::ui::layoutRichText({ run }, options);
  size_t consecutive = 0;
  for (const coral::ui::RichTextLine& line: limited.lines) {
    consecutive = line.hyphenated ? consecutive + 1 : 0;
    EXPECT_LE(consecutive, 1u);
  }
}

TEST(UiRichTextVisual, VerticalColumnsAndHitTesting) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "abc def";
  run.font = font;
  coral::ui::RichTextLayoutOptions options;
  options.maxWidth = font->measureWidth("abc ");
  options.lineHeight = 20.0f;
  options.orientation = coral::ui::TextOrientation::Vertical;
  const coral::ui::RichTextLayout layout = coral::ui::layoutRichText({ run }, options);
  ASSERT_EQ(layout.lines.size(), 2u);
  EXPECT_EQ(layout.orientation, coral::ui::TextOrientation::Vertical);
  EXPECT_FLOAT_EQ(layout.size.width, 40.0f);

  coral::ui::RichTextLayer layer({ run });
  layer.setLineHeight(20.0f);
  layer.setOrientation(coral::ui::TextOrientation::Vertical);
  layer.setBounds(coral::ui::Rect::MakeWH(40.0f, options.maxWidth));
  layer.measure({ 40.0f, options.maxWidth });
  const float left = layer.bounds().getLeft();
  const float top = layer.bounds().getTop();
  EXPECT_EQ(layer.indexAt({ left + 5.0f, top + 1.0f }), 0u);
  EXPECT_EQ(layer.indexAt({ left + 25.0f, top + 1.0f }), 4u);
}

TEST(UiRichTextVisual, JustifySpreadsSpacesAndKeepsTheLastLineFlush) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "one two three four five";
  run.font = font;
  coral::ui::RichTextLayoutOptions options;
  options.maxWidth = font->measureWidth("one two three ");
  options.align = coral::ui::TextAlign::Justify;
  const coral::ui::RichTextLayout layout = coral::ui::layoutRichText({ run }, options);
  ASSERT_GE(layout.lines.size(), 2u);
  const auto& first = layout.lines.front();
  ASSERT_GT(first.justifySpacing, 0.0f);
  size_t spaces = 0;
  for (const coral::ui::RichTextSegment& segment: first.segments) {
    spaces += static_cast<size_t>(std::count(segment.text.begin(), segment.text.end(), ' '));
  }
  ASSERT_GT(spaces, 0u);
  EXPECT_FLOAT_EQ(first.justifySpacing,
    (options.maxWidth - first.width) / static_cast<float>(spaces));
  EXPECT_FLOAT_EQ(layout.lines.back().justifySpacing, 0.0f);

  coral::ui::RichTextLayer layer({ run });
  layer.setTextAlign(coral::ui::TextAlign::Justify);
  layer.setBounds(coral::ui::Rect::MakeWH(options.maxWidth, 80.0f));
  layer.measure({ options.maxWidth, 80.0f });
  const coral::ui::RichTextLine line = layer.layout().lines.front();
  EXPECT_EQ(layer.indexAt({ layer.bounds().getLeft() + options.maxWidth - 1.0f, 5.0f }),
    line.end);
}

TEST(UiRichTextVisual, LineHeightAndAlignmentApplyToRichLines) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "a\nbbbb";
  run.font = font;
  coral::ui::RichTextLayoutOptions options;
  options.maxWidth = 120.0f;
  options.lineHeight = 30.0f;
  options.align = coral::ui::TextAlign::Center;
  const coral::ui::RichTextLayout layout = coral::ui::layoutRichText({ run }, options);
  ASSERT_EQ(layout.lines.size(), 2u);
  EXPECT_FLOAT_EQ(layout.lineHeight, 30.0f);
  EXPECT_FLOAT_EQ(layout.size.height, 60.0f);
  for (const coral::ui::RichTextLine& line: layout.lines) {
    EXPECT_FLOAT_EQ(line.height, 30.0f);
    EXPECT_FLOAT_EQ(line.xOffset, (options.maxWidth - line.width) * 0.5f);
  }
  EXPECT_GT(layout.lines[0].xOffset, layout.lines[1].xOffset);

  coral::ui::RichTextLayer layer({ run });
  layer.setLineHeight(30.0f);
  layer.setBounds(coral::ui::Rect::MakeWH(120.0f, 80.0f));
  layer.measure({ 120.0f, 80.0f });
  EXPECT_FLOAT_EQ(layer.measuredSize().height, 60.0f);
}

TEST(UiRichTextVisual, HyphenWidthIsReservedWhenBreaking) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "hyphenation";
  run.font = font;
  coral::ui::LineBreakPolicy policy;
  policy.language = "en";
  policy.hyphenation = true;
  const float maxWidth = font->measureWidth("hyphen") + font->measureWidth("\xE2\x80\x90");
  coral::ui::RichTextLayoutOptions options;
  options.maxWidth = maxWidth;
  options.policy = policy;
  const coral::ui::RichTextLayout layout = coral::ui::layoutRichText({ run }, options);
  ASSERT_GE(layout.lines.size(), 2u);
  EXPECT_TRUE(layout.lines.front().hyphenated);
  EXPECT_LE(layout.lines.front().width, maxWidth + 0.5f);
}

TEST(UiRichTextVisual, MaxLinesAndEllipsisTruncateTheLastLine) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "one two three four five six seven";
  run.font = font;
  coral::ui::RichTextLayoutOptions options;
  options.maxWidth = font->measureWidth("one two ");
  options.maxLines = 1;
  options.ellipsis = true;
  const coral::ui::RichTextLayout layout = coral::ui::layoutRichText({ run }, options);
  ASSERT_EQ(layout.lines.size(), 1u);
  EXPECT_TRUE(layout.truncated);
  ASSERT_FALSE(layout.lines.front().segments.empty());
  const std::string& text = layout.lines.front().segments.back().text;
  ASSERT_GE(text.size(), 3u);
  EXPECT_EQ(text.substr(text.size() - 3), "\xE2\x80\xA6");
  EXPECT_LE(layout.lines.front().width, options.maxWidth + 0.5f);

  coral::ui::RichTextLayer layer({ run });
  layer.setMaxLines(1);
  layer.setEllipsis(true);
  layer.setBounds(coral::ui::Rect::MakeWH(options.maxWidth, 80.0f));
  layer.measure({ options.maxWidth, 80.0f });
  EXPECT_EQ(layer.layout().lines.size(), 1u);
  ASSERT_FALSE(layer.layout().lines.front().segments.empty());
  EXPECT_NE(layer.layout().lines.front().segments.back().text.find("\xE2\x80\xA6"), std::string::npos);
}

TEST(UiRichTextVisual, BackScannedHyphenMustAlsoFitTheLine) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "a hyphenation";
  run.font = font;
  coral::ui::LineBreakPolicy policy;
  policy.language = "en";
  policy.hyphenation = true;
  coral::ui::RichTextLayoutOptions options;
  options.policy = policy;
  options.maxWidth = font->measureWidth("a ") + 5.0f;
  const coral::ui::RichTextLayout layout = coral::ui::layoutRichText({ run }, options);
  ASSERT_GE(layout.lines.size(), 2u);
  EXPECT_FALSE(layout.lines.front().hyphenated);
  EXPECT_EQ(layout.lines.front().segments.front().text, "a");
  EXPECT_LE(layout.lines.front().width, options.maxWidth + 0.5f);
}

TEST(UiRichTextVisual, CachedLayoutsShareTheirLineStorage) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "shared lines are not copied on a cache hit";
  run.font = font;
  const coral::ui::RichTextLayout first = coral::ui::layoutRichText({ run }, 120.0f);
  const coral::ui::RichTextLayout second = coral::ui::layoutRichText({ run }, 120.0f);
  ASSERT_FALSE(first.lines.empty());
  EXPECT_EQ(&first.lines.vector(), &second.lines.vector());
  EXPECT_EQ(first.lines.size(), second.lines.size());
}

TEST(UiRichTextVisual, IncrementalReflowMatchesAFullLayout) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "alpha beta gamma delta epsilon zeta";
  run.font = font;
  coral::ui::RichTextLayoutOptions options;
  options.maxWidth = font->measureWidth("alpha beta gamma ");
  const coral::ui::RichTextLayout first = coral::ui::layoutRichText({ run }, options);
  ASSERT_GE(first.lines.size(), 2u);

  coral::ui::RichTextRun edited = run;
  edited.text = "alpha beta X gamma delta epsilon zeta";
  const coral::ui::RichTextLayout updated =
    coral::ui::updateRichTextLayout(first, { edited }, options);
  const coral::ui::RichTextLayout full = coral::ui::layoutRichText({ edited }, options);
  ASSERT_EQ(updated.lines.size(), full.lines.size());
  for (size_t index = 0; index < updated.lines.size(); ++index) {
    std::string updatedText;
    std::string fullText;
    for (const auto& segment: updated.lines[index].segments) updatedText += segment.text;
    for (const auto& segment: full.lines[index].segments) fullText += segment.text;
    EXPECT_EQ(updatedText, fullText) << "line " << index;
  }
  EXPECT_FLOAT_EQ(updated.size.width, full.size.width);
  EXPECT_FLOAT_EQ(updated.size.height, full.size.height);
  EXPECT_GE(updated.lines.segmentCount(), 2u)
    << "the unchanged prefix/suffix must be shared instead of copied";
}

TEST(UiRichTextVisual, IncrementalReflowHandlesTailAppends) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "alpha beta gamma";
  run.font = font;
  coral::ui::RichTextLayoutOptions options;
  options.maxWidth = font->measureWidth("alpha beta ");
  const coral::ui::RichTextLayout first = coral::ui::layoutRichText({ run }, options);
  coral::ui::RichTextRun extended = run;
  extended.text += " delta epsilon";
  const coral::ui::RichTextLayout updated =
    coral::ui::updateRichTextLayout(first, { extended }, options);
  const coral::ui::RichTextLayout full = coral::ui::layoutRichText({ extended }, options);
  ASSERT_EQ(updated.lines.size(), full.lines.size());
  for (size_t index = 0; index < updated.lines.size(); ++index) {
    std::string updatedText;
    std::string fullText;
    for (const auto& segment: updated.lines[index].segments) updatedText += segment.text;
    for (const auto& segment: full.lines[index].segments) fullText += segment.text;
    EXPECT_EQ(updatedText, fullText) << "line " << index;
  }
}

TEST(UiRichTextVisual, SystemFallbackSplitsEmojiRun) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  if (font->typeface()->hasGlyphForCodePoint(0x1F600u)) {
    GTEST_SKIP() << "the primary face already covers emoji";
  }
  coral::ui::setSystemFontFilesForTesting({ std::string(CORAL_ASSETS_DIR) + "/fonts/NotoColorEmoji.ttf" });
  coral::ui::RichTextRun run;
  run.text = "A\xF0\x9F\x98\x80";
  run.font = font;
  coral::ui::RichTextLayer layer({ run });
  layer.setSystemFontFallback(true);
  layer.setBounds(coral::ui::Rect::MakeWH(160.0f, 40.0f));
  layer.measure({ 160.0f, 40.0f });
  ASSERT_EQ(layer.layout().lines.size(), 1u);
  const auto& segments = layer.layout().lines.front().segments;
  ASSERT_GE(segments.size(), 2u);
  EXPECT_EQ(segments.front().font->typeface(), font->typeface());
  EXPECT_NE(segments.back().font->typeface(), font->typeface());
  coral::ui::setSystemFontFilesForTesting({});
}

TEST(UiRichTextVisual, SelectionAcrossReorderedRunsIsStoredAndSafe) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "abc \xD7\x90\xD7\x91\xD7\x92";
  run.font = font;
  coral::ui::RichTextLayer layer({ run });
  layer.setBounds(coral::ui::Rect::MakeWH(220.0f, 40.0f));
  layer.measure({ 220.0f, 40.0f });
  layer.setSelectable(true);
  layer.setSelection(2, 9);
  EXPECT_TRUE(layer.hasSelection());
  EXPECT_EQ(layer.layout().lines.front().visualRuns.size(), 2u);
}

TEST(UiRichTextVisual, RunFontFeaturesReachTheSegmentAndLayout) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "fi kern";
  run.font = font;
  run.features = { graphics::FontFeature{ .tag = "kern", .value = 0 } };
  coral::ui::RichTextLayer layer({ run });
  layer.setBounds(coral::ui::Rect::MakeWH(220.0f, 40.0f));
  layer.measure({ 220.0f, 40.0f });
  ASSERT_FALSE(layer.layout().lines.empty());
  const auto& segments = layer.layout().lines.front().segments;
  ASSERT_FALSE(segments.empty());
  ASSERT_EQ(segments.front().features.size(), 1u);
  EXPECT_EQ(segments.front().features.front().tag, "kern");
  EXPECT_EQ(segments.front().features.front().value, 0u);
}
