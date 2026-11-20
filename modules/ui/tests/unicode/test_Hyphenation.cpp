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

#include <ui/LineBreakPolicy.h>
#include <ui/TextLayout.h>

#include "unicode/Hyphenation.h"

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

bool hasHyphenatedBreak(const std::vector<coral::ui::TailoredLineBreak>& breaks, const size_t offset) {
  for (const coral::ui::TailoredLineBreak& entry: breaks) {
    if (entry.byteOffset == offset) return entry.hyphenate;
  }
  return false;
}

} // namespace

TEST(UnicodeHyphenation, LiangPatternsFindEnglishBreaks) {
  using coral::ui::unicode::hyphenateWord;
  std::vector<size_t> positions;

  ASSERT_TRUE(hyphenateWord("hyphenation", "en", positions));
  EXPECT_EQ(positions, (std::vector<size_t>{ 2, 6 }));

  ASSERT_TRUE(hyphenateWord("computer", "en-US", positions));
  EXPECT_EQ(positions, (std::vector<size_t>{ 3 }));

  ASSERT_TRUE(hyphenateWord("table", "en", positions)) << "table is a TeX exception";
  EXPECT_EQ(positions, (std::vector<size_t>{ 2 }));

  EXPECT_FALSE(hyphenateWord("hyphenation", "xx", positions));
  EXPECT_TRUE(positions.empty());
}

TEST(UnicodeHyphenation, DutchSwedishAndDanishTablesAreAvailable) {
  using coral::ui::unicode::hasHyphenation;
  using coral::ui::unicode::hyphenateWord;
  EXPECT_TRUE(hasHyphenation("nl"));
  EXPECT_TRUE(hasHyphenation("sv"));
  EXPECT_TRUE(hasHyphenation("da"));

  std::vector<size_t> positions;
  ASSERT_TRUE(hyphenateWord("lettergrepen", "nl", positions));
  EXPECT_FALSE(positions.empty()) << "Dutch patterns should split a compound-length word";
  ASSERT_TRUE(hyphenateWord("avstavning", "sv", positions));
  EXPECT_FALSE(positions.empty()) << "Swedish patterns should split a compound-length word";
  ASSERT_TRUE(hyphenateWord("orddeling", "da", positions));
  EXPECT_FALSE(positions.empty()) << "Danish patterns should split a compound-length word";
}

TEST(UnicodeHyphenation, FinnishAndCatalanTablesAreAvailable) {
  using coral::ui::unicode::hasHyphenation;
  using coral::ui::unicode::hyphenateWord;
  EXPECT_TRUE(hasHyphenation("fi"));
  EXPECT_TRUE(hasHyphenation("ca"));

  std::vector<size_t> positions;
  ASSERT_TRUE(hyphenateWord("tietokone", "fi", positions));
  EXPECT_FALSE(positions.empty()) << "Finnish patterns should split a compound-length word";
  ASSERT_TRUE(hyphenateWord("informacio", "ca", positions));
  EXPECT_FALSE(positions.empty()) << "Catalan patterns should split a long word";
}

TEST(UnicodeHyphenation, ItalianAndSpanishTablesAreAvailable) {  using coral::ui::unicode::hasHyphenation;
  using coral::ui::unicode::hyphenateWord;
  EXPECT_TRUE(hasHyphenation("it"));
  EXPECT_TRUE(hasHyphenation("it-IT"));
  EXPECT_TRUE(hasHyphenation("es"));
  EXPECT_TRUE(hasHyphenation("es_ES"));
  EXPECT_TRUE(hasHyphenation("de"));

  std::vector<size_t> positions;
  ASSERT_TRUE(hyphenateWord("informazione", "it", positions));
  EXPECT_EQ(positions, (std::vector<size_t>{ 2, 5, 7, 10 })) << "in-for-ma-zio-ne";

  ASSERT_TRUE(hyphenateWord("computadora", "es", positions));
  EXPECT_EQ(positions, (std::vector<size_t>{ 5, 9 })) << "com-pu-ta-do-ra pattern subset";

  ASSERT_TRUE(hyphenateWord("hyphenation", "it", positions));
  EXPECT_FALSE(positions.empty()) << "a language switch must not fall back to English";
}

