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
#include <ui/RichTextLayer.h>
#include <ui/RichText.h>
#include <ui/TextInput.h>
#include <ui/TextLayer.h>
#include <ui/Theme.h>

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

coral::ui::LineBreakPolicy keepAllPolicy() {
  coral::ui::LineBreakPolicy policy;
  policy.wordBreak = coral::ui::WordBreak::KeepAll;
  policy.language = "zh";
  return policy;
}

} // namespace

TEST(UiTextPolicy, TextLayerHonoursKeepAll) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::TextLayer layer("\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95", font);
  layer.setWrapWidth(font->measureWidth("\xE4\xB8\xAD") + 1.0f);
  layer.measure({ layer.wrapWidth(), 0.0f });
  const float wrappedHeight = layer.measuredSize().height;
  ASSERT_GT(wrappedHeight, 0.0f);

  layer.setLineBreakPolicy(keepAllPolicy());
  layer.measure({ layer.wrapWidth(), 0.0f });
  EXPECT_LT(layer.measuredSize().height, wrappedHeight)
          << "keep-all must remove the CJK-CJK opportunities";
  EXPECT_FLOAT_EQ(layer.measuredSize().height, wrappedHeight / 4.0f)
          << "four ideographs collapse to one overflowing line";
}

TEST(UiTextPolicy, RichTextLayerHonoursKeepAll) {  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95";
  run.font = font;
  coral::ui::RichTextLayer layer({ run });
  layer.setWrapWidth(font->measureWidth("\xE4\xB8\xAD") + 1.0f);
  layer.measure({ layer.wrapWidth(), 0.0f });
  const float wrappedHeight = layer.measuredSize().height;
  ASSERT_GT(wrappedHeight, 0.0f);

  layer.setLineBreakPolicy(keepAllPolicy());
  layer.measure({ layer.wrapWidth(), 0.0f });
  EXPECT_LT(layer.measuredSize().height, wrappedHeight);
}

TEST(UiTextPolicy, LayoutCacheKeepsPoliciesApart) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string text = "\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95";
  coral::ui::TextLayoutOptions defaults;
  defaults.maxWidth = font->measureWidth("\xE4\xB8\xAD") + 1.0f;
  coral::ui::TextLayoutOptions keepAll = defaults;
  keepAll.policy = keepAllPolicy();

  const coral::ui::TextLayout first = coral::ui::layoutText(text, *font, defaults);
  const coral::ui::TextLayout second = coral::ui::layoutText(text, *font, keepAll);
  EXPECT_NE(first.lines.size(), second.lines.size());

  const coral::ui::TextLayout firstAgain = coral::ui::layoutText(text, *font, defaults);
  const coral::ui::TextLayout secondAgain = coral::ui::layoutText(text, *font, keepAll);
  EXPECT_EQ(firstAgain.lines.size(), first.lines.size());
  EXPECT_EQ(secondAgain.lines.size(), second.lines.size());
}

TEST(UiTextPolicy, TextInputStoresThePolicy) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::TextInput input("hello", font);
  EXPECT_EQ(input.lineBreakPolicy().overflowWrap, coral::ui::OverflowWrap::BreakWord);
  input.setLineBreakPolicy(keepAllPolicy());
  EXPECT_EQ(input.lineBreakPolicy().wordBreak, coral::ui::WordBreak::KeepAll);
  EXPECT_EQ(input.lineBreakPolicy().language, "zh");
}

TEST(UiTextPolicy, ThemePolicyAppliesUntilExplicitlyOverridden) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::Theme theme;
  theme.lineBreakPolicy = keepAllPolicy();
  auto sharedTheme = std::make_shared<coral::ui::Theme>(theme);

  coral::ui::TextLayer layer("hello world", font);
  layer.setTheme(sharedTheme);
  EXPECT_EQ(layer.lineBreakPolicy().wordBreak, coral::ui::WordBreak::KeepAll)
    << "a control with no explicit policy inherits the application default";

  layer.setLineBreakPolicy(coral::ui::LineBreakPolicy{});
  layer.setTheme(std::make_shared<coral::ui::Theme>());
  EXPECT_EQ(layer.lineBreakPolicy().wordBreak, coral::ui::WordBreak::Normal)
    << "an explicit policy must not be overwritten by a later theme";
}

TEST(UiTextPolicy, RichTextInheritsThemePolicy) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "hello world";
  run.font = font;
  coral::ui::RichTextLayer layer({ run });
  coral::ui::Theme theme;
  theme.lineBreakPolicy = keepAllPolicy();
  layer.setTheme(std::make_shared<coral::ui::Theme>(theme));
  EXPECT_EQ(layer.lineBreakPolicy().wordBreak, coral::ui::WordBreak::KeepAll);
}

TEST(UiTextPolicy, TabSizeProducesAFixedAdvanceInsteadOfAMissingGlyph) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::TextLayer layer("a\tb", font);
  layer.setTabSize(0);
  layer.measure({ 0.0f, 0.0f });
  const float withoutTabs = layer.measuredSize().width;
  layer.setTabSize(8);
  layer.measure({ 0.0f, 0.0f });
  EXPECT_GT(layer.measuredSize().width, withoutTabs + font->measureWidth(" "));
}

TEST(UiTextPolicy, RichTextTabSizeProducesAFixedAdvance) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "a\tb";
  run.font = font;
  coral::ui::RichTextLayoutOptions options;
  options.tabSize = 0;
  const coral::ui::RichTextLayout withoutTabs = coral::ui::layoutRichText({ run }, options);
  options.tabSize = 8;
  const coral::ui::RichTextLayout withTabs = coral::ui::layoutRichText({ run }, options);
  EXPECT_GT(withTabs.size.width, withoutTabs.size.width + font->measureWidth(" "));
}

TEST(UiTextPolicy, RichTextVerticalJustifyFillsTheColumn) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::RichTextRun run;
  run.text = "a b c d";
  run.font = font;
  coral::ui::RichTextLayoutOptions options;
  options.maxWidth = 30.0f;
  options.orientation = coral::ui::TextOrientation::Vertical;
  options.align = coral::ui::TextAlign::Justify;
  const coral::ui::RichTextLayout layout = coral::ui::layoutRichText({ run }, options);
  ASSERT_FALSE(layout.lines.empty());
  EXPECT_GT(layout.lines[0].justifySpacing, 0.0f)
    << "vertical justify must keep the space advance that TextLayout computed before transposing";
}
