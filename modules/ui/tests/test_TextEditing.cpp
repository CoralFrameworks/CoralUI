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

#include <ui/TextInput.h>
#include <ui/Undo.h>
#include <ui/TextLayer.h>

#include <graphics/Font.h>
#include <graphics/Typeface.h>

#include <string>

namespace {

std::shared_ptr<const graphics::Font> testFont(const float size) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  return typeface != nullptr ? graphics::Font::create(typeface, size) : nullptr;
}

coral::ui::KeyEvent key(const coral::ui::Key value) {
  coral::ui::KeyEvent event;
  event.key = value;
  return event;
}

} // namespace

TEST(UiTextEditing, ArrowAndBackspaceStepOverGraphemeClusters) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::TextInput input("e\xCC\x81x", font);
  input.setCursor(input.text().size());
  input.onKeyDown(key(coral::ui::Key::Left));
  EXPECT_EQ(input.cursor(), 3u) << "left moves before 'x'";
  input.onKeyDown(key(coral::ui::Key::Left));
  EXPECT_EQ(input.cursor(), 0u) << "the e + combining acute cluster is one step";

  input.setCursor(input.text().size());
  input.onKeyDown(key(coral::ui::Key::Backspace));
  EXPECT_EQ(input.text(), "e\xCC\x81");
  input.onKeyDown(key(coral::ui::Key::Backspace));
  EXPECT_TRUE(input.text().empty()) << "backspace removes the whole cluster";
}

TEST(UiTextEditing, EmojiZwjAndFlagsAreEditedAsSingleClusters) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string family = "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D"
                              "\xF0\x9F\x91\xA7\xE2\x80\x8D\xF0\x9F\x91\xA6";
  coral::ui::TextInput input(family + "a", font);
  input.setCursor(input.text().size());
  input.onKeyDown(key(coral::ui::Key::Backspace));
  EXPECT_EQ(input.text(), family);
  input.onKeyDown(key(coral::ui::Key::Backspace));
  EXPECT_TRUE(input.text().empty()) << "the ZWJ sequence is one grapheme cluster";

  coral::ui::TextInput flags("\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8", font);
  flags.setCursor(flags.text().size());
  flags.onKeyDown(key(coral::ui::Key::Left));
  EXPECT_EQ(flags.cursor(), 0u) << "a regional-indicator pair is one cluster";
  flags.onKeyDown(key(coral::ui::Key::Delete));
  EXPECT_TRUE(flags.text().empty());
}

TEST(UiTextEditing, RtlArrowKeysFollowVisualOrder) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string hebrew = "\xD7\x90\xD7\x91\xD7\x92"; // אבג
  coral::ui::TextInput input(hebrew, font);
  input.setBounds(coral::ui::Rect::MakeWH(160.0f, 32.0f));
  input.setCursor(0);
  input.onKeyDown(key(coral::ui::Key::Left));
  EXPECT_EQ(input.cursor(), 2u) << "left moves to the visual left, which is the next logical boundary";
  input.setCursor(2);
  input.onKeyDown(key(coral::ui::Key::Right));
  EXPECT_EQ(input.cursor(), 0u) << "right moves to the visual right, the previous logical boundary";
  input.setCursor(static_cast<size_t>(hebrew.size()));
  input.onKeyDown(key(coral::ui::Key::Right));
  EXPECT_EQ(input.cursor(), 4u);
  input.onKeyDown(key(coral::ui::Key::Right));
  EXPECT_EQ(input.cursor(), 2u);
  input.onKeyDown(key(coral::ui::Key::Right));
  EXPECT_EQ(input.cursor(), 0u);
  input.onKeyDown(key(coral::ui::Key::Right));
  EXPECT_EQ(input.cursor(), 0u) << "logical start is the rightmost caret stop in an RTL run";
}

TEST(UiTextEditing, TextLayerHitTestingStopsAtClusterBoundaries) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const std::string text = "e\xCC\x81x";
  coral::ui::TextLayer layer(text, font);
  layer.setBounds(coral::ui::Rect::MakeWH(120.0f, 40.0f));
  layer.measure({ 120.0f, 40.0f });
  const float cluster = font->measureWidth("e\xCC\x81");
  EXPECT_EQ(layer.indexAt({ cluster * 0.5f, 4.0f }), 0u);
  EXPECT_EQ(layer.indexAt({ cluster * 0.7f, 4.0f }), 3u)
          << "the next caret stop is after the whole cluster, not inside it";
}
TEST(UiTextEditing, SharedUndoStackSpansTextInputs) {
  const auto font = testFont(16.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  auto stack = std::make_shared<coral::ui::UndoStack>();
  coral::ui::TextInput first("", font);
  coral::ui::TextInput second("", font);
  first.setUndoStack(stack);
  second.setUndoStack(stack);
  EXPECT_EQ(first.undoStack(), stack);

  ASSERT_TRUE(first.onTextInput(U'a'));
  ASSERT_TRUE(first.onTextInput(U'b'));
  EXPECT_EQ(first.text(), "ab");
  EXPECT_TRUE(stack->undo());
  EXPECT_EQ(first.text(), "a") << "the shared history restores the previous snapshot";
  EXPECT_TRUE(stack->redo());
  EXPECT_EQ(first.text(), "ab");
  EXPECT_EQ(second.text(), "") << "the other field is untouched";
}