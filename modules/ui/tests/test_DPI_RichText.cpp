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

#include <ui/ColorLayer.h>
#include <ui/RenderContext.h>
#include <ui/RichText.h>
#include <ui/RichTextLayer.h>
#include <ui/TextLayer.h>
#include <ui/TextRendererStats.h>
#include <ui/Window.h>
#include <ui/CanvasBackend.h>

#include <graphics/Typeface.h>

#include <gtest/gtest.h>

namespace {

using namespace coral::ui;

std::shared_ptr<const graphics::Font> testFont(const float size) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  return typeface != nullptr ? graphics::Font::create(typeface, size) : nullptr;
}

bool fontHasSubpixelText(const std::shared_ptr<const graphics::Font>& font) {
  if (font == nullptr || font->typeface() == nullptr) return false;
  const uint32_t glyph = font->glyphForCodePoint(U'A');
  return glyph != 0 && font->typeface()->glyphBitmap(glyph, font->size(), true).subpixel;
}

class FocusButton final : public Layer {
 public:
  bool focusable() const override {
    return true;
  }
  bool onPointerDown(const PointerEvent& event) override {
    (void)event;
    return true;
  }
  void onFocusChanged(const bool value) override {
    focused = value;
  }
  bool focused = false;
};

class RawTextLayer final : public Layer {
 public:
  RawTextLayer(std::shared_ptr<const graphics::Font> font) : mFont(std::move(font)) {}

  void drawSelf(RenderContext& context) override {
    graphics::Paint paint;
    paint.color = graphics::Color::MakeRGBA8(255, 255, 255, 255);
    paint.style = graphics::Paint::Style::kFill;
    context.drawText("Raw layer", 4.0f, 24.0f, *mFont, paint);
  }

 private:
  std::shared_ptr<const graphics::Font> mFont;
};

TEST(UiDpi, ALogicalRootIsScaledIntoThePhysicalWindow) {
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 64;
  options.uiScale = 2.0f;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  EXPECT_FLOAT_EQ(window->uiScale(), 2.0f);
  EXPECT_FLOAT_EQ(window->devicePixelRatio(), 2.0f);
  EXPECT_EQ(window->nativePlatform(), Window::NativePlatform::Auto);

  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  window->setRoot(root);
  EXPECT_FLOAT_EQ(root->bounds().getWidth(), 32.0f);
  EXPECT_FLOAT_EQ(root->bounds().getHeight(), 32.0f);
  ASSERT_EQ(window->run(), 1);
}

TEST(UiDpi, IdenticalTextLinesShareOneRunBitmap) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  resetTextRendererStats();
  auto bitmap = graphics::Bitmap::allocate(240, 120);
  ASSERT_FALSE(bitmap.isEmpty());
  auto canvas = graphics::Canvas::create(bitmap);
  ASSERT_NE(canvas, nullptr);
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(240.0f, 120.0f));
  auto first = std::make_shared<TextLayer>("shared label", font, graphics::Color::White());
  first->setBounds(Rect::MakeXYWH(10.0f, 10.0f, 200.0f, 24.0f));
  auto second = std::make_shared<TextLayer>("shared label", font, graphics::Color::White());
  second->setBounds(Rect::MakeXYWH(10.0f, 44.0f, 200.0f, 24.0f));
  root->addChild(first);
  root->addChild(second);
  Canvas uiCanvas = detail::makeCanvas(*canvas);
  RenderContext context(uiCanvas, 1.0f);
  root->draw(context);
  const TextRendererStats stats = textRendererStats();
  EXPECT_EQ(stats.runBuilds, 1u);
  EXPECT_GE(stats.runCacheHits, 1u);
  // Subpixel text draws one channel-masked image per channel; the grayscale fallback draws one.
  EXPECT_EQ(stats.runDrawCalls, fontHasSubpixelText(font) ? 6u : 2u);
  EXPECT_GT(stats.bitmapBytes, 0u);
}

TEST(UiDpi, GpuTextUsesTheSharedTextPath) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  resetTextRendererStats();
  Window::Options options;
  options.headless = true;
  options.width = 240;
  options.height = 120;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<Layer>();
  auto label = std::make_shared<TextLayer>("atlas", font, graphics::Color::White());
  label->setBounds(Rect::MakeXYWH(10.0f, 10.0f, 200.0f, 24.0f));
  root->addChild(label);
  window->setRoot(root);
  ASSERT_EQ(window->run(), 1);
  const TextRendererStats stats = textRendererStats();
  if (fontHasSubpixelText(font)) {
    // The platform ClearType path draws per-channel run masks instead of the glyph atlas.
    EXPECT_GT(stats.runDrawCalls, 0u);
    EXPECT_EQ(stats.atlasGlyphs, 0u);
  } else {
    EXPECT_GT(stats.atlasGlyphs, 0u);
    EXPECT_EQ(stats.runDrawCalls, 0u);
  }
}

