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
#include <ui/Command.h>
#include <ui/Flex.h>
#include <ui/TextBidi.h>
#include <ui/TextInput.h>
#include <ui/TextLayer.h>

#include <graphics/FontFallback.h>
#include <ui/Window.h>

#include <graphics/Typeface.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

using namespace coral::ui;

class ButtonLayer final : public Layer {
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

  bool onKeyDown(const KeyEvent& event) override {
    (void)event;
    ++keys;
    return true;
  }

  bool focused = false;
  int keys = 0;
};

class GestureLayer final : public Layer {
 public:
  bool onGesture(const GestureEvent& value) override {
    gesture = value;
    return true;
  }

  bool onScroll(const ScrollEvent& value) override {
    scroll = value;
    return true;
  }

  GestureEvent gesture;
  ScrollEvent scroll;
};

TEST(UiInput, GesturesAndScrollPhasesReachTheLayerUnderThePointer) {
  Window::Options options;
  options.headless = true;
  options.width = 100;
  options.height = 100;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(100.0f, 100.0f));
  auto target = std::make_shared<GestureLayer>();
  target->setBounds(Rect::MakeLTRB(10.0f, 10.0f, 80.0f, 80.0f));
  root->addChild(target);
  window->setRoot(root);

  GestureEvent gesture;
  gesture.kind = GestureKind::Magnify;
  gesture.phase = GesturePhase::Changed;
  gesture.position = { 30.0f, 30.0f };
  gesture.magnitude = 0.25f;
  EXPECT_TRUE(window->gesture(gesture));
  EXPECT_EQ(target->gesture.kind, GestureKind::Magnify);
  EXPECT_FLOAT_EQ(target->gesture.magnitude, 0.25f);

  ScrollEvent scroll;
  scroll.position = { 30.0f, 30.0f };
  scroll.deltaY = -12.0f;
  scroll.phase = ScrollPhase::Began;
  scroll.momentumPhase = ScrollPhase::Stationary;
  scroll.precise = true;
  EXPECT_TRUE(window->scroll(scroll));
  EXPECT_EQ(target->scroll.phase, ScrollPhase::Began);
  EXPECT_EQ(target->scroll.momentumPhase, ScrollPhase::Stationary);
  EXPECT_TRUE(target->scroll.precise);
}

TEST(UiInput, AClickFocusesTheFocusableLayerAndKeysReachIt) {
  Window::Options options;
  options.headless = true;
  options.width = 100;
  options.height = 100;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(100.0f, 100.0f));
  auto button = std::make_shared<ButtonLayer>();
  button->setBounds(Rect::MakeLTRB(10.0f, 10.0f, 60.0f, 40.0f));
  root->addChild(button);
  window->setRoot(root);

  PointerEvent pointer;
  pointer.position = { 20.0f, 20.0f };
  pointer.button = PointerButton::Primary;
  window->pointerDown(pointer);
  EXPECT_TRUE(button->focused);
  EXPECT_EQ(window->focusedLayer(), button.get());

  KeyEvent key;
  key.key = Key::Space;
  window->keyDown(key);
  EXPECT_EQ(button->keys, 1);
}

TEST(UiInput, AClickOutsideTheFocusableLayerClearsFocus) {
  Window::Options options;
  options.headless = true;
  options.width = 100;
  options.height = 100;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(100.0f, 100.0f));
  auto button = std::make_shared<ButtonLayer>();
  button->setBounds(Rect::MakeLTRB(10.0f, 10.0f, 60.0f, 40.0f));
  root->addChild(button);
  window->setRoot(root);

  PointerEvent pointer;
  pointer.position = { 20.0f, 20.0f };
  window->pointerDown(pointer);
  ASSERT_TRUE(button->focused);

  pointer.position = { 90.0f, 90.0f };
  window->pointerDown(pointer);
  EXPECT_FALSE(button->focused);
  EXPECT_EQ(window->focusedLayer(), nullptr);
}

