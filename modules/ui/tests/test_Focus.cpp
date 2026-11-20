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

#include <ui/Clipboard.h>

#if defined(_WIN32)
#include <windows.h>
#endif
#include <ui/Command.h>
#include <ui/TextInput.h>
#include <ui/Window.h>

#include <graphics/Typeface.h>
#include <graphics/Image.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace coral::ui;

class FocusLayer final : public Layer {
 public:
  explicit FocusLayer(bool consumeKeys = false) : mConsumeKeys(consumeKeys) {}

  [[nodiscard]] bool focusable() const override {
    return true;
  }

  bool onPointerDown(const PointerEvent& event) override {
    (void)event;
    return true;
  }

  bool onKeyDown(const KeyEvent& event) override {
    (void)event;
    ++keys;
    return mConsumeKeys;
  }

  void onFocusChanged(const bool value) override {
    focused = value;
  }

  bool focused = false;
  int keys = 0;

 private:
  bool mConsumeKeys = false;
};

std::shared_ptr<const graphics::Font> testFont(const float size) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  return typeface != nullptr ? graphics::Font::create(typeface, size) : nullptr;
}

std::unique_ptr<Window> headlessWindow() {
  Window::Options options;
  options.headless = true;
  options.width = 320;
  options.height = 200;
  options.frameLimit = 1;
  return Window::create(options);
}

KeyEvent keyEvent(const Key key, const bool control = false, const bool shift = false) {
  KeyEvent event;
  event.key = key;
  event.control = control;
  event.shift = shift;
  return event;
}

} // namespace

TEST(UiFocus, TabTraversesFocusableLayersAndShiftGoesBack) {
  auto window = headlessWindow();
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(320.0f, 200.0f));
  auto first = std::make_shared<FocusLayer>();
  auto second = std::make_shared<FocusLayer>();
  first->setBounds(Rect::MakeLTRB(10.0f, 10.0f, 100.0f, 40.0f));
  second->setBounds(Rect::MakeLTRB(10.0f, 50.0f, 100.0f, 80.0f));
  root->addChild(first);
  root->addChild(second);
  window->setRoot(root);

  window->focusLayer(first.get());
  window->keyDown(keyEvent(Key::Tab));
  EXPECT_EQ(window->focusedLayer(), second.get());
  EXPECT_TRUE(second->focused);

  window->keyDown(keyEvent(Key::Tab, false, true));
  EXPECT_EQ(window->focusedLayer(), first.get());
}

TEST(UiFocus, AFocusScopeTrapsTabAndClickFocus) {
  auto window = headlessWindow();
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(320.0f, 200.0f));
  auto outside = std::make_shared<FocusLayer>();
  outside->setBounds(Rect::MakeLTRB(10.0f, 10.0f, 100.0f, 40.0f));
  auto scope = std::make_shared<Layer>();
  scope->setFocusScope(true);
  scope->setBounds(Rect::MakeLTRB(120.0f, 10.0f, 300.0f, 180.0f));
  auto first = std::make_shared<FocusLayer>();
  auto second = std::make_shared<FocusLayer>();
  first->setBounds(Rect::MakeLTRB(130.0f, 20.0f, 280.0f, 50.0f));
  second->setBounds(Rect::MakeLTRB(130.0f, 60.0f, 280.0f, 90.0f));
  scope->addChild(first);
  scope->addChild(second);
  root->addChild(outside);
  root->addChild(scope);
  window->setRoot(root);

  window->focusLayer(outside.get());
  window->keyDown(keyEvent(Key::Tab));
  EXPECT_EQ(window->focusedLayer(), first.get());
  window->keyDown(keyEvent(Key::Tab));
  EXPECT_EQ(window->focusedLayer(), second.get());

  // A click on the root outside the scope cannot move focus out of it.
  PointerEvent pointer;
  pointer.position = { 40.0f, 20.0f };
  window->pointerDown(pointer);
  EXPECT_EQ(window->focusedLayer(), second.get());
}

