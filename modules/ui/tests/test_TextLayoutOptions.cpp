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

#include <ui/TextLayer.h>
#include "ShapedAdvances.h"
#include <graphics/FontFallback.h>
#include <ui/RichText.h>
#include <ui/TextLayout.h>

#include <graphics/Font.h>
#include <graphics/Typeface.h>

#include <algorithm>
#include <string>

namespace {

std::shared_ptr<const graphics::Font> testFont(const float size) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  return typeface != nullptr ? graphics::Font::create(typeface, size) : nullptr;
}

} // namespace

TEST(UiTextOptions, LineHeightOverridesTheFontLineBox) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::TextLayoutOptions options;
  options.maxWidth = 100.0f;
  options.lineHeight = 40.0f;
  const coral::ui::TextLayout layout = coral::ui::layoutText("a\nbbbb", *font, options);
  ASSERT_EQ(layout.lines.size(), 2u);
  EXPECT_FLOAT_EQ(layout.lineHeight, 40.0f);
  EXPECT_FLOAT_EQ(layout.size.height, 80.0f);
  EXPECT_GT(layout.size.width, 0.0f);
}

TEST(UiTextOptions, AlignmentOffsetsLinesInsideTheWrapWidth) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";

  coral::ui::TextLayoutOptions options;
  options.maxWidth = 120.0f;
  options.align = coral::ui::TextAlign::Center;
  const coral::ui::TextLayout centered = coral::ui::layoutText("a\nbbbb", *font, options);
  ASSERT_EQ(centered.lines.size(), 2u);
  EXPECT_FLOAT_EQ(centered.lines[0].xOffset,
    (options.maxWidth - centered.lines[0].width) * 0.5f);
  EXPECT_FLOAT_EQ(centered.lines[1].xOffset,
    (options.maxWidth - centered.lines[1].width) * 0.5f);
  EXPECT_GT(centered.lines[0].xOffset, centered.lines[1].xOffset)
          << "the shorter line is centred further from the left edge";

  options.align = coral::ui::TextAlign::Right;
  const coral::ui::TextLayout right = coral::ui::layoutText("a\nbbbb", *font, options);
  ASSERT_EQ(right.lines.size(), 2u);
  EXPECT_FLOAT_EQ(right.lines[0].xOffset, options.maxWidth - right.lines[0].width);
  EXPECT_FLOAT_EQ(right.lines[1].xOffset, options.maxWidth - right.lines[1].width);
}

TEST(UiTextOptions, JustifyDistributesSpacesAndLeavesTheLastLineFlush) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::TextLayoutOptions options;
  options.maxWidth = font->measureWidth("one two three ");
  options.align = coral::ui::TextAlign::Justify;
  const coral::ui::TextLayout layout =
    coral::ui::layoutText("one two three four five", *font, options);
  ASSERT_GE(layout.lines.size(), 2u);
  const auto& first = layout.lines[0];
  const auto spaces = static_cast<float>(
    std::count(first.text.begin(), first.text.end(), ' '));
  ASSERT_GT(spaces, 0.0f);
  EXPECT_GT(first.justifySpacing, 0.0f);
  EXPECT_FLOAT_EQ(first.justifySpacing,
    (options.maxWidth - first.width) / spaces);
  EXPECT_FLOAT_EQ(layout.lines.back().justifySpacing, 0.0f)
          << "the last line of a paragraph stays flush left";

  coral::ui::TextLayer layer("one two three four five", font);
  layer.setWrapWidth(options.maxWidth);
  layer.setTextAlign(coral::ui::TextAlign::Justify);
  layer.setLineHeight(30.0f);
  layer.setBounds(coral::ui::Rect::MakeWH(options.maxWidth, 90.0f));
  layer.measure({ options.maxWidth, 90.0f });
  const coral::ui::TextLine line = layer.layout().lines[0];
  EXPECT_FLOAT_EQ(layer.measuredSize().height, 30.0f * 2.0f);
  const coral::ui::Point rightEdge{
    layer.bounds().getLeft() + options.maxWidth - 1.0f,
    layer.bounds().getTop() + 5.0f };
  EXPECT_EQ(layer.indexAt(rightEdge), line.end)
          << "hit testing accounts for the justification advance";
}