TEST(UiInput, DropEventsReachTheLayerUnderThePointer) {
  class DropLayer final : public Layer {
   public:
    bool onDrop(const DropEvent& event) override {
      paths = event.paths;
      position = event.position;
      return true;
    }
    std::vector<std::string> paths;
    Point position{};
  };

  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 160;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<Layer>();
  auto drop = std::make_shared<DropLayer>();
  drop->setBounds(Rect::MakeWH(120.0f, 100.0f));
  root->addChild(drop);
  window->setRoot(root);
  window->drop({ { 20.0f, 20.0f }, { "/tmp/intro.mp4", "/tmp/music.wav" } });
  EXPECT_EQ(drop->paths.size(), 2u);
  EXPECT_EQ(drop->paths.front(), "/tmp/intro.mp4");
  EXPECT_FLOAT_EQ(drop->position.x, 20.0f);
}

TEST(UiText, OutlineGlyphsProvideHintedCoverageBitmaps) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 14.0f);
  ASSERT_NE(font, nullptr);
  const uint32_t glyph = font->glyphForCodePoint(U'A');
  ASSERT_NE(glyph, 0u);
  const graphics::GlyphBitmap bitmap = typeface->glyphBitmap(glyph, font->size());
  EXPECT_TRUE(bitmap.isValid());
  EXPECT_GT(bitmap.width, 0.0f);
  EXPECT_GT(bitmap.height, 0.0f);

  // The LCD rasterisation is an opt-in path: FreeType may be built without subpixel rendering, in
  // which case it falls back to a grayscale bitmap and the capability check must say so.
  const graphics::GlyphBitmap subpixel = typeface->glyphBitmap(glyph, font->size(), true);
  EXPECT_TRUE(subpixel.isValid());
  if (!subpixel.subpixel) {
    GTEST_SKIP() << "this FreeType build has no LCD/subpixel rasteriser";
  }
  EXPECT_FALSE(subpixel.color);
  EXPECT_GT(subpixel.pixels.width(), 0);
  EXPECT_GT(subpixel.pixels.height(), 0);
  EXPECT_NEAR(subpixel.width, bitmap.width, 2.0f)
    << "the LCD mask is three coverage bytes wide but the same logical glyph width";

  // A fractional phase must produce a different rasterisation; it is part of the glyph cache key.
  const graphics::GlyphBitmap shifted = typeface->glyphBitmap(glyph, font->size(), true, 0.25f);
  EXPECT_TRUE(shifted.isValid());
  const graphics::Pixmap base = subpixel.pixels.makePixmap();
  const graphics::Pixmap shiftedPixels = shifted.pixels.makePixmap();
  bool differs = base.width() != shiftedPixels.width() || base.height() != shiftedPixels.height();
  if (!differs) {
    for (int y = 0; y < base.height() && !differs; ++y) {
      for (int x = 0; x < base.width(); ++x) {
        if (base.getPixelARGB32(x, y) != shiftedPixels.getPixelARGB32(x, y)) {
          differs = true;
          break;
        }
      }
    }
  }
  EXPECT_TRUE(differs) << "subpixel phase must change the glyph coverage, not just its cache key";
}

TEST(UiText, MultilineTextInputMovesBetweenVisualLines) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 14.0f);
  ASSERT_NE(font, nullptr);
  TextInput input("a\nb", font);
  input.setMultiline(true);
  input.setBounds(Rect::MakeWH(160.0f, 100.0f));
  input.measure({ 160.0f, 100.0f });
  input.layout();
  input.onFocusChanged(true);
  input.setCursor(input.text().size());
  KeyEvent up;
  up.key = Key::Up;
  EXPECT_TRUE(input.onKeyDown(up));
  EXPECT_EQ(input.cursor(), 1u);
  KeyEvent home;
  home.key = Key::Home;
  input.onKeyDown(home);
  EXPECT_EQ(input.cursor(), 0u);
  input.setCursor(input.text().size());
  KeyEvent enter;
  enter.key = Key::Enter;
  input.onKeyDown(enter);
  EXPECT_EQ(input.text(), "a\nb\n");
}

