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

#include <ui/ColorLayer.h>
#include <ui/DSL.h>
#include <ui/Reconciler.h>
#include <ui/Window.h>

#include <graphics/Bitmap.h>
#include <graphics/Image.h>
#include <graphics/Path.h>
#include <graphics/Paint.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef CORAL_GOLDEN_DIR
#define CORAL_GOLDEN_DIR "."
#endif

namespace {

using coral::ui::ColorLayer;
using coral::ui::Rect;
using coral::ui::Window;

struct GoldenScene {
  const char* file = nullptr;
  int width = 64;
  int height = 64;
  float uiScale = 1.0f;
  void (*build)(Window&) = nullptr;
};

std::shared_ptr<ColorLayer> layer(const uint32_t argb8, const Rect& bounds = {}) {
  auto result = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(
    static_cast<uint8_t>((argb8 >> 16) & 0xFFu), static_cast<uint8_t>((argb8 >> 8) & 0xFFu),
    static_cast<uint8_t>(argb8 & 0xFFu), static_cast<uint8_t>((argb8 >> 24) & 0xFFu)));
  if (bounds.getWidth() > 0.0f || bounds.getHeight() > 0.0f) result->setBounds(bounds);
  return result;
}

/** The original scene: a rounded panel, a translated child and the clear colour. */
void buildLayers(Window& window) {
  auto root = layer(0xFF181A20u, Rect::MakeWH(64.0f, 64.0f));
  auto panel = layer(0xFF3884DCu, Rect::MakeLTRB(8.0f, 8.0f, 56.0f, 56.0f));
  panel->setClipRadius(7.0f);
  auto accent = layer(0xFFF0B440u, Rect::MakeLTRB(18.0f, 18.0f, 42.0f, 42.0f));
  accent->setTranslation({ 3.0f, 2.0f });
  panel->addChild(accent);
  root->addChild(panel);
  window.setRoot(root);
}

/** A multiply child isolates a layer and composites the overlapping pixels once. */
void buildBlend(Window& window) {
  auto root = layer(0xFF0000FFu, Rect::MakeWH(64.0f, 64.0f));
  auto child = layer(0xFFFF0000u, Rect::MakeLTRB(16.0f, 16.0f, 48.0f, 48.0f));
  child->setBlendMode(graphics::BlendMode::kMultiply);
  root->addChild(child);
  window.setRoot(root);
}

/** A shaped clip exercises the mask-composited layer route. */
void buildPathClip(Window& window) {
  auto root = layer(0xFF0000FFu, Rect::MakeWH(64.0f, 64.0f));
  auto child = layer(0xFFFF0000u, Rect::MakeWH(64.0f, 64.0f));
  child->setClip(true);
  graphics::Path clip;
  clip.moveTo({ 32.0f, 8.0f });
  clip.lineTo({ 56.0f, 32.0f });
  clip.lineTo({ 32.0f, 56.0f });
  clip.lineTo({ 8.0f, 32.0f });
  clip.close();
  child->setClipPath(clip);
  root->addChild(child);
  window.setRoot(root);
}

/** Group opacity is a saveLayer path, not a per-child alpha multiply. */
void buildOpacity(Window& window) {
  auto root = layer(0xFF101418u, Rect::MakeWH(64.0f, 64.0f));
  auto panel = layer(0xFF3884DCu, Rect::MakeLTRB(8.0f, 8.0f, 56.0f, 56.0f));
  panel->setOpacity(0.5f);
  auto accent = layer(0xFFFF0000u, Rect::MakeLTRB(20.0f, 20.0f, 44.0f, 44.0f));
  panel->addChild(accent);
  root->addChild(panel);
  window.setRoot(root);
}

/** A 2x window: the tree is laid out in 32 logical units and drawn at 64 device pixels. */
void buildDpi(Window& window) {
  auto root = layer(0xFF181A20u, Rect::MakeWH(32.0f, 32.0f));
  auto panel = layer(0xFF3884DCu, Rect::MakeLTRB(4.0f, 4.0f, 28.0f, 28.0f));
  auto accent = layer(0xFFF0B440u, Rect::MakeLTRB(8.0f, 8.0f, 20.0f, 20.0f));
  panel->addChild(accent);
  root->addChild(panel);
  window.setRoot(root);
}

