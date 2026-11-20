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
#include <ui/TextLayout.h>
#include <ui/RenderContext.h>
#include <ui/CanvasBackend.h>
#include <ui/TextRendererStats.h>

#include "TextPainter.h"
#include "unicode/GraphemeBreak.h"

#include <graphics/Bitmap.h>
#include <graphics/Canvas.h>
#include <graphics/Font.h>
#include <graphics/Paint.h>
#include <graphics/Typeface.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

std::shared_ptr<const graphics::Font> testFont(const float size) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  return typeface != nullptr ? graphics::Font::create(typeface, size) : nullptr;
}

bool isGraphemeBoundary(const std::string& text, const size_t offset) {
  const std::vector<size_t> boundaries = coral::ui::unicode::computeGraphemeBreaks(text);
  return std::find(boundaries.begin(), boundaries.end(), offset) != boundaries.end();
}

/**
 * Gives one test its own run-cache budget and drops the entries on both ends.
 *
 * The budget is process-wide, and the cache holds rasterised line bitmaps that other tests would
 * otherwise inherit; a test that left either behind would make the next one depend on execution order.
 */
class RunCacheScope {
 public:
  explicit RunCacheScope(const size_t budget)
      : mPrevious(coral::ui::textRunCacheBudgetBytes()) {
    coral::ui::setTextRunCacheBudgetBytes(budget);
    coral::ui::clearTextRunCache();
  }
  ~RunCacheScope() {
    coral::ui::setTextRunCacheBudgetBytes(mPrevious);
    coral::ui::clearTextRunCache();
  }
  RunCacheScope(const RunCacheScope&) = delete;
  RunCacheScope& operator=(const RunCacheScope&) = delete;

 private:
  size_t mPrevious;
};

} // namespace

