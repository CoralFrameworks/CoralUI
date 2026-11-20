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

#include <ui/SystemFonts.h>
#include <ui/TextLayer.h>

#include <graphics/Font.h>
#include <graphics/FontFallback.h>
#include <graphics/Typeface.h>

#include <memory>
#include <string>
#include <vector>

namespace {

std::string assetFont(const char* name) {
  return std::string(CORAL_ASSETS_DIR) + "/fonts/" + name;
}

std::shared_ptr<const graphics::Font> testFont(const char* name, const float size) {
  const auto typeface = graphics::Typeface::fromFile(assetFont(name));
  return typeface != nullptr ? graphics::Font::create(typeface, size) : nullptr;
}

} // namespace

TEST(UiSystemFonts, CachedFallbackSplitsEmojiToTheSecondaryFace) {
  const auto primary = testFont("NotoSansSC-Regular.otf", 16.0f);
  if (primary == nullptr) GTEST_SKIP() << "the UI test font is not available";
  if (primary->typeface()->hasGlyphForCodePoint(0x1F600u)) {
    GTEST_SKIP() << "the primary face already covers emoji";
  }
  const std::string emojiPath = assetFont("NotoColorEmoji.ttf");
  coral::ui::setSystemFontFilesForTesting({ emojiPath });

  const auto fallback = coral::ui::systemFontFallback(primary);
  ASSERT_NE(fallback, nullptr);
  EXPECT_EQ(fallback, coral::ui::systemFontFallback(primary)) << "the fallback chain is cached";

  const std::string text = "A\xF0\x9F\x98\x80";
  const std::vector<graphics::FontRun> runs = fallback->shape(text);
  ASSERT_EQ(runs.size(), 2u);
  EXPECT_EQ(runs[0].font->typeface(), primary->typeface());
  EXPECT_NE(runs[1].font->typeface(), primary->typeface());
  EXPECT_FLOAT_EQ(runs[1].font->size(), primary->size());
  EXPECT_GT(fallback->measure(text), primary->measureWidth("A"));

  coral::ui::setSystemFontFilesForTesting({});
}

TEST(UiSystemFonts, TextLayerOptsIntoSystemFallback) {
  const auto font = testFont("NotoSansSC-Regular.otf", 16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  if (font->typeface()->hasGlyphForCodePoint(0x1F600u)) {
    GTEST_SKIP() << "the primary face already covers emoji";
  }
  coral::ui::setSystemFontFilesForTesting({ assetFont("NotoColorEmoji.ttf") });

  coral::ui::TextLayer label("\xF0\x9F\x98\x80", font);
  EXPECT_FALSE(label.usesSystemFontFallback());
  EXPECT_EQ(label.fontFallback(), nullptr);
  label.setSystemFontFallback(true);
  EXPECT_TRUE(label.usesSystemFontFallback());
  ASSERT_NE(label.fontFallback(), nullptr);
  label.setBounds(coral::ui::Rect::MakeWH(64.0f, 32.0f));
  label.measure({ 64.0f, 32.0f });
  EXPECT_GT(label.measuredSize().width, 0.0f);

  coral::ui::setSystemFontFilesForTesting({});
}

TEST(UiSystemFonts, DiscoveryReturnsCuratedFilesOrNothing) {
  // Discovery is platform dependent; the contract is that the list is usable
  // (all paths exist) even when a minimal CI image has no curated faces.
  const std::vector<std::string> files = coral::ui::systemFontFiles();
  for (const std::string& path: files) {
    EXPECT_FALSE(path.empty());
  }
}
