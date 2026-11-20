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

#include "unicode/LineBreakIterator.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace {

bool hasBreak(const std::vector<coral::ui::TailoredLineBreak>& breaks, const size_t offset) {
  for (const auto& entry: breaks) {
    if (entry.byteOffset == offset) return true;
  }
  return false;
}

bool isMandatory(const std::vector<coral::ui::TailoredLineBreak>& breaks, const size_t offset) {
  for (const auto& entry: breaks) {
    if (entry.byteOffset == offset) return entry.mandatory;
  }
  return false;
}

class FixedDictionary final : public coral::ui::BreakDictionary {
 public:
  std::vector<size_t> opportunities(const std::string_view, const std::string_view) const override {
    return { 1, 3 };
  }
};

} // namespace

TEST(LineBreakPolicy, DefaultPolicyIsCssNormal) {
  const coral::ui::LineBreakPolicy policy;
  EXPECT_TRUE(policy.isDefault());
  EXPECT_EQ(policy.overflowWrap, coral::ui::OverflowWrap::Normal);

  coral::ui::LineBreakPolicy legacy;
  legacy.overflowWrap = coral::ui::OverflowWrap::BreakWord;
  EXPECT_FALSE(legacy.isDefault());
}

TEST(LineBreakPolicy, BreakAllAddsGraphemeBoundaries) {
  coral::ui::LineBreakPolicy policy;
  policy.wordBreak = coral::ui::WordBreak::BreakAll;
  const auto breaks = coral::ui::computeTailoredLineBreaks("abc", policy);
  EXPECT_TRUE(hasBreak(breaks, 1));
  EXPECT_TRUE(hasBreak(breaks, 2));
  EXPECT_FALSE(hasBreak(breaks, 0));
  EXPECT_FALSE(hasBreak(breaks, 3));
}

TEST(LineBreakPolicy, BreakAllRespectsUax14Prohibitions) {
  coral::ui::LineBreakPolicy policy;
  policy.wordBreak = coral::ui::WordBreak::BreakAll;
  // No break before a closing mark, and none after an opening mark.
  EXPECT_FALSE(hasBreak(coral::ui::computeTailoredLineBreaks("a\xE3\x80\x82" "b", policy), 1));
  EXPECT_FALSE(hasBreak(coral::ui::computeTailoredLineBreaks("(\x61", policy), 1));
  EXPECT_TRUE(hasBreak(coral::ui::computeTailoredLineBreaks("ab", policy), 1));
}

TEST(LineBreakPolicy, KeepAllRemovesHangulSyllableBreaks) {
  coral::ui::LineBreakPolicy policy;
  policy.wordBreak = coral::ui::WordBreak::KeepAll;
  const std::string hangul = "\xED\x95\x9C\xEA\xB5\xAD"; // 한국
  EXPECT_FALSE(hasBreak(coral::ui::computeTailoredLineBreaks(hangul, policy), 3));
  EXPECT_TRUE(hasBreak(coral::ui::computeTailoredLineBreaks(hangul, coral::ui::LineBreakPolicy{}), 3));
}

TEST(LineBreakPolicy, KeepAllRemovesCjkCjkBreaks) {
  coral::ui::LineBreakPolicy policy;
  policy.wordBreak = coral::ui::WordBreak::KeepAll;
  const auto breaks = coral::ui::computeTailoredLineBreaks("\xE4\xB8\xAD\xE6\x96\x87", policy);
  EXPECT_FALSE(hasBreak(breaks, 3));
}

TEST(LineBreakPolicy, LooseAllowsSmallKanaAndStrictKeepsClosers) {
  coral::ui::LineBreakPolicy loose;
  loose.strictness = coral::ui::LineBreakStrictness::Loose;
  const auto smallKana = coral::ui::computeTailoredLineBreaks("\xE3\x81\x82\xE3\x81\x81", loose);
  EXPECT_TRUE(hasBreak(smallKana, 3)) << "loose must allow a break before small kana";

  coral::ui::LineBreakPolicy strict;
  strict.strictness = coral::ui::LineBreakStrictness::Strict;
  const auto closer = coral::ui::computeTailoredLineBreaks("\xE3\x81\x82\xE3\x80\x82", strict);
  EXPECT_FALSE(hasBreak(closer, 3)) << "strict must not break before a closing mark";
}

TEST(LineBreakPolicy, ThaiDictionaryFindsWordBoundaries) {
  coral::ui::LineBreakPolicy policy;
  policy.language = "th";
  policy.dictionary = coral::ui::makeThaiBreakDictionary();
  // Thai words for language and Thai; both are in the ICU-derived list.
  const auto breaks = coral::ui::computeTailoredLineBreaks("\xE0\xB8\xA0\xE0\xB8\xB2\xE0\xB8\xA9\xE0\xB8\xB2\xE0\xB9\x84\xE0\xB8\x97\xE0\xB8\xA2", policy);
  EXPECT_TRUE(hasBreak(breaks, 12)) << "Thai dictionary must break after the first word";
}

TEST(LineBreakPolicy, KhmerDictionaryFindsWordBoundaries) {
  coral::ui::LineBreakPolicy policy;
  policy.language = "km";
  policy.dictionary = coral::ui::makeKhmerBreakDictionary();
  // Khmer words for language, and, Khmer.
  const auto breaks = coral::ui::computeTailoredLineBreaks(
    "\xE1\x9E\x97\xE1\x9E\xB6\xE1\x9E\x9F\xE1\x9E\xB6\xE1\x9E\x93\xE1\x9E\xB7\xE1\x9E\x84\xE1\x9E\x81\xE1\x9F\x92\xE1\x9E\x98\xE1\x9F\x82\xE1\x9E\x9A",
    policy);
  EXPECT_TRUE(hasBreak(breaks, 12)) << "Khmer dictionary must break after the first word";
  EXPECT_TRUE(hasBreak(breaks, 21)) << "Khmer dictionary must break after the second word";
}