TEST(UiText, TextLayerReportsHitTestingAndSelection) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 14.0f);
  ASSERT_NE(font, nullptr);
  TextLayer label("hello world", font, graphics::Color::White());
  label.setBounds(Rect::MakeWH(120.0f, 60.0f));
  label.setWrapWidth(48.0f);
  label.measure({ 120.0f, 60.0f });
  label.layout();
  EXPECT_EQ(label.indexAt({ 0.0f, 5.0f }), 0u);
  EXPECT_GT(label.indexAt({ 100.0f, 5.0f }), 0u);
  label.setSelectable(true);
  EXPECT_TRUE(label.onPointerDown({ { 2.0f, 5.0f }, PointerButton::Primary, true, false }));
  EXPECT_TRUE(label.onPointerMove({ { 40.0f, 5.0f }, PointerButton::Primary, true, false }));
  label.onPointerUp({ { 40.0f, 5.0f }, PointerButton::Primary, true, false });
  EXPECT_TRUE(label.hasSelection());
}

TEST(UiText, BidiSegmentsStrongRunsAndReversesRtlParagraphs) {
  const std::string arabic = "\xD8\xA7\xD9\x84\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A\xD8\xA9";
  const std::string mixed = "abc " + arabic + " def";
  EXPECT_EQ(baseDirection(mixed), TextDirection::Ltr);
  const auto runs = bidiRuns(mixed);
  ASSERT_EQ(runs.size(), 3u);
  EXPECT_EQ(runs[0].direction, TextDirection::Ltr);
  EXPECT_EQ(runs[1].direction, TextDirection::Rtl);
  EXPECT_EQ(runs[2].direction, TextDirection::Ltr);
  EXPECT_EQ(baseDirection(arabic), TextDirection::Rtl);
  const auto order = visualRunOrder(runs, TextDirection::Rtl);
  ASSERT_EQ(order.size(), 3u);
  EXPECT_EQ(order.front(), 2u);
  EXPECT_EQ(order.back(), 0u);
}

TEST(UiText, FontFallbackSplitsRunsAndLaysOutTheMixedString) {
  const std::string latinPath = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const std::string emojiPath = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoColorEmoji.ttf";
  const auto latinFace = graphics::Typeface::fromFile(latinPath);
  const auto emojiFace = graphics::Typeface::fromFile(emojiPath);
  if (latinFace == nullptr || emojiFace == nullptr) {
    GTEST_SKIP() << "the fallback test fonts are not available";
  }
  const auto latin = graphics::Font::create(latinFace, 14.0f);
  const auto emoji = graphics::Font::create(emojiFace, 14.0f);
  ASSERT_NE(latin, nullptr);
  ASSERT_NE(emoji, nullptr);
  graphics::FontFallback fallback(latin, { emoji });
  const std::string text = "A\xF0\x9F\x98\x80" "B";
  const auto runs = fallback.shape(text);
  ASSERT_GE(runs.size(), 2u);
  bool usedFallback = false;
  for (const auto& run: runs) {
    if (run.font != nullptr && run.font->typeface() == emojiFace) usedFallback = true;
  }
  EXPECT_TRUE(usedFallback);
  EXPECT_GT(fallback.measure(text), latin->measureWidth("AB"));

  TextLayer label(text, latin, graphics::Color::White());
  label.setFontFallback(std::make_shared<graphics::FontFallback>(fallback));
  label.setBounds(Rect::MakeWH(160.0f, 28.0f));
  const Size measured = label.measure({ 160.0f, 28.0f });
  EXPECT_GT(measured.width, 0.0f);
}

TEST(UiText, NumpadAndModifierKeysAreDistinctAndLabelled) {
  EXPECT_NE(Key::LeftShift, Key::RightShift);
  EXPECT_NE(Key::LeftControl, Key::RightControl);
  EXPECT_EQ(keyLabel(Key::Numpad5), "Num 5");
  EXPECT_EQ(keyLabel(Key::NumpadAdd), "Num +");
  EXPECT_EQ(keyLabel(Key::NumpadEnter), "Num Enter");
  EXPECT_EQ(keyLabel(Key::CapsLock), "Caps Lock");
  EXPECT_EQ(keyLabel(Key::LeftShift), "Left Shift");

  // A shortcut on a keypad key matches that key only; the digit row is a different key.
  const Shortcut numpadAdd{ Key::NumpadAdd };
  KeyEvent event;
  event.key = Key::NumpadAdd;
  EXPECT_TRUE(numpadAdd.matches(event));
  event.key = Key::NumpadSubtract;
  EXPECT_FALSE(numpadAdd.matches(event));
  event.key = Key::Equal;
  EXPECT_FALSE(numpadAdd.matches(event)) << "the keypad and the digit row are separate keys";
  EXPECT_EQ(shortcutLabel(numpadAdd), "Num +");
}