TEST(UiDpi, PointerPositionsAreConvertedToLogicalUnits) {
  Window::Options options;
  options.headless = true;
  options.width = 100;
  options.height = 100;
  options.uiScale = 2.0f;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto root = std::make_shared<Layer>();
  auto button = std::make_shared<FocusButton>();
  button->setBounds(Rect::MakeLTRB(10.0f, 10.0f, 30.0f, 30.0f));
  root->addChild(button);
  window->setRoot(root);

  PointerEvent pointer;
  pointer.position = { 40.0f, 40.0f }; // physical -> logical (20,20)
  window->pointerDown(pointer);
  EXPECT_TRUE(button->focused);
}

TEST(UiDpi, TextIsRasterisedAtTheDeviceScale) {
  const auto font = testFont(16.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 80;
  options.uiScale = 2.0f;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto text = std::make_shared<TextLayer>("Coral", font,
    graphics::Color::MakeRGBA8(255, 255, 255, 255));
  text->setBounds(Rect::MakeLTRB(4.0f, 4.0f, 80.0f, 32.0f));
  window->setRoot(text);
  ASSERT_EQ(window->run(), 1);

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  bool hasLightPixel = false;
  const graphics::Pixmap pixels = bitmap.makePixmap();
  for (int y = 0; y < pixels.height() && !hasLightPixel; ++y) {
    for (int x = 0; x < pixels.width(); ++x) {
      const uint32_t pixel = pixels.getPixelARGB32(x, y);
      const auto red = static_cast<uint8_t>((pixel >> 24) & 0xFF);
      const auto green = static_cast<uint8_t>((pixel >> 16) & 0xFF);
      const auto blue = static_cast<uint8_t>((pixel >> 8) & 0xFF);
      if (red > 180 && green > 180 && blue > 180) {
        hasLightPixel = true;
        break;
      }
    }
  }
  EXPECT_TRUE(hasLightPixel) << "the device-scaled text produced no visible pixels";
}

TEST(UiDpi, ACustomLayerCanOptIntoTheDeviceTextPath) {
  const auto font = testFont(16.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 80;
  options.uiScale = 2.0f;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  window->setRoot(std::make_shared<RawTextLayer>(font));
  ASSERT_EQ(window->run(), 1);

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixels = bitmap.makePixmap();
  bool hasLightPixel = false;
  for (int y = 0; y < pixels.height() && !hasLightPixel; ++y) {
    for (int x = 0; x < pixels.width(); ++x) {
      const uint32_t pixel = pixels.getPixelARGB32(x, y);
      if (((pixel >> 24) & 0xFFu) > 180 && ((pixel >> 16) & 0xFFu) > 180 &&
          ((pixel >> 8) & 0xFFu) > 180) {
        hasLightPixel = true;
        break;
      }
    }
  }
  EXPECT_TRUE(hasLightPixel) << "RenderContext::drawText produced no pixels";
}

TEST(UiTextLayout, TheCrossContextLayoutCacheIsConsistent) {
  const auto first = testFont(16.0f);
  const auto second = testFont(16.0f);
  if (first == nullptr || second == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const std::string text = "a cached line shared by two contexts with enough words to wrap";
  const TextLayout a = layoutText(text, *first, 140.0f);
  const TextLayout b = layoutText(text, *second, 140.0f);
  ASSERT_EQ(a.lines.size(), b.lines.size());
  EXPECT_FLOAT_EQ(a.size.width, b.size.width);
  EXPECT_FLOAT_EQ(a.size.height, b.size.height);
  for (size_t line = 0; line < a.lines.size(); ++line) {
    EXPECT_EQ(a.lines[line].text, b.lines[line].text);
  }
}

TEST(UiRichText, TheCrossContextLayoutCacheIsConsistent) {
  const auto first = testFont(16.0f);
  const auto second = testFont(16.0f);
  if (first == nullptr || second == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  RichTextRun runA;
  runA.text = "shared rich text across contexts with enough words to wrap ";
  runA.font = first;
  runA.color = graphics::Color::MakeRGBA8(255, 0, 0, 255);
  RichTextRun runB;
  runB.text = "and a second styled run";
  runB.font = second;
  runB.color = graphics::Color::MakeRGBA8(0, 0, 255, 255);
  runB.underline = true;

  const RichTextLayout a = layoutRichText({ runA, runB }, 140.0f);
  const RichTextLayout b = layoutRichText({ runA, runB }, 140.0f);
  ASSERT_EQ(a.lines.size(), b.lines.size());
  EXPECT_FLOAT_EQ(a.size.width, b.size.width);
  for (size_t line = 0; line < a.lines.size(); ++line) {
    ASSERT_EQ(a.lines[line].segments.size(), b.lines[line].segments.size());
    for (size_t segment = 0; segment < a.lines[line].segments.size(); ++segment) {
      EXPECT_EQ(a.lines[line].segments[segment].text, b.lines[line].segments[segment].text);
      EXPECT_FLOAT_EQ(a.lines[line].segments[segment].width,
        b.lines[line].segments[segment].width);
    }
  }
}

TEST(UiRichText, RunsKeepTheirOwnFontsColoursAndUnderline) {
  const auto small = testFont(14.0f);
  const auto large = testFont(20.0f);
  if (small == nullptr || large == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  RichTextRun first;
  first.text = "Hello ";
  first.font = small;
  first.color = graphics::Color::MakeRGBA8(255, 0, 0, 255);
  RichTextRun second;
  second.text = "world";
  second.font = large;
  second.color = graphics::Color::MakeRGBA8(0, 0, 255, 255);
  second.underline = true;

  const RichTextLayout layout = layoutRichText({ first, second }, 0.0f);
  ASSERT_EQ(layout.lines.size(), 1u);
  ASSERT_GE(layout.lines[0].segments.size(), 2u);
  EXPECT_EQ(layout.lines[0].segments[0].color.red, 1.0f);
  EXPECT_TRUE(layout.lines[0].segments.back().underline);
  EXPECT_GT(layout.size.width, 0.0f);
  EXPECT_GT(layout.size.height, 0.0f);
}

TEST(UiRichText, RichTextLayerHitTestsAndSelectsAcrossRuns) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  RichTextLayer layer({ RichTextRun{ "hello ", font, nullptr, graphics::Color::White(), false },
    RichTextRun{ "world", font, nullptr, graphics::Color::MakeRGBA8(200, 200, 200, 255), false } });
  layer.setBounds(Rect::MakeWH(160.0f, 40.0f));
  layer.measure({ 160.0f, 40.0f });
  layer.layout();
  layer.setSelectable(true);
  EXPECT_EQ(layer.indexAt({ 0.0f, 5.0f }), 0u);
  EXPECT_GT(layer.indexAt({ 100.0f, 5.0f }), 4u);
  EXPECT_TRUE(layer.onPointerDown({ { 2.0f, 5.0f }, PointerButton::Primary, true, false }));
  EXPECT_TRUE(layer.onPointerMove({ { 80.0f, 5.0f }, PointerButton::Primary, true, false }));
  layer.onPointerUp({ { 80.0f, 5.0f }, PointerButton::Primary, true, false });
  EXPECT_TRUE(layer.hasSelection());
}

TEST(UiDpi, RichTextEllipsisRendersWithARightToLeftBase) {
  const auto font = testFont(16.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Window::Options options;
  options.headless = true;
  options.width = 120;
  options.height = 48;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  RichTextRun run;
  run.text = "\xD7\x90\xD7\x91\xD7\x92 \xD7\x90\xD7\x91\xD7\x92 \xD7\x90\xD7\x91\xD7\x92";
  run.font = font;
  run.color = graphics::Color::MakeRGBA8(255, 255, 255, 255);
  auto text = std::make_shared<RichTextLayer>(std::vector<RichTextRun>{ run });
  text->setBounds(Rect::MakeWH(120.0f, 48.0f));
  text->setWrapWidth(80.0f);
  text->setMaxLines(1);
  text->setEllipsis(true);
  text->setDirection(TextDirectionMode::Rtl);
  window->setRoot(text);
  ASSERT_EQ(window->run(), 1);
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixels = bitmap.makePixmap();
  size_t lightPixels = 0;
  for (int y = 0; y < pixels.height(); ++y) {
    for (int x = 0; x < pixels.width(); ++x) {
      const uint32_t pixel = pixels.getPixelARGB32(x, y);
      if (((pixel >> 24) & 0xFF) > 180 && ((pixel >> 16) & 0xFF) > 180 &&
          ((pixel >> 8) & 0xFF) > 180) {
        ++lightPixels;
      }
    }
  }
  EXPECT_GT(lightPixels, 0u) << "the ellipsis line produced no visible pixels";
}

TEST(UiDpi, VerticalRtlRunOrderChangesThePixelOutput) {
  const auto font = testFont(16.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto render = [&](const TextDirectionMode direction) -> uint64_t {
    Window::Options options;
    options.headless = true;
    options.width = 80;
    options.height = 160;
    options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
    options.frameLimit = 1;
    auto window = Window::create(options);
    if (window == nullptr) return 0;
    auto text = std::make_shared<TextLayer>("abc \xD7\x90\xD7\x91\xD7\x92", font,
      graphics::Color::MakeRGBA8(255, 255, 255, 255));
    text->setBounds(Rect::MakeWH(80.0f, 160.0f));
    text->setWrapWidth(140.0f);
    text->setLineHeight(20.0f);
    text->setOrientation(TextOrientation::VerticalMixed);
    text->setDirection(direction);
    window->setRoot(text);
    if (window->run() != 1) return 0;
    graphics::Bitmap bitmap;
    if (!window->snapshot(&bitmap)) return 0;
    const graphics::Pixmap pixels = bitmap.makePixmap();
    uint64_t checksum = 1469598103934665603ull;
    for (int y = 0; y < pixels.height(); ++y) {
      for (int x = 0; x < pixels.width(); ++x) {
        checksum ^= pixels.getPixelARGB32(x, y);
        checksum *= 1099511628211ull;
      }
    }
    return checksum;
  };
  const uint64_t ltr = render(TextDirectionMode::Ltr);
  const uint64_t rtl = render(TextDirectionMode::Rtl);
  if (ltr == 0 || rtl == 0) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  EXPECT_NE(ltr, rtl) << "vertical run order must affect the rendered pixels";
}

TEST(UiRichText, ARichTextLayerWrapsItsRuns) {
  const auto font = testFont(16.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  RichTextRun run;
  run.text = "one two three four five six seven";
  run.font = font;
  auto layer = std::make_shared<RichTextLayer>(std::vector<RichTextRun>{ run });
  layer->setWrapWidth(80.0f);
  const Size measured = layer->measure({ 80.0f, 0.0f });
  EXPECT_GT(measured.height, font->metrics().ascent + font->metrics().descent);
}

} // namespace

TEST(UiDpi, BidiTextLayerPixelsDependOnTheBaseDirection) {
  const auto font = testFont(16.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto render = [&font](const std::string& text, const TextDirectionMode direction)
      -> std::optional<uint64_t> {
    Window::Options options;
    options.headless = true;
    options.width = 160;
    options.height = 40;
    options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
    options.frameLimit = 1;
    auto window = Window::create(options);
    if (window == nullptr) return std::nullopt;
    auto layer = std::make_shared<TextLayer>(text, font);
    layer->setBounds(Rect::MakeWH(160.0f, 40.0f));
    layer->setDirection(direction);
    window->setRoot(layer);
    if (window->run() != 1) return std::nullopt;
    graphics::Bitmap bitmap;
    if (!window->snapshot(&bitmap)) return std::nullopt;
    const graphics::Pixmap pixels = bitmap.makePixmap();
    uint64_t hash = 1469598103934665603ull;
    for (int y = 0; y < pixels.height(); ++y) {
      for (int x = 0; x < pixels.width(); ++x) {
        hash ^= pixels.getPixelARGB32(x, y);
        hash *= 1099511628211ull;
      }
    }
    return hash;
  };

  // Hebrew letters embedded in Latin text: the visual order mirrors, so the pixels differ.
  const std::string mixed = "abc \xD7\x90\xD7\x91\xD7\x92 def";
  const auto mixedLtr = render(mixed, TextDirectionMode::Ltr);
  const auto mixedRtl = render(mixed, TextDirectionMode::Rtl);
  if (!mixedLtr.has_value() || !mixedRtl.has_value()) {
    GTEST_SKIP() << "the headless surface cannot render text here";
  }
  EXPECT_NE(*mixedLtr, *mixedRtl) << "a bidi paragraph must mirror with the base direction";

  // Pure Latin is unaffected by the base direction.
  const auto latinLtr = render("abc def", TextDirectionMode::Ltr);
  const auto latinRtl = render("abc def", TextDirectionMode::Rtl);
  if (!latinLtr.has_value() || !latinRtl.has_value()) return;
  EXPECT_EQ(*latinLtr, *latinRtl) << "a single-direction Latin run keeps its pixels";
}