TEST(UiTextOptions, VerticalLayoutUsesColumnsAndIndexesWithinAColumn) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::TextLayoutOptions options;
  options.maxWidth = font->measureWidth("abc ");
  options.lineHeight = 20.0f;
  options.orientation = coral::ui::TextOrientation::Vertical;
  const coral::ui::TextLayout layout = coral::ui::layoutText("abc def", *font, options);
  ASSERT_EQ(layout.orientation, coral::ui::TextOrientation::Vertical);
  ASSERT_EQ(layout.lines.size(), 2u);
  EXPECT_FLOAT_EQ(layout.size.width, 40.0f);
  EXPECT_LE(layout.size.height, options.maxWidth + 0.5f);

  coral::ui::TextLayer layer("abc def", font);
  layer.setWrapWidth(options.maxWidth);
  layer.setLineHeight(20.0f);
  layer.setOrientation(coral::ui::TextOrientation::Vertical);
  layer.setBounds(coral::ui::Rect::MakeWH(40.0f, options.maxWidth));
  layer.measure({ 40.0f, options.maxWidth });
  const float left = layer.bounds().getLeft();
  const float top = layer.bounds().getTop();
  EXPECT_EQ(layer.indexAt({ left + 5.0f, top + 1.0f }), 0u);
  EXPECT_EQ(layer.indexAt({ left + 25.0f, top + 1.0f }), 4u);
  EXPECT_EQ(layer.indexAt({ left + 25.0f, top + options.maxWidth - 1.0f }), 7u);
}

TEST(UiTextOptions, VerticalRlColumnsAdvanceLeftward) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::TextLayoutOptions options;
  options.maxWidth = font->measureWidth("abc ");
  options.lineHeight = 20.0f;
  options.orientation = coral::ui::TextOrientation::VerticalRl;
  const coral::ui::TextLayout layout = coral::ui::layoutText("abc def", *font, options);
  ASSERT_EQ(layout.orientation, coral::ui::TextOrientation::VerticalRl);
  EXPECT_FLOAT_EQ(layout.size.width, 40.0f);

  coral::ui::TextLayer layer("abc def", font);
  layer.setWrapWidth(options.maxWidth);
  layer.setLineHeight(20.0f);
  layer.setOrientation(coral::ui::TextOrientation::VerticalRl);
  layer.setBounds(coral::ui::Rect::MakeWH(40.0f, options.maxWidth));
  layer.measure({ 40.0f, options.maxWidth });
  const float left = layer.bounds().getLeft();
  const float right = layer.bounds().getRight();
  const float top = layer.bounds().getTop();
  EXPECT_EQ(layer.indexAt({ right - 5.0f, top + 1.0f }), 0u)
          << "the first column is on the right in vertical-rl";
  EXPECT_EQ(layer.indexAt({ left + 5.0f, top + 1.0f }), 4u);
}

TEST(UiTextOptions, VerticalMixedKeepsCjkUprightAndRotatesLatin) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::TextLayoutOptions options;
  options.maxWidth = font->measureWidth("あい ");
  options.lineHeight = 20.0f;
  options.orientation = coral::ui::TextOrientation::VerticalMixed;
  const coral::ui::TextLayout layout = coral::ui::layoutText("あい abc", *font, options);
  EXPECT_EQ(layout.orientation, coral::ui::TextOrientation::VerticalMixed);
  EXPECT_GT(layout.size.width, 0.0f);
  EXPECT_GT(layout.size.height, 0.0f);
}

TEST(UiTextOptions, DirectionOptionOverridesTheParagraphBase) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::TextLayoutOptions options;
  options.maxWidth = 200.0f;
  options.direction = coral::ui::TextDirectionMode::Rtl;
  const coral::ui::TextLayout rtl = coral::ui::layoutText("abc", *font, options);
  EXPECT_EQ(rtl.directionMode, coral::ui::TextDirectionMode::Rtl);
  EXPECT_EQ(rtl.baseDirection, coral::ui::TextDirection::Rtl);

  const std::string hebrew = "\xD7\x90\xD7\x91\xD7\x92";
  options.direction = coral::ui::TextDirectionMode::Ltr;
  const coral::ui::TextLayout ltr = coral::ui::layoutText(hebrew, *font, options);
  EXPECT_EQ(ltr.baseDirection, coral::ui::TextDirection::Ltr);

  coral::ui::TextLayer layer("abc", font);
  layer.setDirection(coral::ui::TextDirectionMode::Rtl);
  layer.setBounds(coral::ui::Rect::MakeWH(200.0f, 40.0f));
  layer.measure({ 200.0f, 40.0f });
  EXPECT_EQ(layer.layout().baseDirection, coral::ui::TextDirection::Rtl);
}

