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

#include <ui/Button.h>
#include <ui/Dialog.h>
#include <ui/ListView.h>
#include <ui/Menu.h>
#include <ui/ScrollView.h>
#include <ui/TextInput.h>
#include <ui/TextLayout.h>
#include <ui/Window.h>

#include <graphics/Bitmap.h>
#include <graphics/Image.h>

#include <graphics/Typeface.h>

#include <gtest/gtest.h>

namespace {

using namespace coral::ui;

std::shared_ptr<const graphics::Font> testFont(const float size) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  return typeface != nullptr ? graphics::Font::create(typeface, size) : nullptr;
}

class FixedLayer final : public Layer {
 public:
  FixedLayer(const float width, const float height) : mSize{ width, height } {}

  Size measure(const Size& available) override {
    (void)available;
    setMeasuredSize(mSize);
    return mSize;
  }

 private:
  Size mSize;
};

TEST(UiTextLayout, WrappingBreaksAtSpacesAndRespectsTheWidth) {
  const auto font = testFont(16.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const TextLayout layout = layoutText("one two three four five six", *font, 80.0f);
  ASSERT_GT(layout.lines.size(), 1u);
  for (const TextLine& line: layout.lines) {
    EXPECT_LE(line.width, 80.0f + 0.5f) << line.text;
    EXPECT_EQ(line.text.find('\n'), std::string::npos);
  }
  EXPECT_GT(layout.size.height, 0.0f);
}

TEST(UiTextLayout, ANewlineStartsANewLine) {
  const auto font = testFont(16.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const TextLayout layout = layoutText("first\nsecond", *font, 0.0f);
  ASSERT_EQ(layout.lines.size(), 2u);
  EXPECT_EQ(layout.lines[0].text, "first");
  EXPECT_EQ(layout.lines[1].text, "second");
}

TEST(UiText, CjkBreaksBetweenIdeographsAndRespectsClosingPunctuation) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const std::string text = "\xE4\xB8\xAD\xE6\x96\x87\xE3\x80\x82\xE6\xB5\x8B\xE8\xAF\x95";
  const TextLayout layout = layoutText(text, *font, font->measureWidth("\xE4\xB8\xAD\xE6\x96\x87") + 1.0f);
  EXPECT_GE(layout.lines.size(), 2u);
  for (const TextLine& line: layout.lines) {
    EXPECT_FALSE(line.text.empty());
    if (line.text.size() >= 3) {
      EXPECT_NE(line.text.substr(0, 3), "\xE3\x80\x82")
        << "closing punctuation must not start a wrapped line";
    }
  }
}

TEST(UiText, EllipsisTruncatesToTheLastLineBudget) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  TextLayoutOptions options{ 52.0f, 1, true };
  // Legacy editor behaviour: an overlong word wraps instead of overflowing.
  options.policy.overflowWrap = OverflowWrap::BreakWord;
  const TextLayout layout = layoutText("abcdefghijklmnopqrstuvwxyz", *font, options);
  ASSERT_EQ(layout.lines.size(), 1u);
  EXPECT_TRUE(layout.truncated);
  EXPECT_NE(layout.lines.front().text.find("\xE2\x80\xA6"), std::string::npos);
  EXPECT_LE(layout.lines.front().width, 52.0f);
}

TEST(UiScroll, TheScrollOffsetClampsToTheContent) {
  auto content = std::make_shared<FixedLayer>(80.0f, 200.0f);
  auto view = std::make_shared<ScrollView>();
  view->setBounds(Rect::MakeWH(100.0f, 100.0f));
  view->setContent(content);
  view->measure({ 100.0f, 100.0f });
  view->layout();
  EXPECT_FLOAT_EQ(view->contentSize().height, 200.0f);

  ScrollEvent scroll;
  scroll.position = { 50.0f, 50.0f };
  scroll.deltaY = -500.0f;
  ASSERT_TRUE(view->onScroll(scroll));
  EXPECT_FLOAT_EQ(view->scrollY(), 100.0f);

  scroll.deltaY = 500.0f;
  view->onScroll(scroll);
  EXPECT_FLOAT_EQ(view->scrollY(), 0.0f);
}

TEST(UiButton, AClickAndSpaceBothActivate) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  auto button = std::make_shared<Button>("Run", font);
  button->setBounds(Rect::MakeWH(100.0f, 32.0f));
  int clicks = 0;
  button->setOnClick([&clicks] { ++clicks; });

  PointerEvent pointer;
  pointer.position = { 20.0f, 16.0f };
  ASSERT_TRUE(button->onPointerDown(pointer));
  ASSERT_TRUE(button->onPointerUp(pointer));
  EXPECT_EQ(clicks, 1);

  KeyEvent key;
  key.key = Key::Space;
  ASSERT_TRUE(button->onKeyDown(key));
  EXPECT_EQ(clicks, 2);
  EXPECT_EQ(button->activationCount(), 2);
}