TEST(UiTextVisual, SubpixelTextAlsoWorksOnTheRasterCanvas) {
  const auto font = testFont(14.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const auto typeface = font->typeface();
  const uint32_t glyph = font->glyphForCodePoint(U'A');
  if (typeface == nullptr || glyph == 0 || !typeface->glyphBitmap(glyph, font->size(), true).subpixel) {
    GTEST_SKIP() << "this platform has no subpixel glyph rasteriser";
  }

  graphics::Bitmap target = graphics::Bitmap::allocate(160, 48);
  ASSERT_FALSE(target.isEmpty());
  auto canvas = graphics::Canvas::create(target);
  ASSERT_NE(canvas, nullptr);
  graphics::Paint background;
  background.color = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  canvas->drawRect(coral::ui::Rect::MakeWH(160.0f, 48.0f), background);

  coral::ui::Canvas uiCanvas = coral::ui::detail::makeCanvas(*canvas);

  coral::ui::RenderContext context(uiCanvas, 1.0f);
  coral::ui::TextPainter painter;
  graphics::Paint paint;
  // A non-white colour proves the per-channel strips are baked with the paint's colour instead of
  // being drawn as a white mask.
  paint.color = graphics::Color::MakeRGBA8(255, 32, 32, 255);
  painter.draw(context, "AVA", 0.0f, 32.0f, *font, paint);

  const graphics::Pixmap pixels = target.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  int greatestChannelDelta = 0;
  int greatestRed = 0;
  int greatestGreen = 0;
  int greatestBlue = 0;
  for (int y = 0; y < target.height(); ++y) {
    for (int x = 0; x < target.width(); ++x) {
      const uint32_t argb = pixels.getPixelARGB32(x, y);
      const int red = static_cast<int>((argb >> 16) & 0xFFu);
      const int green = static_cast<int>((argb >> 8) & 0xFFu);
      const int blue = static_cast<int>(argb & 0xFFu);
      greatestRed = red > greatestRed ? red : greatestRed;
      greatestGreen = green > greatestGreen ? green : greatestGreen;
      greatestBlue = blue > greatestBlue ? blue : greatestBlue;
      int high = red;
      if (green > high) high = green;
      if (blue > high) high = blue;
      int low = red;
      if (green < low) low = green;
      if (blue < low) low = blue;
      if (high - low > greatestChannelDelta) greatestChannelDelta = high - low;
    }
  }
  EXPECT_GT(greatestChannelDelta, 8)
    << "the raster backend must apply the per-channel write mask, not a grayscale mask";
  EXPECT_GT(greatestRed, greatestGreen + 8) << "the subpixel mask must use the paint colour";
  EXPECT_GT(greatestRed, greatestBlue + 8) << "the subpixel mask must use the paint colour";
}

TEST(UiTextVisual, LinearLightLayerReadsDestination) {
  const auto font = testFont(14.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const auto typeface = font->typeface();
  const uint32_t glyph = font->glyphForCodePoint(U'A');
  if (typeface == nullptr || glyph == 0 || !typeface->glyphBitmap(glyph, font->size(), true).subpixel) {
    GTEST_SKIP() << "this platform has no subpixel glyph rasteriser";
  }

  graphics::Bitmap target = graphics::Bitmap::allocate(200, 48);
  ASSERT_FALSE(target.isEmpty());
  auto canvas = graphics::Canvas::create(target);
  ASSERT_NE(canvas, nullptr);
  graphics::Paint background;
  background.color = graphics::Color::White();
  canvas->drawRect(coral::ui::Rect::MakeWH(200.0f, 48.0f), background);

  coral::ui::Canvas uiCanvas = coral::ui::detail::makeCanvas(*canvas);

  coral::ui::RenderContext context(uiCanvas, 1.0f);
  coral::ui::TextLayer layer("AVA", font, graphics::Color::Black());
  layer.setBounds(coral::ui::Rect::MakeWH(200.0f, 48.0f));
  layer.setLinearLightLayer(true);
  layer.draw(context);

  const graphics::Pixmap pixels = target.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  int darkPixels = 0;
  for (int y = 0; y < target.height(); ++y) {
    for (int x = 0; x < target.width(); ++x) {
      const uint32_t argb = pixels.getPixelARGB32(x, y);
      const int luminance = static_cast<int>((argb >> 16) & 0xFFu) +
                            static_cast<int>((argb >> 8) & 0xFFu) +
                            static_cast<int>(argb & 0xFFu);
      if (luminance < 600) ++darkPixels;
    }
  }
  EXPECT_GT(darkPixels, 10) << "the linear-light layer must composite text over the destination it reads";
}

TEST(UiTextVisual, LinearLightLayerSurvivesTransparentDestination) {
  const auto font = testFont(14.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const auto typeface = font->typeface();
  const uint32_t glyph = font->glyphForCodePoint(U'A');
  if (typeface == nullptr || glyph == 0 || !typeface->glyphBitmap(glyph, font->size(), true).subpixel) {
    GTEST_SKIP() << "this platform has no subpixel glyph rasteriser";
  }

  graphics::Bitmap target = graphics::Bitmap::allocate(200, 48);
  ASSERT_FALSE(target.isEmpty());
  auto canvas = graphics::Canvas::create(target);
  ASSERT_NE(canvas, nullptr);
  coral::ui::Canvas uiCanvas = coral::ui::detail::makeCanvas(*canvas);
  coral::ui::RenderContext context(uiCanvas, 1.0f);
  coral::ui::TextLayer layer("AVA", font, graphics::Color::White());
  layer.setBounds(coral::ui::Rect::MakeWH(200.0f, 48.0f));
  layer.setLinearLightLayer(true);
  layer.draw(context);

  const graphics::Pixmap pixels = target.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  int covered = 0;
  for (int y = 0; y < target.height(); ++y) {
    for (int x = 0; x < target.width(); ++x) {
      const uint32_t argb = pixels.getPixelARGB32(x, y);
      if (((argb >> 24) & 0xFFu) > 16) ++covered;
    }
  }
  EXPECT_GT(covered, 10) << "text must contribute alpha on a transparent destination";
}

TEST(UiTextVisual, EllipsisNeverSplitsAGraphemeCluster) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string family = "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D"
                              "\xF0\x9F\x91\xA7\xE2\x80\x8D\xF0\x9F\x91\xA6";
  const std::string text = family + " tail";
  coral::ui::TextLayoutOptions options;
  options.maxWidth = font->measureWidth(family) + 1.0f;
  options.maxLines = 1;
  options.ellipsis = true;
  options.policy.overflowWrap = coral::ui::OverflowWrap::BreakWord;
  const coral::ui::TextLayout layout = coral::ui::layoutText(text, *font, options);
  ASSERT_EQ(layout.lines.size(), 1u);
  EXPECT_TRUE(layout.truncated);
  EXPECT_EQ(layout.lines.front().text, "\xE2\x80\xA6")
          << "the whole emoji cluster is wider than the budget, so only the ellipsis remains";
}

TEST(UiTextVisual, EllipsisPrefixEndsOnAGraphemeBoundary) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string text = "e\xCC\x81" "e\xCC\x81" "e\xCC\x81" "e\xCC\x81" "e\xCC\x81";
  coral::ui::TextLayoutOptions options;
  options.maxWidth = font->measureWidth("e\xCC\x81" "e\xCC\x81") + 1.0f;
  options.maxLines = 1;
  options.ellipsis = true;
  options.policy.overflowWrap = coral::ui::OverflowWrap::BreakWord;
  const coral::ui::TextLayout layout = coral::ui::layoutText(text, *font, options);
  ASSERT_EQ(layout.lines.size(), 1u);
  ASSERT_TRUE(layout.truncated);
  const std::string_view rendered = layout.lines.front().text;
  const size_t ellipsisBytes = std::string("\xE2\x80\xA6").size();
  ASSERT_GE(rendered.size(), ellipsisBytes);
  EXPECT_EQ(rendered.substr(rendered.size() - ellipsisBytes), "\xE2\x80\xA6");
  const size_t prefix = rendered.size() - ellipsisBytes;
  EXPECT_TRUE(isGraphemeBoundary(text, prefix))
          << "prefix ends inside a base + combining-mark cluster";
}

TEST(UiTextVisual, EllipsisDropsStaleVisualRuns) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string text = "\xD7\x90\xD7\x91\xD7\x92\xD7\x93\xD7\x94\xD7\x95\xD7\x96\xD7\x97";
  coral::ui::TextLayoutOptions options;
  options.maxWidth = font->measureWidth("\xD7\x90\xD7\x91") + 1.0f;
  options.maxLines = 1;
  options.ellipsis = true;
  options.policy.overflowWrap = coral::ui::OverflowWrap::BreakWord;
  const coral::ui::TextLayout layout = coral::ui::layoutText(text, *font, options);
  ASSERT_EQ(layout.lines.size(), 1u);
  ASSERT_TRUE(layout.truncated);
  EXPECT_TRUE(layout.lines.front().visualRuns.empty())
          << "synthesized ellipsis text has no source mapping for visual runs";
  EXPECT_NE(layout.lines.front().text.find("\xE2\x80\xA6"), std::string::npos);
}