TEST(UiTextOptions, RtlEllipsisLineIsMarkedForRtlDrawing) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::TextLayoutOptions options;
  options.maxWidth = font->measureWidth("\xD7\x90\xD7\x91");
  options.maxLines = 1;
  options.ellipsis = true;
  options.direction = coral::ui::TextDirectionMode::Rtl;
  options.policy.overflowWrap = coral::ui::OverflowWrap::BreakWord;
  const std::string hebrew = "\xD7\x90\xD7\x91\xD7\x92\xD7\x93\xD7\x94";
  const coral::ui::TextLayout layout = coral::ui::layoutText(hebrew, *font, options);
  ASSERT_FALSE(layout.lines.empty());
  EXPECT_TRUE(layout.truncated);
  EXPECT_TRUE(layout.lines.back().rtl);
}

TEST(UiTextOptions, BidiSpansApplyOverrideAndNesting) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::TextLayoutOptions options;
  options.maxWidth = 300.0f;
  options.bidiSpans = { { 0, 3, coral::ui::UnicodeBidi::BidiOverride, coral::ui::TextDirection::Rtl } };
  const coral::ui::TextLayout overrideLayout = coral::ui::layoutText("abc", *font, options);
  ASSERT_EQ(overrideLayout.lines.size(), 1u);
  ASSERT_FALSE(overrideLayout.lines[0].visualRuns.empty());
  for (const auto run: overrideLayout.lines[0].visualRuns) {
    EXPECT_EQ(run.level % 2, 1) << "bidi-override: rtl forces odd levels";
  }

  options.bidiSpans = {
    { 0, 6, coral::ui::UnicodeBidi::BidiOverride, coral::ui::TextDirection::Rtl },
    { 1, 3, coral::ui::UnicodeBidi::BidiOverride, coral::ui::TextDirection::Ltr },
  };
  const coral::ui::TextLayout nested = coral::ui::layoutText("abcdef", *font, options);
  ASSERT_EQ(nested.lines.size(), 1u);
  bool hasOdd = false;
  bool hasEven = false;
  for (const auto run: nested.lines[0].visualRuns) {
    hasOdd = hasOdd || run.level % 2 == 1;
    hasEven = hasEven || run.level % 2 == 0;
  }
  EXPECT_TRUE(hasOdd);
  EXPECT_TRUE(hasEven) << "the nested LTR override must raise to an even level";
}

TEST(UiTextOptions, PlaintextSpanChoosesItsOwnBaseDirection) {
  const std::string text = "\xD7\x90\xD7\x91\xD7\x92 abc";
  const std::vector<coral::ui::BidiSpan> spans{
    { 7, 10, coral::ui::UnicodeBidi::Plaintext, coral::ui::TextDirection::Rtl } };
  const coral::ui::BidiParagraph paragraph = coral::ui::resolveBidiParagraphWithSpans(
    text, coral::ui::TextDirection::Rtl, spans);
  ASSERT_EQ(paragraph.levels.size(), 7u);
  // `levels` is one entry per code point, while the span above is a UTF-8 byte range: "abc" starts at
  // code point 4 (three Hebrew letters, a space, then the Latin run).
  EXPECT_EQ(paragraph.levels[4] % 2, 0)
    << "plaintext must pick LTR from the span's first strong character, ignoring the RTL property";
}

TEST(UiTextOptions, BidiSpansEmbedAndIsolateForceDirection) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  for (const auto mode: { coral::ui::UnicodeBidi::Embed, coral::ui::UnicodeBidi::Isolate }) {
    coral::ui::TextLayoutOptions options;
    options.maxWidth = 300.0f;
    options.bidiSpans = { { 0, 3, mode, coral::ui::TextDirection::Rtl } };
    const coral::ui::TextLayout layout = coral::ui::layoutText("abc", *font, options);
    ASSERT_EQ(layout.lines.size(), 1u);
    ASSERT_FALSE(layout.lines[0].visualRuns.empty());
    for (const auto run: layout.lines[0].visualRuns) {
      EXPECT_EQ(run.level % 2, 0) << "RLE/LRI embedding raises LTR text to level 2";
    }
  }
}