TEST(UiTextInput, TypingAndBackspaceEditTheText) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 60;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto input = std::make_shared<TextInput>("", font);
  std::string latest;
  auto changed = input->observeChanged([&latest](const std::string& text) { latest = text; });
  window->setRoot(input);

  PointerEvent pointer;
  pointer.position = { 20.0f, 20.0f };
  window->pointerDown(pointer);
  EXPECT_EQ(window->focusedLayer(), input.get());

  window->textInput(U'A');
  window->textInput(U'B');
  window->textInput(U'C');
  EXPECT_EQ(input->text(), "ABC");
  EXPECT_EQ(latest, "ABC");

  KeyEvent backspace;
  backspace.key = Key::Backspace;
  window->keyDown(backspace);
  EXPECT_EQ(input->text(), "AB");

  KeyEvent left;
  left.key = Key::Left;
  window->keyDown(left);
  window->textInput(U'X');
  EXPECT_EQ(input->text(), "AXB");
}

TEST(UiList, SelectingARowReportsItsIndexAndText) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 160;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto list = std::make_shared<ListView>(font, 28.0f);
  list->addItem("First");
  list->addItem("Second");
  list->addItem("Third");
  int selected = -1;
  std::string selectedText;
  list->setOnSelection([&](const int index, const std::string& text) {
    selected = index;
    selectedText = text;
  });
  window->setRoot(list);

  PointerEvent pointer;
  pointer.position = { 20.0f, 14.0f };
  window->pointerDown(pointer);
  EXPECT_EQ(list->selectedIndex(), 0);
  EXPECT_EQ(selected, 0);
  EXPECT_EQ(selectedText, "First");

  KeyEvent space;
  space.key = Key::Space;
  window->keyDown(space);
  EXPECT_EQ(list->selectedIndex(), 0);
}

TEST(UiList, ALargeListIsStillOneContentLayer) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  auto list = std::make_shared<ListView>(font, 20.0f);
  for (int index = 0; index < 10000; ++index) {
    list->addItem("Item " + std::to_string(index));
  }
  list->setBounds(Rect::MakeWH(200.0f, 100.0f));
  list->measure({ 200.0f, 100.0f });
  list->layout();
  EXPECT_EQ(list->itemCount(), 10000u);
  list->setSelectedIndex(9999);
  EXPECT_EQ(list->selectedIndex(), 9999);
}

TEST(UiMenu, ClickingAnItemRunsItAndCloses) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 160;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto menu = std::make_shared<Menu>(font);
  int activations = 0;
  menu->addItem("Open", [&activations] { ++activations; });
  menu->addItem("Close", [&activations] { activations += 10; });
  window->setRoot(menu);
  menu->showAt({ 10.0f, 10.0f });
  ASSERT_TRUE(menu->isOpen());

  PointerEvent pointer;
  pointer.position = { 20.0f, 20.0f };
  window->pointerDown(pointer);
  EXPECT_EQ(activations, 1);
  EXPECT_FALSE(menu->isOpen());
}

TEST(UiMenu, KeyboardNavigationSkipsSeparatorsAndDisabledItems) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Menu menu(font);
  int activations = 0;
  menu.addItem("One", [&activations] { activations += 1; });
  menu.addSeparator();
  menu.addItem("Disabled", [&activations] { activations += 100; }, false);
  menu.addItem("Two", [&activations] { activations += 10; });
  menu.showAt({ 10.0f, 10.0f });
  KeyEvent down;
  down.key = Key::Down;
  EXPECT_TRUE(menu.onKeyDown(down));
  EXPECT_TRUE(menu.onKeyDown(down));
  KeyEvent space;
  space.key = Key::Space;
  EXPECT_TRUE(menu.onKeyDown(space));
  EXPECT_EQ(activations, 10);
  EXPECT_FALSE(menu.isOpen());
}

TEST(UiMenu, AnchorsInsideTheParentAndOpensASubmenu) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(200.0f, 160.0f));
  auto menu = std::make_shared<Menu>(font);
  auto submenu = std::make_shared<Menu>(font);
  submenu->addItem("Nested", [] {});
  menu->addSubmenu("More", submenu);
  root->addChild(menu);
  menu->showAt(Rect::MakeXYWH(190.0f, 150.0f, 10.0f, 5.0f));
  EXPECT_LE(menu->panelBounds().getRight(), 200.0f);
  EXPECT_LE(menu->panelBounds().getBottom(), 160.0f);
  KeyEvent down;
  down.key = Key::Down;
  menu->onKeyDown(down);
  KeyEvent right;
  right.key = Key::Right;
  menu->onKeyDown(right);
  EXPECT_TRUE(submenu->isOpen());
}