TEST(UiFocus, TheCommandTableRunsAfterTheFocusedLayerDeclinesTheKey) {
  auto window = headlessWindow();
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(320.0f, 200.0f));
  auto focus = std::make_shared<FocusLayer>();
  focus->setBounds(Rect::MakeLTRB(10.0f, 10.0f, 100.0f, 40.0f));
  root->addChild(focus);
  window->setRoot(root);
  window->focusLayer(focus.get());

  int runs = 0;
  window->commands().bind(Shortcut::controlKey(Key::S), "save", [&runs] { ++runs; });
  window->keyDown(keyEvent(Key::S, true));
  EXPECT_EQ(runs, 1);
  EXPECT_EQ(focus->keys, 1);
  EXPECT_EQ(window->commands().shortcutLabelFor("save"), "Ctrl+S");
}

TEST(UiFocus, CommandShortcutsCoverFunctionDigitsAndPunctuationKeys) {
  CommandRegistry commands;
  int runs = 0;
  commands.bind(Shortcut::controlKey(Key::Digit1), "view.one", [&runs] { ++runs; });
  commands.bind(Shortcut{ Key::F5 }, "refresh", [&runs] { ++runs; });
  commands.bind(Shortcut::controlShift(Key::Z), "redo", [&runs] { ++runs; });
  commands.bind(Shortcut{ Key::Comma }, "prefs", [&runs] { ++runs; });

  EXPECT_TRUE(commands.dispatch(keyEvent(Key::Digit1, true)));
  EXPECT_TRUE(commands.dispatch(keyEvent(Key::F5)));
  EXPECT_TRUE(commands.dispatch(keyEvent(Key::Z, true, true)));
  EXPECT_TRUE(commands.dispatch(keyEvent(Key::Comma)));
  EXPECT_EQ(runs, 4);
  EXPECT_EQ(shortcutLabel(Shortcut::controlShift(Key::Z)), "Ctrl+Shift+Z");
  EXPECT_EQ(shortcutLabel(Shortcut{ Key::F5 }), "F5");
  EXPECT_EQ(keyLabel(Key::PageUp), "PageUp");
  EXPECT_EQ(keyLabel(Key::Digit1), "1");
}

TEST(UiFocus, DisabledCommandsDoNotConsumeTheirShortcut) {
  CommandRegistry commands;
  bool enabled = false;
  int runs = 0;
  commands.bind(Shortcut::controlKey(Key::S), "save", [&runs] { ++runs; },
    [&enabled] { return enabled; });
  EXPECT_FALSE(commands.isEnabled("save"));
  EXPECT_FALSE(commands.dispatch(keyEvent(Key::S, true)));
  EXPECT_EQ(runs, 0);

  enabled = true;
  EXPECT_TRUE(commands.dispatch(keyEvent(Key::S, true)));
  EXPECT_EQ(runs, 1);

  commands.setEnabled("save", false);
  EXPECT_FALSE(commands.invoke("save"));
  EXPECT_EQ(runs, 1);
  commands.setEnabled("save", true);
  EXPECT_TRUE(commands.invoke("save"));
  EXPECT_EQ(runs, 2);
}

TEST(UiFocus, CommandsExposeCheckedStateAndNotifyListeners) {
  CommandRegistry commands;
  int changes = 0;
  commands.setChangedCallback([&changes] { ++changes; });
  commands.bind("view.grid", [] {});
  commands.setChecked("view.grid", true);
  EXPECT_TRUE(commands.isChecked("view.grid"));
  EXPECT_GT(changes, 0);
  commands.setChecked("view.grid", false);
  EXPECT_FALSE(commands.isChecked("view.grid"));
  EXPECT_TRUE(commands.shortcutLabelFor("view.grid").empty());
  ASSERT_EQ(commands.names().size(), 1u);
  EXPECT_EQ(commands.names().front(), "view.grid");
}

