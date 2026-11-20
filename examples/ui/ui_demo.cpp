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

/**
 * @file ui_demo.cpp
 *
 * A small showcase: a native window (or `--headless` offscreen surface) with a flex column, solid
 * panels, text and one focusable button. It is deliberately built only from the public `ui` layer
 * primitives -- no editor code -- so it doubles as the manual smoke test for the window, layout,
 * input and present path.
 */

#include <ui/Button.h>
#include <ui/ColorLayer.h>
#include <ui/Flex.h>
#include <ui/ScrollView.h>
#include <ui/TextInput.h>
#include <ui/TextLayer.h>
#include <ui/TitleBar.h>
#include <ui/Theme.h>
#include <ui/Window.h>

#include <graphics/Data.h>
#include <graphics/Font.h>
#include <graphics/Image.h>
#include <graphics/Paint.h>
#include <graphics/Typeface.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

namespace {

using namespace coral::ui;


bool writePng(const graphics::Bitmap& bitmap, const std::string& path) {
  if (bitmap.isEmpty()) return false;
  std::string error;
  const graphics::Data encoded = graphics::Image::fromBitmap(bitmap).encode(
    graphics::ImageFormat::PNG, 100, &error);
  if (encoded.size() == 0) {
    std::fprintf(stderr, "could not encode the UI frame: %s\n", error.c_str());
    return false;
  }
  std::ofstream stream(path, std::ios::binary);
  if (!stream.is_open()) return false;
  stream.write(reinterpret_cast<const char*>(encoded.bytes()),
    static_cast<std::streamsize>(encoded.size()));
  return stream.good();
}

} // namespace anonymous

const char* platformName(coral::ui::Window::NativePlatform platform);

