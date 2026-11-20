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

#include <ui/TitleBar.h>
#include <ui/Window.h>

#include <graphics/Font.h>
#include <graphics/Typeface.h>

#include <memory>
#include <string>

namespace {

std::shared_ptr<const graphics::Font> testFont() {
  const auto typeface = graphics::Typeface::fromFile(
    std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf");
  if (typeface == nullptr) return nullptr;
  return graphics::Font::create(typeface, 14.0f);
}

} // namespace

TEST(UiTitleBar, MeasuresAFixedChromeHeightAndKeepsItsTitle) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::TitleBar bar("Coral Player", font);
  EXPECT_EQ(bar.title(), "Coral Player");
  bar.setTitle("Edited");
  EXPECT_EQ(bar.title(), "Edited");
  const coral::ui::Size size = bar.measure(coral::ui::Size{ 400.0f, 200.0f });
  EXPECT_FLOAT_EQ(size.width, 400.0f);
  EXPECT_GT(size.height, 0.0f);
}

TEST(UiTitleBar, CloseButtonDispatchesItsCallbackThroughAWindow) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  coral::ui::Window::Options options;
  options.headless = true;
  options.rendererBackend = coral::ui::RendererBackend::Raster;
  options.width = 400;
  options.height = 200;
  auto window = coral::ui::Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no raster window is available";
  auto bar = std::make_shared<coral::ui::TitleBar>("Title", font);
  bool closed = false;
  bar->setOnClose([&closed] { closed = true; });
  window->setRoot(bar);
  window->setDecorations(coral::ui::Window::Decorations::Client);
  window->setChromeMetrics({ .titleBarHeight = 40.0f });
  window->renderFrame();
  coral::ui::PointerEvent down;
  down.position = { 380.0f, 20.0f };
  down.button = coral::ui::PointerButton::Primary;
  window->pointerDown(down);
  window->pointerUp(down);
  EXPECT_TRUE(closed);
}