TEST(UiFocus, MemoryClipboardCarriesMultipleFormatsAndNotifies) {
  MemoryClipboard clipboard;
  int changes = 0;
  clipboard.setChangeHandler([&changes] { ++changes; });
  EXPECT_TRUE(clipboard.hasChangeHandler());

  EXPECT_TRUE(clipboard.set("text/plain", "hello"));
  EXPECT_EQ(clipboard.getText(), "hello");
  EXPECT_TRUE(clipboard.set("text/html", "<b>hello</b>"));
  EXPECT_TRUE(clipboard.hasFormat("text/html"));
  EXPECT_EQ(clipboard.get("text/html"), "<b>hello</b>");
  EXPECT_EQ(clipboard.formats().size(), 2u);
  EXPECT_EQ(changes, 2);

  EXPECT_TRUE(clipboard.set("text/plain", "bye"));
  EXPECT_EQ(clipboard.getText(), "bye") << "setting an existing format replaces it";
  EXPECT_EQ(clipboard.formats().size(), 2u) << "and does not add a duplicate";
  EXPECT_EQ(clipboard.get("application/x-unknown"), "");

  EXPECT_TRUE(clipboard.setFiles({ "clip.mp4", "b.mp4" }));
  EXPECT_TRUE(clipboard.hasFormat("text/uri-list"));
  const auto files = clipboard.getFiles();
  ASSERT_EQ(files.size(), 2u);
  EXPECT_EQ(files[0], "clip.mp4");
  EXPECT_EQ(files[1], "b.mp4");
  EXPECT_EQ(clipboard.getText(), "bye") << "adding a format keeps the text";
}

TEST(UiFocus, PlatformClipboardNotificationReachesTheSystemHandler) {
  auto clipboard = std::make_shared<MemoryClipboard>();
  int changes = 0;
  clipboard->setChangeHandler([&changes] { ++changes; });
  Clipboard::setSystem(clipboard);

  // This is what a platform backend calls from its native change message (Win32
  // WM_CLIPBOARDUPDATE): the handler runs without the platform layer knowing about observers.
  Clipboard::notifySystemChanged();
  EXPECT_EQ(changes, 1);

  Clipboard::setSystem(nullptr);
}

TEST(UiFocus, MemoryClipboardIgnoresEmptyFormatNames) {
  MemoryClipboard clipboard;
  EXPECT_FALSE(clipboard.set("", "data"));
  EXPECT_TRUE(clipboard.formats().empty());
}