TEST(UiTextOptions, VerticalRtlRunsAreReorderedInsideTheColumn) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string hebrew = "\xD7\x90\xD7\x91\xD7\x92";
  coral::ui::TextLayoutOptions options;
  options.maxWidth = 120.0f;
  options.lineHeight = 20.0f;
  options.orientation = coral::ui::TextOrientation::Vertical;
  const coral::ui::TextLayout layout = coral::ui::layoutText(hebrew, *font, options);
  ASSERT_EQ(layout.lines.size(), 1u);
  ASSERT_FALSE(layout.lines[0].visualRuns.empty());

  coral::ui::TextLayer layer(hebrew, font);
  layer.setWrapWidth(120.0f);
  layer.setLineHeight(20.0f);
  layer.setOrientation(coral::ui::TextOrientation::Vertical);
  layer.setBounds(coral::ui::Rect::MakeWH(20.0f, 120.0f));
  layer.measure({ 20.0f, 120.0f });
  const float left = layer.bounds().getLeft();
  const float top = layer.bounds().getTop();
  EXPECT_EQ(layer.indexAt({ left + 5.0f, top + 1.0f }), hebrew.size())
          << "the top of an RTL column is the logical end";
  EXPECT_EQ(layer.indexAt({ left + 5.0f, top + 100.0f }), 0u)
          << "the bottom of an RTL column is the logical start";
}

TEST(UiTextOptions, LineHeightSurvivesIncrementalReflow) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::TextLayoutOptions options;
  options.maxWidth = 120.0f;
  options.lineHeight = 32.0f;
  const coral::ui::TextLayout original = coral::ui::layoutText("alpha beta gamma", *font, options);
  const coral::ui::TextLayout updated = coral::ui::updateTextLayout(
    original, "alpha beta gamma delta", *font, options);
  EXPECT_FLOAT_EQ(updated.lineHeight, 32.0f);
  EXPECT_FLOAT_EQ(updated.size.height,
    32.0f * static_cast<float>(updated.lines.size()));
}

TEST(UiTextLayoutOptions, WrappingUsesShapedAdvancesNotPerCodePointSums) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";

  // A kerning pair is the cheapest way to make the two width models disagree: summing single-glyph
  // advances overestimates "AV". If this font has no such pair the test cannot discriminate and is
  // skipped instead of passing vacuously.
  const std::string word = "AV";
  if (font->measureWidth(word) >= font->measureWidth("A") + font->measureWidth("V") - 0.05f) {
    GTEST_SKIP() << "the test font has no 'AV' kerning pair";
  }

  const std::string text = "AV AV AV AV AV AV";
  const float shapedLine = font->measureWidth(text);
  // The sample text is ASCII, so one byte is one code point here.
  float perCodePointSum = 0.0f;
  for (const char character: text) {
    perCodePointSum += font->measureWidth(std::string(1, character));
  }
  if (perCodePointSum <= shapedLine + 0.5f) {
    GTEST_SKIP() << "the two width models are too close to discriminate";
  }

  coral::ui::TextLayoutOptions options;
  options.maxWidth = shapedLine + 0.5f;
  const coral::ui::TextLayout layout = coral::ui::layoutText(text, *font, options);
  ASSERT_EQ(layout.lines.size(), 1u)
    << "the paragraph fits when measured the way it is drawn; the per-code-point sum would split it";
  EXPECT_LE(layout.lines.front().width, options.maxWidth + 0.01f);
  EXPECT_EQ(layout.lines.front().text, text);

  // The invariant that matters for consumers: a wrapped line never exceeds the width it was measured
  // against, and the width it reports is the shaped width that drawing uses.
  coral::ui::TextLayoutOptions narrow;
  narrow.maxWidth = shapedLine * 0.45f;
  const coral::ui::TextLayout wrapped = coral::ui::layoutText(text, *font, narrow);
  ASSERT_GT(wrapped.lines.size(), 1u);
  for (const coral::ui::TextLine& line: wrapped.lines) {
    EXPECT_LE(line.width, narrow.maxWidth + 0.01f) << "line '" << line.text << "' overflows its width";
    EXPECT_FLOAT_EQ(line.width, font->measureWidth(line.text));
  }
}