TEST(UiTextVisual, HitTestingUsesVisualRunOrder) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string text = "abc \xD7\x90\xD7\x91\xD7\x92";
  coral::ui::TextLayer layer(text, font);
  layer.setBounds(coral::ui::Rect::MakeWH(200.0f, 40.0f));
  layer.measure({ 200.0f, 40.0f });
  const coral::ui::TextLine line = layer.layout().lines[0];
  ASSERT_EQ(line.text, text);
  ASSERT_EQ(line.visualRuns.size(), 2u);

  const float latinWidth = font->measureWidth("abc ");
  const float hebrewWidth = font->measureWidth("\xD7\x90\xD7\x91\xD7\x92");
  const size_t hebrewStart = 4;
  const size_t hebrewEnd = text.size();
  // Just inside the Hebrew run's left edge is its logical end in a RTL run.
  EXPECT_EQ(layer.indexAt({ latinWidth + 1.0f, 4.0f }), hebrewEnd);
  // Just inside its right edge is its logical start.
  EXPECT_EQ(layer.indexAt({ latinWidth + hebrewWidth - 1.0f, 4.0f }), hebrewStart);
}

TEST(UiTextVisual, SelectionSpansReorderedRunsWithoutCrashing) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string text = "abc \xD7\x90\xD7\x91\xD7\x92";
  coral::ui::TextLayer layer(text, font);
  layer.setBounds(coral::ui::Rect::MakeWH(200.0f, 40.0f));
  layer.setSelectable(true);
  layer.measure({ 200.0f, 40.0f });
  layer.setSelection(2, 9);
  EXPECT_TRUE(layer.hasSelection());
  // Draw goes through the visual-run span path; a headless window is covered by
  // the pixel tests elsewhere, so this asserts the layout state used by drawing.
  const coral::ui::TextLine line = layer.layout().lines[0];
  EXPECT_EQ(line.visualRuns.size(), 2u);
}