/** A DSL Panel and keyed rows: the golden covers the DSL-to-Reconciler path at two scales. */
void buildDslPanel(Window& window) {
  coral::ui::Theme theme = coral::ui::Theme::dark();
  coral::ui::dsl::ThemeScope scope(theme);
  coral::ui::Reconciler reconciler;
  auto root = reconciler.mount(coral::ui::dsl::Column(
    coral::ui::dsl::StackOptions{ .gap = 4.0f, .padding = coral::ui::Insets{}.all(4.0f),
      .key = "golden.root" },
    coral::ui::dsl::Panel(coral::ui::dsl::PanelOptions{ .padding = 6.0f, .gap = 4.0f,
      .key = "golden.panel" },
      coral::ui::dsl::Swatch(graphics::Color::MakeRGBA8(56, 132, 220, 255), 40.0f, 20.0f, 3.0f,
        "golden.swatch"),
      coral::ui::dsl::Swatch(graphics::Color::MakeRGBA8(240, 180, 64, 255), 20.0f, 20.0f, 3.0f,
        "golden.accent")),
    coral::ui::dsl::Body("DSL", "golden.text")));
  window.setRoot(std::move(root));
}
const GoldenScene kScenes[] = {
  { "window_scene_blend.png", 64, 64, 1.0f, buildBlend },
  { "window_scene_path_clip.png", 64, 64, 1.0f, buildPathClip },
  { "window_scene_opacity.png", 64, 64, 1.0f, buildOpacity },
  { "window_scene_dpi2.png", 64, 64, 2.0f, buildDpi },
  { "dsl_panel.png", 64, 64, 1.0f, buildDslPanel },
  { "dsl_panel_dpi15.png", 64, 64, 1.5f, buildDslPanel },
};

graphics::Bitmap renderScene(const GoldenScene& scene) {
  Window::Options options;
  options.headless = true;
  options.width = scene.width;
  options.height = scene.height;
  options.frameLimit = 1;
  options.uiScale = scene.uiScale;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) return {};
  scene.build(*window);
  if (!window->renderFrame()) return {};
  graphics::Bitmap bitmap;
  if (!window->snapshot(&bitmap)) return {};
  return bitmap;
}

std::string goldenDirectory() {
  const char* output = std::getenv("CORAL_GOLDEN_OUTPUT_DIR");
  if (output != nullptr && output[0] != '\0') return output;
  return CORAL_GOLDEN_DIR;
}

std::vector<uint8_t> readFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.good()) return {};
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool writeFile(const std::string& path, const graphics::Data& bytes) {
  std::error_code error;
  std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);
  if (error) {
    std::fprintf(stderr, "golden: cannot create %s: %s\n", path.c_str(), error.message().c_str());
    return false;
  }
  std::ofstream file;
  file.open(path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    std::fprintf(stderr, "golden: cannot open %s\n", path.c_str());
    return false;
  }
  file.write(reinterpret_cast<const char*>(bytes.bytes()), static_cast<std::streamsize>(bytes.size()));
  if (!file.good()) {
    std::fprintf(stderr, "golden: cannot write %s (%zu bytes)\n", path.c_str(), bytes.size());
    return false;
  }
  return true;
}

/** Compares two pixmaps channel-wise, allowing a small driver-dependent delta. */
int countMismatches(const graphics::Pixmap& actual, const graphics::Pixmap& expected) {
  if (!actual.isValid() || !expected.isValid()) return -1;
  constexpr int kTolerance = 12;
  int mismatches = 0;
  for (int y = 0; y < expected.height(); ++y) {
    for (int x = 0; x < expected.width(); ++x) {
      const uint32_t pa = actual.getPixelARGB32(x, y);
      const uint32_t pe = expected.getPixelARGB32(x, y);
      for (int shift = 0; shift < 32; shift += 8) {
        if (std::abs(static_cast<int>((pa >> shift) & 0xFFu) -
                     static_cast<int>((pe >> shift) & 0xFFu)) > kTolerance) {
          ++mismatches;
          break;
        }
      }
    }
  }
  return mismatches;
}

} // namespace

TEST(UiGolden, TheExtraScenesMatchTheirGoldenImages) {
  const bool update = std::getenv("CORAL_UPDATE_GOLDEN") != nullptr;
  const std::string directory = goldenDirectory();

  for (const GoldenScene& scene: kScenes) {
    SCOPED_TRACE(scene.file);
    const graphics::Bitmap actual = renderScene(scene);
    if (actual.isEmpty()) {
      GTEST_SKIP() << "no GPU device is available for the headless surface";
    }

    const std::string path = directory + "/" + scene.file;
    if (update) {
      std::string error;
      const graphics::Data encoded = graphics::Image::fromBitmap(actual).encode(
        graphics::ImageFormat::PNG, 100, &error);
      ASSERT_GT(encoded.size(), 0) << error;
      ASSERT_TRUE(writeFile(path, encoded)) << "could not write " << path;
      continue;
    }

    const std::vector<uint8_t> bytes = readFile(path);
    if (bytes.empty()) {
      GTEST_SKIP() << "golden image is missing: " << path
                   << " (run with CORAL_UPDATE_GOLDEN=1 to create it)";
    }
    std::string error;
    const graphics::Image expected = graphics::Image::decode(
      graphics::Data::makeCopy(bytes.data(), bytes.size()), &error);
    ASSERT_FALSE(expected.isEmpty()) << error << ": " << path;
    ASSERT_EQ(expected.width(), scene.width);
    ASSERT_EQ(expected.height(), scene.height);
    const int mismatches = countMismatches(actual.makePixmap(), expected.makePixmap());
    ASSERT_GE(mismatches, 0) << "could not lock the golden or actual pixels";
    EXPECT_LE(mismatches, (scene.width * scene.height) / 200)
      << "scene differs from " << path << " in " << mismatches << " pixels";
  }
}