TEST(UiText, TextInputDoubleClickSelectsAWordAndControlDeleteRemovesIt) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 14.0f);
  ASSERT_NE(font, nullptr);
  TextInput input("alpha beta", font);
  input.setBounds(Rect::MakeWH(220.0f, 32.0f));
  input.measure({ 220.0f, 32.0f });
  input.layout();
  input.onFocusChanged(true);
  const float betaCentre = 10.0f + font->measureWidth("alpha ") + font->measureWidth("be");
  PointerEvent click{ { betaCentre, 16.0f }, PointerButton::Primary, true, false };
  EXPECT_TRUE(input.onPointerDown(click));
  EXPECT_TRUE(input.onPointerDown(click));
  EXPECT_EQ(input.selectedText(), "beta");
  input.onPointerUp(click);
  input.setCursor(input.text().size());
  KeyEvent wordDelete;
  wordDelete.key = Key::Backspace;
  wordDelete.control = true;
  EXPECT_TRUE(input.onKeyDown(wordDelete));
  EXPECT_EQ(input.text(), "alpha ");
}

TEST(UiText, MultilineTextInputScrollsToKeepTheCursorVisible) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 14.0f);
  ASSERT_NE(font, nullptr);
  TextInput input("line\nline\nline\nline\nline\nline\nline\nline", font);
  input.setMultiline(true);
  input.setBounds(Rect::MakeWH(180.0f, 60.0f));
  input.measure({ 180.0f, 60.0f });
  input.layout();
  input.onFocusChanged(true);
  input.setCursor(input.text().size());
  EXPECT_GT(input.scrollOffset(), 0.0f);
  const Rect cursor = input.imeCursorRect();
  EXPECT_GE(cursor.getTop(), input.bounds().getTop() - 1.0f);
  EXPECT_LE(cursor.getBottom(), input.bounds().getBottom() + 1.0f);
}

TEST(UiText, TextInputRightClickOpensAContextMenu) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 14.0f);
  ASSERT_NE(font, nullptr);
  TextInput input("context", font);
  input.setBounds(Rect::MakeWH(220.0f, 32.0f));
  input.measure({ 220.0f, 32.0f });
  input.layout();
  EXPECT_TRUE(input.onPointerDown({ { 20.0f, 16.0f }, PointerButton::Secondary, true, false }));
  bool menuVisible = false;
  for (const auto& child: input.children()) {
    if (child->visible()) menuVisible = true;
  }
  EXPECT_TRUE(menuVisible);
}

TEST(UiText, ALabelMeasuresWithTheFontMetrics) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 18.0f);
  ASSERT_NE(font, nullptr);

  TextLayer label("Coral UI", font, graphics::Color::White());
  const Size measured = label.measure({ 500.0f, 100.0f });
  EXPECT_GT(measured.width, 0.0f);
  EXPECT_GT(measured.height, 0.0f);
  EXPECT_NEAR(measured.height, font->metrics().ascent + font->metrics().descent, 0.001f);
}

TEST(UiText, BidiCursorUsesVisualRunOrder) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 16.0f);
  TextInput input("\xD7\x90\xD7\x91\xD7\x92 abc", font);
  input.setBounds(Rect::MakeWH(220.0f, 32.0f));
  input.measure({ 220.0f, 32.0f });
  input.layout();
  input.onFocusChanged(true);
  input.setCursor(0);
  const Rect atStart = input.imeCursorRect();
  input.setCursor(6);
  const Rect atRtlEnd = input.imeCursorRect();
  EXPECT_GT(atStart.getLeft(), atRtlEnd.getLeft())
    << "in an RTL run the logical start must sit to the right of its end";
}

} // namespace