int main(int argc, char** argv) {
  bool headless = false;
  int frames = 0;
  std::string output;
  int width = 960;
  int height = 540;
  Window::NativePlatform platform = Window::NativePlatform::Auto;
  float uiScale = 0.0f;
  bool focusInput = false;
  bool threadedHost = false;
  std::string themeName = "dark";
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    const auto next = [&]() -> std::string { return index + 1 < argc ? argv[++index] : std::string(); };
    if (argument == "--headless") {
      headless = true;
    } else if (argument == "--frames") {
      frames = std::max(1, std::atoi(next().c_str()));
    } else if (argument == "--out") {
      output = next();
    } else if (argument == "--width") {
      width = std::max(320, std::atoi(next().c_str()));
    } else if (argument == "--height") {
      height = std::max(240, std::atoi(next().c_str()));
    } else if (argument == "--platform") {
      const std::string value = next();
      platform = value == "wayland" ? Window::NativePlatform::Wayland
        : (value == "x11" ? Window::NativePlatform::X11 : Window::NativePlatform::Auto);
    } else if (argument == "--focus-input") {
      focusInput = true;
    } else if (argument == "--render-host") {
      const std::string value = next();
      threadedHost = value == "render-thread" || value == "threaded";
    } else if (argument == "--scale") {
      uiScale = std::max(0.0f, static_cast<float>(std::atof(next().c_str())));
    } else if (argument == "--theme") {
      themeName = next();
    } else if (argument == "--help") {
      std::printf("usage: ui_demo [--headless] [--frames N] [--out FILE] [--width W] [--height H] "
        "[--platform auto|x11|wayland] [--render-host same-thread|render-thread] "
        "[--scale S] [--theme dark|light] [--focus-input]\n");
      return 0;
    }
  }

  Window::Options options;
  options.width = width;
  options.height = height;
  options.headless = headless;
  options.title = "Coral UI";
  options.platform = platform;
  options.uiScale = uiScale;
  options.renderHost = threadedHost ? RenderHostKind::RenderThread : RenderHostKind::SameThread;
  options.frameLimit = headless && frames == 0 ? 1 : frames;
  auto window = Window::create(options);
  if (window == nullptr) {
    std::fprintf(stderr, "the UI window could not be created: %s\n",
      Window::lastCreationError().c_str());
    return 1;
  }

  const auto typeface = graphics::Typeface::fromFile(std::string(CORAL_ASSETS_DIR) +
    "/fonts/NotoSansSC-Regular.otf");
  Theme theme = themeName == "light" ? Theme::light() : Theme::dark();
  if (typeface != nullptr) {
    theme.body = graphics::Font::create(typeface, 14.0f);
    theme.title = graphics::Font::create(typeface, 24.0f);
  }
  window->setTheme(theme);

  auto root = std::make_shared<Flex>(Axis::Column);
  root->setGap(0.0f);
  root->setCrossAlign(Align::Stretch);
  std::shared_ptr<TextInput> inputField;

  if (!headless) {
    // GNOME's Wayland compositor does not implement xdg-decoration. Drawing the frame in the
    // product is the supported path (the same one Windows uses); the title bar is placed at y = 0 so
    // `Window::hitTest` maps its caption band and buttons onto the native move/maximize/minimize.
    auto titleBar = std::make_shared<TitleBar>("Coral UI", theme.body, theme.text);
    titleBar->setOnMinimize([w = window.get()] { w->minimize(); });
    titleBar->setOnMaximize([w = window.get()] {
      if (w->isMaximized()) w->restore(); else w->maximize();
    });
    titleBar->setOnClose([w = window.get()] { w->requestClose(); });
    root->addChild(titleBar);
    window->setDecorations(Window::Decorations::Client);
    window->setChromeMetrics({ .titleBarHeight = 40.0f });
  }

  auto page = std::make_shared<Flex>(Axis::Column);
  page->setPadding({ 24.0f, 24.0f, 24.0f, 24.0f });
  page->setGap(16.0f);
  page->setCrossAlign(Align::Stretch);
  root->addChild(page);

  if (theme.title != nullptr) {
    auto title = std::make_shared<TextLayer>("Coral UI", theme.title, theme.text);
    page->addChild(title);
  }
  if (theme.body != nullptr) {
    auto subtitle = std::make_shared<TextLayer>("layers, flex, wrapping, widgets, scroll",
      theme.body, theme.textMuted);
    page->addChild(subtitle);
  }

  auto toolbar = std::make_shared<ColorLayer>(theme.surface);
  toolbar->setBounds(Rect::MakeWH(static_cast<float>(width - 48), 56.0f));
  page->addChild(toolbar);

  auto button = std::make_shared<Button>("Focusable button", theme.body);
  page->addChild(button);

  auto content = std::make_shared<ColorLayer>(theme.surfaceRaised);
  content->setBounds(Rect::MakeWH(static_cast<float>(width - 48), 120.0f));
  page->addChild(content);

  if (theme.body != nullptr) {
    auto input = std::make_shared<TextInput>("", theme.body);
    input->setPlaceholder("type here - focus with a click, edit with the keyboard");
    input->setPreferredWidth(static_cast<float>(width - 48));
    page->addChild(input);
    inputField = input;

    auto listContent = std::make_shared<Flex>(Axis::Column);
    listContent->setGap(2.0f);
    listContent->setCrossAlign(Align::Stretch);
    for (int row = 0; row < 8; ++row) {
      auto entry = std::make_shared<ColorLayer>(
        row % 2 == 0 ? theme.surface : theme.surfaceRaised);
      entry->setBounds(Rect::MakeWH(static_cast<float>(width - 64), 28.0f));
      listContent->addChild(entry);
    }
    auto list = std::make_shared<ScrollView>();
    list->setPreferredHeight(100.0f);
    list->setBounds(Rect::MakeWH(static_cast<float>(width - 48), 100.0f));
    list->setContent(listContent);
    page->addChild(list);
  }

  if (theme.body != nullptr) {
    auto footer = std::make_shared<TextLayer>(headless ? "headless frame" : "native window - Esc closes",
      theme.body, theme.textMuted);
    page->addChild(footer);
  }

  window->setRoot(root);
  if (focusInput && inputField != nullptr) {
    window->focusLayer(inputField.get());
  }
  const int rendered = window->run();
  if (rendered == 0) {
    std::fprintf(stderr, "the UI window never rendered: %s\n", window->diagnostics().c_str());
    return 1;
  }
  if (!headless && !window->isRenderTargetOnScreen()) {
    std::fprintf(stderr, "the render target is offscreen; frames and snapshots work but no window "
                         "presents them\n");
  }
  if (!window->diagnostics().empty()) {
    std::fprintf(stderr, "window diagnostics: %s\n", window->diagnostics().c_str());
  }

  if (!output.empty()) {
    graphics::Bitmap bitmap;
    if (!window->snapshot(&bitmap) || !writePng(bitmap, output)) {
      return 1;
    }
  }
  std::printf("%s %d frame(s), refresh %.1f Hz, platform=%s, dpr=%.2f, uiScale=%.2f\n",
    headless ? "headless" : "window", rendered, window->refreshHertz(),
    platformName(window->nativePlatform()), window->devicePixelRatio(), window->uiScale());
  return 0;
}

const char* platformName(const coral::ui::Window::NativePlatform platform) {
  switch (platform) {
    case coral::ui::Window::NativePlatform::Win32: return "win32";
    case coral::ui::Window::NativePlatform::X11: return "x11";
    case coral::ui::Window::NativePlatform::Wayland: return "wayland";
    case coral::ui::Window::NativePlatform::Auto: return "headless";
  }
  return "unknown";
}