TEST(LineBreakPolicy, LaoDictionaryFindsWordBoundaries) {
  coral::ui::LineBreakPolicy policy;
  policy.language = "lo";
  policy.dictionary = coral::ui::makeLaoBreakDictionary();
  // Lao words for language and Lao.
  const auto breaks = coral::ui::computeTailoredLineBreaks(
    "\xE0\xBA\x9E\xE0\xBA\xB2\xE0\xBA\xAA\xE0\xBA\xB2\xE0\xBA\xA5\xE0\xBA\xB2\xE0\xBA\xA7",
    policy);
  EXPECT_TRUE(hasBreak(breaks, 12)) << "Lao dictionary must break after the first word";
}

TEST(LineBreakPolicy, BurmeseDictionaryFindsWordBoundaries) {
  coral::ui::LineBreakPolicy policy;
  policy.language = "my";
  policy.dictionary = coral::ui::makeBurmeseBreakDictionary();
  const auto breaks = coral::ui::computeTailoredLineBreaks(
    "\xE1\x80\x80\xE1\x80\x80\xE1\x80\x90\xE1\x80\xAD\xE1\x80\xAF\xE1\x80\xB8"
    "\xE1\x80\x80\xE1\x80\x80\xE1\x80\x90\xE1\x80\x85\xE1\x80\xBA", policy);
  EXPECT_TRUE(hasBreak(breaks, 18)) << "Burmese dictionary must break after the first word";
}

TEST(LineBreakPolicy, DictionaryFallbackStaysOnGraphemeBoundaries) {
  coral::ui::LineBreakPolicy policy;
  policy.language = "th";
  policy.dictionary = coral::ui::makeThaiBreakDictionary();
  // "a" + combining acute is one grapheme; the fallback must not split it.
  const auto breaks = coral::ui::computeTailoredLineBreaks("a\xCC\x81", policy);
  EXPECT_FALSE(hasBreak(breaks, 1));

  // Unknown Latin text still gets fallback boundaries between graphemes.
  const auto latin = coral::ui::computeTailoredLineBreaks("abc", policy);
  EXPECT_TRUE(hasBreak(latin, 1));
  EXPECT_TRUE(hasBreak(latin, 2));
}

TEST(LineBreakPolicy, EmptyLanguageDisablesDictionaryBackends) {
  coral::ui::LineBreakPolicy policy;
  policy.dictionary = coral::ui::makeThaiBreakDictionary();
  const auto breaks = coral::ui::computeTailoredLineBreaks(
    "\xE0\xB8\xA0\xE0\xB8\xB2\xE0\xB8\xA9\xE0\xB8\xB2\xE0\xB9\x84\xE0\xB8\x97\xE0\xB8\xA2", policy);
  EXPECT_FALSE(hasBreak(breaks, 12)) << "an empty language must not use a Thai dictionary";
}

TEST(LineBreakPolicy, CjkStrictnessRules) {
  coral::ui::LineBreakPolicy normal;
  normal.language = "ja";
  const auto hyphen = coral::ui::computeTailoredLineBreaks("\xE3\x81\x82\xE3\x80\x9C", normal);
  EXPECT_TRUE(hasBreak(hyphen, 3)) << "normal CJK allows a break before U+301C";

  coral::ui::LineBreakPolicy strict;
  strict.language = "ja";
  strict.strictness = coral::ui::LineBreakStrictness::Strict;
  const auto strictHyphen = coral::ui::computeTailoredLineBreaks("\xE3\x81\x82\xE3\x80\x9C", strict);
  EXPECT_FALSE(hasBreak(strictHyphen, 3)) << "strict forbids a break before U+301C";

  coral::ui::LineBreakPolicy loose;
  loose.language = "ja";
  loose.strictness = coral::ui::LineBreakStrictness::Loose;
  const auto dot = coral::ui::computeTailoredLineBreaks("\xE3\x81\x82\xE3\x83\xBB", loose);
  EXPECT_TRUE(hasBreak(dot, 3)) << "loose CJK allows a break before U+30FB";
}

TEST(LineBreakPolicy, DictionaryAndMandatoryBreaksAreMerged) {
  coral::ui::LineBreakPolicy policy;
  policy.dictionary = std::make_shared<FixedDictionary>();
  const auto breaks = coral::ui::computeTailoredLineBreaks("abcdef", policy);
  EXPECT_TRUE(hasBreak(breaks, 1));
  EXPECT_TRUE(hasBreak(breaks, 3));

  coral::ui::LineBreakPolicy breakAll;
  breakAll.wordBreak = coral::ui::WordBreak::BreakAll;
  const auto newline = coral::ui::computeTailoredLineBreaks("a\nb", breakAll);
  EXPECT_TRUE(isMandatory(newline, 2));
}

TEST(LineBreakPolicy, RuleIdsAreVisibleForDiagnostics) {
  const std::vector<uint32_t> codePoints{ 'a', ' ', 'b' };
  const std::vector<uint16_t> rules = coral::ui::unicode::computeLineBreakRules(codePoints);
  ASSERT_EQ(rules.size(), codePoints.size() + 1);
  ASSERT_NE(rules[2], 0xFFFFu);
  const char* name = coral::ui::unicode::lineBreakRuleName(rules[2]);
  ASSERT_NE(name, nullptr);
  EXPECT_STRNE(name, "");
  EXPECT_STRNE(name, "unknown");
}