TEST(UiFocus, TextInputSelectsCopiesPastesAndUndoes) {
  const auto font = testFont(16.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Clipboard::setSystem(std::make_shared<MemoryClipboard>());

  TextInput input("hello world", font);
  input.setBounds(Rect::MakeXYWH(0.0f, 0.0f, 240.0f, 32.0f));
  input.setSelection(0, 5);
  EXPECT_TRUE(input.hasSelection());
  EXPECT_EQ(input.selectedText(), "hello");

  input.onKeyDown(keyEvent(Key::C, true));
  EXPECT_EQ(Clipboard::system()->getText(), "hello");
  input.setCursor(input.text().size());
  input.onKeyDown(keyEvent(Key::V, true));
  EXPECT_EQ(input.text(), "hello worldhello");

  input.setSelection(0, 5);
  input.onTextInput(U'H');
  EXPECT_EQ(input.text(), "H worldhello");
  EXPECT_TRUE(input.undo());
  EXPECT_EQ(input.text(), "hello worldhello");
  EXPECT_TRUE(input.redo());
  EXPECT_EQ(input.text(), "H worldhello");

  input.setCursor(input.text().size());
  input.onTextInput(U'!');
  EXPECT_EQ(input.text(), "H worldhello!");
  EXPECT_TRUE(input.undo());
  EXPECT_EQ(input.text(), "H worldhello");

  input.onKeyDown(keyEvent(Key::Home));
  EXPECT_EQ(input.cursor(), 0u);
  input.onKeyDown(keyEvent(Key::End, false, true));
  EXPECT_EQ(input.cursor(), input.text().size());
  EXPECT_EQ(input.selectionStart(), 0u);
}

TEST(UiFocus, TextInputReportsAnImeCursorRectangle) {
  const auto font = testFont(16.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  TextInput input("hello", font);
  input.setBounds(Rect::MakeXYWH(10.0f, 20.0f, 200.0f, 32.0f));
  input.onFocusChanged(true);
  input.setCursor(0);
  const Rect start = input.imeCursorRect();
  EXPECT_GT(start.getHeight(), 0.0f);
  input.setCursor(input.text().size());
  const Rect end = input.imeCursorRect();
  EXPECT_GT(end.getLeft(), start.getLeft());
}

TEST(UiFocus, TextInputAcceptsCjkCompositionAndCommit) {
  const auto font = testFont(16.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  TextInput input("", font);
  input.setBounds(Rect::MakeXYWH(0.0f, 0.0f, 200.0f, 32.0f));
  input.onFocusChanged(true);
  EXPECT_TRUE(input.onTextComposition("ni"));
  EXPECT_EQ(input.composition(), "ni");
  EXPECT_TRUE(input.onTextCommit("\xE4\xBD\xA0"));
  EXPECT_EQ(input.composition(), "");
  EXPECT_EQ(input.text(), "\xE4\xBD\xA0");
}

TEST(UiFocus, NativeClipboardRoundTripsWhenThePlatformProvidesOne) {
  const auto previous = Clipboard::system();
  const std::string previousText = previous->getText();
  Clipboard::setSystem(nullptr);
  const auto native = Clipboard::system();
  native->setText("coral clipboard");
  EXPECT_EQ(native->getText(), "coral clipboard");
  previous->setText(previousText);
  Clipboard::setSystem(previous);
}

TEST(UiFocus, SystemClipboardRoundTripsRichFormats) {
  const auto previous = Clipboard::system();
  const std::string previousText = previous->getText();
  Clipboard::setSystem(nullptr);
  const auto native = Clipboard::system();
  ASSERT_NE(native, nullptr);

  // PNG is a byte stream: the embedded NUL proves the backend moves bytes instead of a C string.
  const std::string png{ '\x89', 'P', 'N', 'G', '\r', '\n', '\x1a', '\n', '\0', '\x01', '\x02' };
  ASSERT_TRUE(native->set("image/png", png));
  EXPECT_EQ(native->get("image/png"), png) << "binary payloads survive NUL bytes";
  const auto withPng = native->formats();
  EXPECT_NE(std::find(withPng.begin(), withPng.end(), "image/png"), withPng.end());

  ASSERT_TRUE(native->set("text/html", "<b>coral</b>"));
  EXPECT_EQ(native->get("text/html"), "<b>coral</b>")
    << "an owner that keeps several representations serves each MIME separately";
  const auto withHtml = native->formats();
  EXPECT_NE(std::find(withHtml.begin(), withHtml.end(), "text/html"), withHtml.end());

  previous->setText(previousText);
  Clipboard::setSystem(previous);
}

TEST(UiFocus, SystemClipboardFallsBackToMemoryWithoutADisplay) {
#if defined(__linux__)
  const auto previous = Clipboard::system();
  const char* display = std::getenv("DISPLAY");
  const char* wayland = std::getenv("WAYLAND_DISPLAY");
  const std::string savedDisplay = display != nullptr ? display : "";
  const std::string savedWayland = wayland != nullptr ? wayland : "";
  const bool hadDisplay = display != nullptr;
  const bool hadWayland = wayland != nullptr;

  unsetenv("DISPLAY");
  unsetenv("WAYLAND_DISPLAY");
  Clipboard::setSystem(nullptr);
  const auto fallback = Clipboard::system();
  ASSERT_NE(fallback, nullptr);
  EXPECT_TRUE(fallback->set("application/x-coral-probe", "probe"))
    << "without a display the in-memory clipboard is installed and keeps private formats";
  EXPECT_EQ(fallback->get("application/x-coral-probe"), "probe");

  Clipboard::setSystem(nullptr);
  if (hadDisplay) setenv("DISPLAY", savedDisplay.c_str(), 1);
  if (hadWayland) setenv("WAYLAND_DISPLAY", savedWayland.c_str(), 1);
  Clipboard::setSystem(previous);
#else
  GTEST_SKIP() << "only the Linux clipboard selects X11/Wayland from the environment";
#endif
}

TEST(UiFocus, MemoryClipboardRoundTripsPngImages) {
  if (!graphics::isEncodeSupported(graphics::ImageFormat::PNG) ||
      !graphics::isDecodeSupported(graphics::ImageFormat::PNG)) {
    GTEST_SKIP() << "this build has no PNG codec";
  }
  MemoryClipboard clipboard;

  graphics::Bitmap bitmap = graphics::Bitmap::allocate(3, 2);
  {
    graphics::Pixmap pixels = bitmap.makePixmap();
    for (int y = 0; y < 2; ++y) {
      for (int x = 0; x < 3; ++x) {
        pixels.setPixelARGB32(x, y, (uint32_t{ 255 } << 24) |
            (static_cast<uint32_t>(x * 60) << 16) | (static_cast<uint32_t>(y * 90) << 8) | 7u);
      }
    }
  }
  const graphics::Image image = graphics::Image::fromBitmap(bitmap);
  ASSERT_TRUE(clipboard.setImage(image)) << "a non-empty image is published as image/png";
  EXPECT_TRUE(clipboard.hasFormat("image/png"));

  const graphics::Image decoded = clipboard.getImage();
  ASSERT_FALSE(decoded.isEmpty()) << "the published PNG decodes back into pixels";
  EXPECT_EQ(decoded.width(), 3);
  EXPECT_EQ(decoded.height(), 2);
  const graphics::Pixmap expected = image.makePixmap();
  const graphics::Pixmap actual = decoded.makePixmap();
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 3; ++x) {
      EXPECT_EQ(actual.getPixelARGB32(x, y), expected.getPixelARGB32(x, y))
        << "pixel " << x << "," << y;
    }
  }

  EXPECT_FALSE(clipboard.setImage(graphics::Image())) << "an empty image is not published";
  MemoryClipboard other;
  EXPECT_TRUE(other.getImage().isEmpty()) << "an absent image reads as empty";
  other.set("image/png", "not a png");
  EXPECT_TRUE(other.getImage().isEmpty()) << "undecodable bytes read as empty, not as garbage";
}

TEST(UiFocus, MemoryClipboardKeepsPrimarySeparateFromTheClipboard) {
  MemoryClipboard clipboard;
  EXPECT_TRUE(clipboard.primaryFormats().empty());
  EXPECT_TRUE(clipboard.getPrimary("text/plain").empty());

  ASSERT_TRUE(clipboard.setPrimary("text/plain", "marked"));
  EXPECT_EQ(clipboard.getPrimary("text/plain"), "marked");
  EXPECT_EQ(clipboard.primaryFormats().size(), 1u);
  EXPECT_TRUE(clipboard.getText().empty()) << "PRIMARY must not leak into the clipboard";
  EXPECT_TRUE(clipboard.formats().empty());

  ASSERT_TRUE(clipboard.set("text/plain", "copied"));
  EXPECT_EQ(clipboard.getText(), "copied");
  EXPECT_EQ(clipboard.getPrimary("text/plain"), "marked")
    << "and a clipboard write must not replace the primary selection";
}

TEST(UiFocus, PlatformsWithoutPrimarySelectionReportAbsence) {
  // The default implementation is what Win32 uses: no primary selection, no errors.
  class ClipboardOnly final : public Clipboard {
   public:
    [[nodiscard]] std::string get(const std::string&) const override {
      return {};
    }
    bool set(const std::string&, std::string) override {
      return false;
    }
    [[nodiscard]] std::vector<std::string> formats() const override {
      return {};
    }
  };
  ClipboardOnly clipboard;
  EXPECT_TRUE(clipboard.getPrimary("text/plain").empty());
  EXPECT_FALSE(clipboard.setPrimary("text/plain", "marked"));
  EXPECT_TRUE(clipboard.primaryFormats().empty());
}

TEST(UiFocus, TextInputPublishesTheMarkedTextToPrimaryAndPastesItOnMiddleClick) {
  const auto font = testFont(16.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto clipboard = std::make_shared<MemoryClipboard>();
  Clipboard::setSystem(clipboard);

  TextInput input("hello world", font);
  input.setBounds(Rect::MakeWH(240.0f, 32.0f));
  input.onFocusChanged(true);

  PointerEvent press;
  press.button = PointerButton::Primary;
  press.position = Point{ 2.0f, 16.0f };
  ASSERT_TRUE(input.onPointerDown(press));
  PointerEvent drag = press;
  drag.position = Point{ 70.0f, 16.0f };
  ASSERT_TRUE(input.onPointerMove(drag));
  PointerEvent release = drag;
  release.button = PointerButton::None;
  ASSERT_TRUE(input.onPointerUp(release));

  const std::string marked = input.selectedText();
  ASSERT_FALSE(marked.empty());
  EXPECT_EQ(clipboard->getPrimary("text/plain"), marked)
    << "releasing a drag publishes the marked text to PRIMARY";

  input.setCursor(input.text().size());
  PointerEvent middle;
  middle.button = PointerButton::Middle;
  middle.position = Point{ 2.0f, 16.0f };
  ASSERT_TRUE(input.onPointerDown(middle));
  EXPECT_EQ(input.text(), "hello world" + marked)
    << "a middle click pastes the PRIMARY selection at the cursor";
}

TEST(UiFocus, CaretBlinksAndStaysSolidDuringComposition) {
  const auto font = testFont(16.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  TextInput input("ab", font);
  input.setBounds(Rect::MakeWH(200.0f, 32.0f));
  input.onFocusChanged(true);
  input.setCaretBlinkInterval(std::chrono::milliseconds(100));

  const auto start = std::chrono::steady_clock::now();
  EXPECT_TRUE(input.caretVisibleAt(start)) << "a freshly focused caret is visible";
  EXPECT_FALSE(input.caretVisibleAt(start + std::chrono::milliseconds(100)));
  EXPECT_TRUE(input.caretVisibleAt(start + std::chrono::milliseconds(200)));

  input.onTextComposition("zh");
  EXPECT_TRUE(input.caretVisibleAt(start + std::chrono::milliseconds(300)))
    << "the preedit must stay readable while composing";
  input.onTextComposition(std::string());

  input.setCaretBlinkInterval(std::chrono::milliseconds(0));
  EXPECT_TRUE(input.caretVisibleAt(start + std::chrono::hours(1)))
    << "a zero interval disables blinking";

  input.onFocusChanged(false);
  EXPECT_FALSE(input.caretVisibleAt(std::chrono::steady_clock::now()));
}

TEST(UiFocus, SystemChangeCountTracksExternalClipboardWrites) {
#if defined(_WIN32)
  const uint64_t before = Clipboard::systemChangeCount();
  ASSERT_TRUE(::OpenClipboard(nullptr));
  ::EmptyClipboard();
  const wchar_t* text = L"coral-system-change";
  const size_t bytes = (wcslen(text) + 1) * sizeof(wchar_t);
  HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
  ASSERT_NE(memory, nullptr);
  if (void* target = ::GlobalLock(memory)) {
    std::memcpy(target, text, bytes);
    ::GlobalUnlock(memory);
  }
  ASSERT_NE(::SetClipboardData(CF_UNICODETEXT, memory), nullptr);
  ::CloseClipboard();
  const uint64_t after = Clipboard::systemChangeCount();
  EXPECT_GT(after, before) << "a write from another path bumps the sequence number";
#else
  GTEST_SKIP() << "the platform has no clipboard sequence number";
#endif
}

TEST(UiFocus, SystemClipboardRoundTripsPngBytes) {
#if defined(_WIN32)
  const auto clipboard = Clipboard::system();
  ASSERT_NE(clipboard, nullptr);
  const std::string png{ '\x89', 'P', 'N', 'G', '\r', '\n', '\x1a', '\n', '\0', '\x01', '\x02' };
  clipboard->set("image/png", png);
  EXPECT_EQ(clipboard->get("image/png"), png) << "binary payloads survive NUL bytes";
  const auto formats = clipboard->formats();
  EXPECT_NE(std::find(formats.begin(), formats.end(), "image/png"), formats.end());
#else
  GTEST_SKIP() << "the Win32 PNG format is not available on this platform";
#endif
}