TEST(UnicodeHyphenation, AccentedWordsAreFoldedToBaseLetters) {
  using coral::ui::unicode::hasHyphenation;
  using coral::ui::unicode::hyphenateWord;
  EXPECT_TRUE(hasHyphenation("fr"));
  EXPECT_TRUE(hasHyphenation("pt"));
  std::vector<size_t> positions;

  ASSERT_TRUE(hyphenateWord("informaci\xC3\xB3n", "es", positions));
  EXPECT_EQ(positions, (std::vector<size_t>{ 2, 5, 7 })) << "in-for-ma-ción";

  ASSERT_TRUE(hyphenateWord("d\xC3\xA9veloppement", "fr", positions));
  EXPECT_EQ(positions, (std::vector<size_t>{ 3, 5, 8 })) << "dé-ve-lo-ppement (byte offsets)";

  ASSERT_TRUE(hyphenateWord("computa\xC3\xA7\xC3\xA3o", "pt", positions));
  EXPECT_EQ(positions, (std::vector<size_t>{ 3, 5, 7 })) << "com-pu-ta-ção";
}

TEST(UnicodeHyphenation, AccentedPolicyAddsHyphenatedOpportunities) {
  coral::ui::LineBreakPolicy policy;
  policy.language = "es";
  policy.hyphenation = true;
  const auto breaks = coral::ui::computeTailoredLineBreaks("informaci\xC3\xB3n", policy);
  EXPECT_TRUE(hasHyphenatedBreak(breaks, 2));
  EXPECT_TRUE(hasHyphenatedBreak(breaks, 5));
  EXPECT_TRUE(hasHyphenatedBreak(breaks, 7));
}

TEST(UnicodeHyphenation, GermanPatternsAndSharpS) {
  using coral::ui::unicode::hasHyphenation;
  using coral::ui::unicode::hyphenateWord;
  EXPECT_TRUE(hasHyphenation("de"));
  EXPECT_TRUE(hasHyphenation("de-DE"));
  std::vector<size_t> positions;
  ASSERT_TRUE(hyphenateWord("Silbentrennung", "de", positions));
  EXPECT_EQ(positions, (std::vector<size_t>{ 3, 6, 10 })) << "Sil-ben-tren-nung";

  ASSERT_TRUE(hyphenateWord("Stra\xC3\x9F" "e", "de", positions));
  EXPECT_EQ(positions, (std::vector<size_t>{ 4 })) << "Stra-ße (ß folds to s)";
}

TEST(UnicodeHyphenation, TexExceptionsOverridePatterns) {
  using coral::ui::unicode::hyphenateWord;
  std::vector<size_t> positions;
  ASSERT_TRUE(hyphenateWord("associate", "en", positions));
  EXPECT_EQ(positions, (std::vector<size_t>{ 2, 4 }));
  ASSERT_TRUE(hyphenateWord("present", "en", positions));
  EXPECT_TRUE(positions.empty()) << "a no-hyphen exception suppresses pattern breaks";
  ASSERT_TRUE(hyphenateWord("hardware", "pt", positions));
  EXPECT_EQ(positions, (std::vector<size_t>{ 4 }));
}

TEST(UnicodeHyphenation, LimitCharsOverridesLanguageMinima) {
  using coral::ui::unicode::HyphenationLimits;
  using coral::ui::unicode::hyphenateWord;
  std::vector<size_t> positions;
  HyphenationLimits limits;
  limits.minWordLength = 20;
  ASSERT_TRUE(hyphenateWord("hyphenation", "en", positions, limits));
  EXPECT_TRUE(positions.empty()) << "word shorter than the configured minimum";

  limits = {};
  limits.minBefore = 4;
  ASSERT_TRUE(hyphenateWord("hyphenation", "en", positions, limits));
  EXPECT_EQ(positions, (std::vector<size_t>{ 6 })) << "the break after 2 chars is filtered";

  limits = {};
  limits.minAfter = 6;
  ASSERT_TRUE(hyphenateWord("hyphenation", "en", positions, limits));
  EXPECT_EQ(positions, (std::vector<size_t>{ 2 })) << "the break with only 5 chars left is filtered";
}

TEST(UnicodeHyphenation, PolicyLimitCharsAffectsOpportunities) {
  coral::ui::LineBreakPolicy policy;
  policy.language = "en";
  policy.hyphenation = true;
  policy.hyphenationMinBefore = 4;
  const auto breaks = coral::ui::computeTailoredLineBreaks("hyphenation", policy);
  EXPECT_FALSE(hasHyphenatedBreak(breaks, 2));
  EXPECT_TRUE(hasHyphenatedBreak(breaks, 6));
}

TEST(UnicodeHyphenation, RussianPatternsUseTheCyrillicFold) {
  using coral::ui::unicode::hasHyphenation;
  using coral::ui::unicode::hyphenateWord;
  EXPECT_TRUE(hasHyphenation("ru"));
  std::vector<size_t> positions;
  ASSERT_TRUE(hyphenateWord("\xD0\xBC\xD0\xBE\xD0\xBB\xD0\xBE\xD0\xBA\xD0\xBE", "ru", positions));
  EXPECT_EQ(positions, (std::vector<size_t>{ 4, 8 })) << "мо-ло-ко (byte offsets)";
}