TEST(UiMenu, CheckRadioAndIconItemsUpdateTheirState) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Menu menu(font);
  bool checked = true;
  const int check = menu.addCheckItem("Grid", true, [&checked](const bool value) {
    checked = value;
  });
  const int radioA = menu.addRadioItem("Option A", true, [] {});
  const int radioB = menu.addRadioItem("Option B", false, [] {});
  EXPECT_EQ(check, 0);
  EXPECT_TRUE(menu.isItemChecked(check));
  EXPECT_TRUE(menu.isItemChecked(radioA));
  EXPECT_FALSE(menu.isItemChecked(radioB));

  menu.showAt({ 0.0f, 0.0f });
  KeyEvent down;
  down.key = Key::Down;
  menu.onKeyDown(down);
  KeyEvent space;
  space.key = Key::Space;
  menu.onKeyDown(space);
  EXPECT_FALSE(checked);
  EXPECT_FALSE(menu.isItemChecked(check));
  EXPECT_FALSE(menu.isOpen());

  menu.setItemChecked(radioB, true);
  EXPECT_FALSE(menu.isItemChecked(radioA));
  EXPECT_TRUE(menu.isItemChecked(radioB));

  graphics::Bitmap bitmap = graphics::Bitmap::allocate(2, 2);
  ASSERT_FALSE(bitmap.isEmpty());
  menu.setItemIcon(radioB,
    std::make_shared<graphics::Image>(graphics::Image::fromBitmap(bitmap)));
}

TEST(UiMenu, CheckRadioAndIconItemsDrawIntoAFrame) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Window::Options options;
  options.headless = true;
  options.width = 220;
  options.height = 160;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<Layer>();
  auto menu = std::make_shared<Menu>(font);
  menu->addCheckItem("Grid", true, [](bool) {});
  menu->addRadioItem("Option A", true, [] {});
  menu->addRadioItem("Option B", false, [] {});
  graphics::Bitmap icon = graphics::Bitmap::allocate(4, 4);
  if (!icon.isEmpty()) {
    menu->setItemIcon(0,
      std::make_shared<graphics::Image>(graphics::Image::fromBitmap(icon)));
  }
  root->addChild(menu);
  window->setRoot(root);
  menu->showAt({ 8.0f, 8.0f });
  ASSERT_EQ(window->run(), 1);
  graphics::Bitmap frame;
  EXPECT_TRUE(window->snapshot(&frame));
}

TEST(UiMenu, PopupOwnsKeysWhileAnotherLayerHasFocus) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
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
  auto input = std::make_shared<TextInput>("focus", font);
  root->addChild(input);
  auto menu = std::make_shared<Menu>(font);
  menu->addItem("Action", [] {});
  root->addChild(menu);
  window->setRoot(root);
  window->focusLayer(input.get());
  menu->showAt({ 10.0f, 10.0f });
  KeyEvent escape;
  escape.key = Key::Escape;
  window->keyDown(escape);
  EXPECT_FALSE(menu->isOpen());
}

TEST(UiDialog, AButtonInsideTheDialogRunsAndCloses) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Window::Options options;
  options.headless = true;
  options.width = 240;
  options.height = 180;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto dialog = std::make_shared<Dialog>("Confirm", font);
  dialog->setMessage("Continue?");
  int confirmed = 0;
  dialog->addButton("OK", [&confirmed] { ++confirmed; });
  window->setRoot(dialog);
  dialog->show();
  ASSERT_TRUE(dialog->isOpen());

  const Rect panel = dialog->panelBounds();
  PointerEvent pointer;
  pointer.position = { panel.getRight() - 50.0f, panel.getBottom() - 36.0f };
  window->pointerDown(pointer);
  window->pointerUp(pointer);
  EXPECT_EQ(confirmed, 1);
  EXPECT_FALSE(dialog->isOpen());
}

TEST(UiTextInput, AnImeCanDeleteAroundTheCursor) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  auto input = std::make_shared<TextInput>("abcd", font);
  ASSERT_TRUE(input->acceptsTextInput());
  ASSERT_TRUE(input->onTextDelete(-2, 2)); // delete "cd" before the end cursor
  EXPECT_EQ(input->text(), "ab");
  KeyEvent left;
  left.key = Key::Left;
  ASSERT_TRUE(input->onKeyDown(left));
  ASSERT_TRUE(input->onTextDelete(0, 1)); // delete "b" after the cursor
  EXPECT_EQ(input->text(), "a");
}

TEST(UiTextInput, AnImeCompositionIsShownAndThenCommitted) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 60;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto input = std::make_shared<TextInput>("", font);
  window->setRoot(input);
  PointerEvent pointer;
  pointer.position = { 20.0f, 20.0f };
  window->pointerDown(pointer);

  window->textComposition("ni");
  EXPECT_EQ(input->composition(), "ni");
  window->textCommit("ni");
  EXPECT_TRUE(input->composition().empty());
  EXPECT_EQ(input->text(), "ni");
}

} // namespace