TEST(UiTextLayoutOptions, FallbackChainUsesTheSameShapedAdvances) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string text = "AV AV AV AV AV AV";
  if (font->measureWidth("AV") >= font->measureWidth("A") + font->measureWidth("V") - 0.05f) {
    GTEST_SKIP() << "the test font has no 'AV' kerning pair";
  }

  coral::ui::TextLayoutOptions options;
  options.maxWidth = font->measureWidth(text) + 0.5f;
  const graphics::FontFallback fallback(font);
  const coral::ui::TextLayout plain = coral::ui::layoutText(text, *font, options);
  const coral::ui::TextLayout chained = coral::ui::layoutText(text, *font, fallback, options);
  ASSERT_EQ(plain.lines.size(), 1u);
  ASSERT_EQ(chained.lines.size(), plain.lines.size())
    << "a primary-only fallback chain must not change where the line breaks";
  EXPECT_FLOAT_EQ(chained.lines.front().width, plain.lines.front().width);
}

TEST(UiTextLayoutOptions, RichTextBreaksLikePlainTextForOneRun) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string text = "AV AV AV AV AV AV";
  if (font->measureWidth("AV") >= font->measureWidth("A") + font->measureWidth("V") - 0.05f) {
    GTEST_SKIP() << "the test font has no 'AV' kerning pair";
  }

  coral::ui::TextLayoutOptions options;
  options.maxWidth = font->measureWidth(text) + 0.5f;
  const coral::ui::TextLayout plain = coral::ui::layoutText(text, *font, options);
  ASSERT_EQ(plain.lines.size(), 1u);

  coral::ui::RichTextRun run;
  run.text = text;
  run.font = font;
  const coral::ui::RichTextLayout rich = coral::ui::layoutRichText({ run }, options.maxWidth);
  ASSERT_EQ(rich.lines.size(), plain.lines.size())
    << "rich text and plain text must agree on line breaking for one run";
  float widest = 0.0f;
  for (const coral::ui::RichTextLine& line: rich.lines) {
    widest = std::max(widest, line.width);
  }
  EXPECT_LE(widest, options.maxWidth + 0.01f);
}

TEST(UiTextLayoutOptions, ShapedAdvancesMatchTheMeasuredWidthForMixedScripts) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";

  // Width agreement must not depend on a specific face having coverage for the script: the table and
  // measureWidth must be two views of the same shaping pass (missing glyphs included).
  const std::vector<std::string> samples = {
    "Hello",
    "AVAVAV",
    "\xE4\xBD\xA0\xE5\xA5\xBD",                        // CJK
    "abc \xD7\x90\xD7\x91\xD7\x92 def",                // Hebrew inside Latin
    "\xD8\xA7\xD9\x84\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A\xD8\xA9", // Arabic
    "\xF0\x9F\x98\x80",                                // emoji
  };
  for (const std::string& sample: samples) {
    const coral::ui::detail::ShapedAdvances table =
      coral::ui::detail::ShapedAdvances::fromFont(*font, sample);
    if (!table.valid()) continue;
    EXPECT_NEAR(table.width(), font->measureWidth(sample), 0.01f) << "sample: " << sample;
  }

  // The same invariant on the layout path: a wrapped RTL paragraph never reports a line wider than
  // the width it was measured against.
  const std::string mixed = "abc \xD7\x90\xD7\x91\xD7\x92 def \xD8\xA7\xD9\x84\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A\xD8\xA9 ghi";
  coral::ui::TextLayoutOptions options;
  options.maxWidth = font->measureWidth(mixed) * 0.35f;
  // A single unbreakable word is allowed to overflow under CSS `overflow-wrap: normal`; ask for
  // emergency breaks so every line is expected to fit.
  options.policy.overflowWrap = coral::ui::OverflowWrap::BreakWord;
  const coral::ui::TextLayout layout = coral::ui::layoutText(mixed, *font, options);
  ASSERT_FALSE(layout.lines.empty());
  for (const coral::ui::TextLine& line: layout.lines) {
    EXPECT_LE(line.width, options.maxWidth + 0.01f) << "line '" << line.text << "'";
  }
}