TEST(UnicodeHyphenation, ZoneAndLineLimitsControlHyphenBreaks) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string word = "hyphenation";
  const float width = font->measureWidth("hyphen") + font->measureWidth("\xE2\x80\x90") + 2.0f;
  coral::ui::TextLayoutOptions options;
  options.maxWidth = width;
  options.policy.language = "en";
  options.policy.hyphenation = true;

  const coral::ui::TextLayout allowed = coral::ui::layoutText(word, *font, options);
  ASSERT_GE(allowed.lines.size(), 2u);
  EXPECT_TRUE(allowed.lines.front().hyphenated);

  options.policy.hyphenateLimitZone = 1.0f;
  const coral::ui::TextLayout zoneRejected = coral::ui::layoutText(word, *font, options);
  EXPECT_EQ(zoneRejected.lines.size(), 1u) << "the slack is larger than the zone, so the word is pushed";
  EXPECT_FALSE(zoneRejected.lines.front().hyphenated);

  options.policy.hyphenateLimitZone = 0.0f;
  options.policy.hyphenateLimitLines = 1;
  const coral::ui::TextLayout limited = coral::ui::layoutText(word + " " + word, *font, options);
  size_t run = 0;
  for (const coral::ui::TextLine& line: limited.lines) {
    run = line.hyphenated ? run + 1 : 0;
    EXPECT_LE(run, 1u) << "hyphenate-limit-lines must cap consecutive hyphenated lines";
  }
}

TEST(UnicodeHyphenation, GreekPatternsUseTheGreekFold) {
  using coral::ui::unicode::hasHyphenation;
  using coral::ui::unicode::hyphenateWord;
  EXPECT_TRUE(hasHyphenation("el"));
  std::vector<size_t> positions;
  ASSERT_TRUE(hyphenateWord("\xCE\xBA\xCE\xB1\xCE\xBB\xCE\xB7\xCE\xBC\xCE\xAD\xCF\x81\xCE\xB1", "el", positions));
  EXPECT_EQ(positions, (std::vector<size_t>{ 4, 8, 12 })) << "κα-λη-μέ-ρα (byte offsets)";
}

TEST(UnicodeHyphenation, HyphenMinimaAndNonWordsAreRejected) {
  using coral::ui::unicode::hyphenateWord;
  std::vector<size_t> positions;
  EXPECT_TRUE(hyphenateWord("aaaa", "en", positions));
  EXPECT_TRUE(positions.empty()) << "shorter than the 2+3 minimum";
  EXPECT_FALSE(hyphenateWord("hy-phen", "en", positions)) << "non-letters are not hyphenated";
  EXPECT_FALSE(hyphenateWord(std::string(80, 'a'), "en", positions));
}

TEST(UnicodeHyphenation, PolicyAddsHyphenatedOpportunities) {
  coral::ui::LineBreakPolicy policy;
  policy.language = "en";
  policy.hyphenation = true;
  const std::string text = "hyphenation";
  const auto breaks = coral::ui::computeTailoredLineBreaks(text, policy);
  EXPECT_TRUE(hasHyphenatedBreak(breaks, 2));
  EXPECT_TRUE(hasHyphenatedBreak(breaks, 6));
  EXPECT_FALSE(hasHyphenatedBreak(breaks, 3));
}

TEST(UnicodeHyphenation, LayoutMarksTheHyphenatedLine) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::TextLayoutOptions options;
  options.policy.language = "en";
  options.policy.hyphenation = true;
  options.maxWidth = font->measureWidth("hyphen") + font->measureWidth("\xE2\x80\x90") + 1.0f;
  const coral::ui::TextLayout layout = coral::ui::layoutText("hyphenation", *font, options);
  ASSERT_GE(layout.lines.size(), 2u);
  EXPECT_TRUE(layout.lines[0].hyphenated);
  EXPECT_EQ(layout.lines[0].text, "hyphen");
  EXPECT_FALSE(layout.lines.back().hyphenated);
}

TEST(UnicodeHyphenation, BrokenSoftHyphenIsTrimmedAndMarked) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string text = "abc\xC2\xAD""def";
  coral::ui::TextLayoutOptions options;
  options.maxWidth = font->measureWidth("abc") + font->measureWidth("\xE2\x80\x90") + 1.0f;
  const coral::ui::TextLayout layout = coral::ui::layoutText(text, *font, options);
  ASSERT_EQ(layout.lines.size(), 2u);
  EXPECT_TRUE(layout.lines[0].hyphenated);
  EXPECT_EQ(layout.lines[0].text, "abc");
  EXPECT_EQ(layout.lines[1].text, "def");
}
