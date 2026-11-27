/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include "TestSupport.h"

#include <ui/ColorLayer.h>
#include <ui/Window.h>

#include <graphics/Bitmap.h>
#include <graphics/Canvas.h>
#include <graphics/Image.h>
#include <graphics/Paint.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef CORAL_GOLDEN_DIR
#define CORAL_GOLDEN_DIR "."
#endif

namespace {

std::shared_ptr<coral::ui::Window> makeWindow(int width, int height) {
  coral::ui::Window::Options options;
  options.headless = true;
  options.width = width;
  options.height = height;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  return coral::ui::Window::create(options);
}

/**
 * A deterministic scene with a rect, a rounded clip and a translated child. Text is deliberately
 * absent: glyph rasterisation is allowed to differ by a pixel between drivers, while these shapes
 * exercise the same blend/clip/layer path a golden image is meant to protect.
 */
graphics::Bitmap renderScene(int width, int height) {
  auto window = makeWindow(width, height);
  if (window == nullptr) return {};
  auto root = std::make_shared<coral::ui::ColorLayer>(graphics::Color::MakeRGBA8(24, 26, 32, 255));
  auto panel = std::make_shared<coral::ui::ColorLayer>(graphics::Color::MakeRGBA8(56, 132, 220, 255));
  panel->setBounds(coral::ui::Rect::MakeLTRB(8.0f, 8.0f, width - 8.0f, height - 8.0f));
  panel->setClipRadius(7.0f);
  auto accent = std::make_shared<coral::ui::ColorLayer>(graphics::Color::MakeRGBA8(240, 180, 64, 255));
  accent->setBounds(coral::ui::Rect::MakeLTRB(18.0f, 18.0f, 42.0f, 42.0f));
  accent->setTranslation({ 3.0f, 2.0f });
  panel->addChild(accent);
  root->addChild(panel);
  window->setRoot(root);
  if (!window->renderFrame()) return {};
  graphics::Bitmap bitmap;
  if (!window->snapshot(&bitmap)) return {};
  return bitmap;
}

std::vector<uint8_t> readFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.good()) return {};
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void writeFile(const std::string& path, const graphics::Data& bytes) {
  std::filesystem::create_directories(std::filesystem::path(path).parent_path());
  std::ofstream file(path, std::ios::binary);
  ASSERT_TRUE(file.good()) << "could not open " << path;
  file.write(reinterpret_cast<const char*>(bytes.bytes()), static_cast<std::streamsize>(bytes.size()));
  ASSERT_TRUE(file.good()) << "could not write " << path;
}

} // namespace

TEST(UiWindow, AFrameMatchesItsGoldenImage) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  constexpr int kWidth = 64;
  constexpr int kHeight = 64;
  const graphics::Bitmap actual = renderScene(kWidth, kHeight);
  if (actual.isEmpty()) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  const std::string path = std::string(CORAL_GOLDEN_DIR) + "/window_scene.png";
  if (std::getenv("CORAL_UPDATE_GOLDEN") != nullptr) {
    const char* output = std::getenv("CORAL_GOLDEN_OUTPUT");
    std::string error;
    const graphics::Data encoded = graphics::Image::fromBitmap(actual).encode(
      graphics::ImageFormat::PNG, 100, &error);
    ASSERT_GT(encoded.size(), 0) << error;
    writeFile(output != nullptr ? output : path, encoded);
    return;
  }

  const std::vector<uint8_t> bytes = readFile(path);
  if (bytes.empty()) {
    GTEST_SKIP() << "golden image is missing: " << path
                 << " (run with CORAL_UPDATE_GOLDEN=1 to create it)";
  }
  std::string error;
  const graphics::Image expected = graphics::Image::decode(
    graphics::Data::makeCopy(bytes.data(), bytes.size()), &error);
  ASSERT_FALSE(expected.isEmpty()) << error;
  ASSERT_EQ(expected.width(), kWidth);
  ASSERT_EQ(expected.height(), kHeight);
  const graphics::Pixmap expectedPixels = expected.makePixmap();
  const graphics::Pixmap actualPixels = actual.makePixmap();

  // GPU blending and the rounded mask are allowed to differ by a few units between drivers; a
  // structural regression (wrong layer order, missing mask, offset) is far larger than this.
  constexpr int kTolerance = 12;
  int mismatches = 0;
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      const uint32_t a = actualPixels.getPixelARGB32(x, y);
      const uint32_t e = expectedPixels.getPixelARGB32(x, y);
      for (int shift = 0; shift < 32; shift += 8) {
        if (std::abs(static_cast<int>((a >> shift) & 0xFFu) -
                     static_cast<int>((e >> shift) & 0xFFu)) > kTolerance) {
          ++mismatches;
          break;
        }
      }
    }
  }
  EXPECT_LE(mismatches, (kWidth * kHeight) / 200)
    << "render differs from " << path;
}

TEST(UiWindow, AGpuFrameMatchesTheRasterReference) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  auto window = makeWindow(48, 48);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<coral::ui::ColorLayer>(graphics::Color::MakeRGBA8(20, 40, 80, 255));
  auto child = std::make_shared<coral::ui::ColorLayer>(graphics::Color::MakeRGBA8(200, 80, 40, 255));
  child->setBounds(coral::ui::Rect::MakeLTRB(8.0f, 8.0f, 32.0f, 32.0f));
  root->addChild(child);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());
  graphics::Bitmap frame;
  ASSERT_TRUE(window->snapshot(&frame));

  graphics::Bitmap reference;
  reference = graphics::Bitmap::allocate(48, 48);
  auto canvas = graphics::Canvas::create(reference);
  ASSERT_NE(canvas, nullptr);
  graphics::Paint background;
  background.color = graphics::Color::MakeRGBA8(20, 40, 80, 255);
  canvas->drawRect(coral::ui::Rect::MakeWH(48.0f, 48.0f), background);
  graphics::Paint foreground;
  foreground.color = graphics::Color::MakeRGBA8(200, 80, 40, 255);
  canvas->drawRect(coral::ui::Rect::MakeLTRB(8.0f, 8.0f, 32.0f, 32.0f), foreground);

  const graphics::Pixmap a = frame.makePixmap();
  const graphics::Pixmap b = reference.makePixmap();
  int mismatches = 0;
  for (int y = 0; y < 48; ++y) {
    for (int x = 0; x < 48; ++x) {
      const uint32_t pa = a.getPixelARGB32(x, y);
      const uint32_t pb = b.getPixelARGB32(x, y);
      for (int shift = 0; shift < 32; shift += 8) {
        if (std::abs(static_cast<int>((pa >> shift) & 0xFFu) -
                     static_cast<int>((pb >> shift) & 0xFFu)) > 1) {
          ++mismatches;
          break;
        }
      }
    }
  }
  EXPECT_EQ(mismatches, 0) << "GPU and raster disagree on a flat rect scene";
}