TEST(UiTextVisual, TextRunCacheReusesARunAcrossPainters) {
  const auto font = testFont(14.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";

  const RunCacheScope scope(64u * 1024u * 1024u);
  graphics::Bitmap target = graphics::Bitmap::allocate(240, 48);
  ASSERT_FALSE(target.isEmpty());
  auto canvas = graphics::Canvas::create(target);
  ASSERT_NE(canvas, nullptr);
  coral::ui::Canvas uiCanvas = coral::ui::detail::makeCanvas(*canvas);
  coral::ui::RenderContext context(uiCanvas, 1.0f);
  graphics::Paint paint;
  coral::ui::resetTextRendererStats();

  {
    coral::ui::TextPainter first;
    first.draw(context, "Run cache reuse", 0.0f, 32.0f, *font, paint);
  }
  const size_t builtOnce = coral::ui::textRendererStats().runBuilds;
  ASSERT_EQ(builtOnce, 1u) << "the first painter has to build its run";

  {
    // A second painter has no local run, so it can only hit the shared cache. This is the path a
    // scrolled list takes on every frame: new painters, same lines.
    coral::ui::TextPainter second;
    second.draw(context, "Run cache reuse", 0.0f, 32.0f, *font, paint);
  }
  const coral::ui::TextRendererStats stats = coral::ui::textRendererStats();
  EXPECT_EQ(stats.runBuilds, builtOnce) << "the shared cache must serve the second painter";
  EXPECT_GE(stats.runCacheHits, 1u);
  EXPECT_GT(stats.runCacheBytes, 0u);
  EXPECT_LE(stats.runCacheBytes, coral::ui::textRunCacheBudgetBytes());
}

TEST(UiTextVisual, TextRunCacheEvictsByBytesInsteadOfClearing) {
  const auto font = testFont(14.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";

  // A budget that holds a handful of these lines, so the second pass evicts the early ones. It also
  // has to exceed one run: a run larger than the whole budget is deliberately left uncached.
  const size_t budget = 256u * 1024u;
  const RunCacheScope scope(budget);
  graphics::Bitmap target = graphics::Bitmap::allocate(400, 48);
  ASSERT_FALSE(target.isEmpty());
  auto canvas = graphics::Canvas::create(target);
  ASSERT_NE(canvas, nullptr);
  coral::ui::Canvas uiCanvas = coral::ui::detail::makeCanvas(*canvas);
  coral::ui::RenderContext context(uiCanvas, 1.0f);
  graphics::Paint paint;
  coral::ui::resetTextRendererStats();

  std::vector<std::string> lines;
  for (int index = 0; index < 24; ++index) {
    lines.push_back("evicted line " + std::to_string(index) + " with some width");
  }
  const auto drawAll = [&]() {
    for (const std::string& line: lines) {
      coral::ui::TextPainter painter;
      painter.draw(context, line, 0.0f, 32.0f, *font, paint);
    }
  };
  drawAll();
  const size_t afterFirstPass = coral::ui::textRendererStats().runBuilds;
  EXPECT_EQ(afterFirstPass, lines.size()) << "each distinct line builds once";
  drawAll();

  const coral::ui::TextRendererStats stats = coral::ui::textRendererStats();
  EXPECT_GT(stats.runCacheEvictions, 0u) << "24 lines must not fit a budget that holds a few";
  EXPECT_LE(stats.runCacheBytes, budget) << "the budget is a byte ceiling, not a suggestion";
  EXPECT_GT(stats.runBuilds, afterFirstPass)
    << "a second pass over more text than the budget must rebuild the evicted lines";
}
