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
 * A repeatable text-rendering micro-benchmark.
 *
 * It uses the real GPU recording path, so the reported image draw-call count is what the renderer actually
 * submitted. All labels share one text by default; `--unique` gives every label a distinct string so the
 * atlas still shares glyph cells but the shaped text and command count differ.
 */

#include <ui/Layer.h>
#include <ui/RenderContext.h>
#include <ui/TextLayer.h>
#include <ui/TextRendererStats.h>
#include <ui/Theme.h>
#include <ui/CanvasBackend.h>

#include <graphics/Canvas.h>
#include <graphics/Color.h>
#include <graphics/Font.h>
#include <graphics/GlyphAtlas.h>
#include <graphics/Typeface.h>
#include <gpu/Surface.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

int main(int argc, char** argv) {
  int labels = 200;
  int frames = 10;
  int inFlight = 1;
  int plainSize = 1024;
  bool unique = false;
  bool verbose = false;
  bool plainImage = false;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--labels" && index + 1 < argc) labels = std::max(0, std::atoi(argv[++index]));
    else if (argument == "--frames" && index + 1 < argc) frames = std::max(1, std::atoi(argv[++index]));
    else if (argument == "--in-flight" && index + 1 < argc)
      inFlight = std::clamp(std::atoi(argv[++index]), 1, 8);
    else if (argument == "--unique") unique = true;
    else if (argument == "--verbose") verbose = true;
    else if (argument == "--plain-image") plainImage = true;
    else if (argument == "--plain-size" && index + 1 < argc)
      plainSize = std::clamp(std::atoi(argv[++index]), 1, 4096);
    else if (argument == "--help") {
      std::printf("usage: text_benchmark [--labels N] [--frames N] [--in-flight N] [--unique] "
                  "[--verbose] [--plain-image]\n");
      return 0;
    }
  }

  const auto typeface = graphics::Typeface::fromFile(std::string(CORAL_ASSETS_DIR) +
    "/fonts/NotoSansSC-Regular.otf");
  if (typeface == nullptr) {
    std::fprintf(stderr, "the UI font could not be loaded\n");
    return 1;
  }
  const auto font = graphics::Font::create(typeface, 14.0f);

  graphics::Bitmap plain;
  if (plainImage) {
    plain = graphics::Bitmap::allocate(plainSize, plainSize);
    graphics::Pixmap pixels = plain.lockPixels();
    for (int y = 0; y < plainSize; ++y) {
      for (int x = 0; x < plainSize; ++x) pixels.setPixelARGB32(x, y, 0xFFFFFFFFu);
    }
  }

  graphics::gpu::SurfaceConfiguration configuration;
  configuration.width = 1280;
  configuration.height = 720;
  configuration.maxFramesInFlight = static_cast<uint32_t>(inFlight);
  auto surface = graphics::gpu::Surface::make(configuration);
  if (surface == nullptr) {
    std::fprintf(stderr, "the GPU surface could not be created\n");
    return 1;
  }

  auto root = std::make_shared<coral::ui::Layer>();
  root->setBounds(coral::ui::Rect::MakeWH(1280.0f, 720.0f));
  const int columns = 4;
  for (int index = 0; index < labels; ++index) {
    const std::string text = unique ? "Shared text " + std::to_string(index) : "Shared text";
    auto label = std::make_shared<coral::ui::TextLayer>(text, font,
      graphics::Color::MakeRGBA8(226, 232, 240, 255));
    label->setBounds(coral::ui::Rect::MakeXYWH(20.0f + static_cast<float>(index % columns) * 300.0f,
      20.0f + static_cast<float>(index / columns) * 28.0f, 280.0f, 24.0f));
    root->addChild(label);
  }

  coral::ui::resetTextRendererStats();
  const auto start = std::chrono::steady_clock::now();
  bool submitted = true;
  for (int frame = 0; frame < frames && submitted; ++frame) {
    const auto frameStart = std::chrono::steady_clock::now();
    graphics::Canvas* canvas = surface->beginFrame();
    if (canvas == nullptr) {
      submitted = false;
      break;
    }
    canvas->clear(graphics::Color::MakeRGBA8(24, 26, 32, 255));
    if (plainImage) {
      // The same route as a glyph: one image command, one small texture. It isolates "a textured
      // draw" from the glyph atlas without changing anything else in the frame.
      graphics::Paint paint;
      paint.color = graphics::Color::White();
      paint.style = graphics::Paint::Style::kFill;
      // An atlas draw is a *sub-rectangle* of a large page, not a whole small bitmap.
      canvas->drawBitmapRect(plain, graphics::Rect::MakeXYWH(0.0f, 0.0f, 16.0f, 16.0f),
        graphics::Rect::MakeXYWH(10.0f, 10.0f, 16.0f, 16.0f), paint);
    } else {
      coral::ui::Canvas uiCanvas = coral::ui::detail::makeCanvas(*canvas);
      coral::ui::RenderContext context(uiCanvas, 1.0f);
      root->draw(context);
    }
    const auto drawEnd = std::chrono::steady_clock::now();
    submitted = surface->submit();
    const auto submitEnd = std::chrono::steady_clock::now();
    if (verbose) {
      std::printf("frame %d: draw=%.3f submit=%.3f precompile=%.3f render=%.3f ms "
                  "draws=%u images=%u upload_bytes=%llu textures=%llu fields=%.3f ms\n", frame,
        std::chrono::duration<double, std::milli>(drawEnd - frameStart).count(),
        std::chrono::duration<double, std::milli>(submitEnd - drawEnd).count(),
        surface->lastFramePrecompileMilliseconds(), surface->lastFrameRenderMilliseconds(),
        surface->lastFrameDrawCallCount(), surface->lastFrameImageBatchCount(),
        static_cast<unsigned long long>(surface->lastFrameUploadBytes()),
        static_cast<unsigned long long>(surface->textureUploadCount()),
        surface->lastFrameCoverageFieldMilliseconds());
    }
  }
  const auto end = std::chrono::steady_clock::now();
  if (!submitted) {
    std::fprintf(stderr, "the GPU frame was refused: %s\n", surface->diagnostics().c_str());
    return 1;
  }
  const double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
  const auto stats = coral::ui::textRendererStats();
  const size_t atlasBytes = graphics::GlyphAtlas::instance().bytes();
  std::printf("text_benchmark labels=%d frames=%d unique=%d elapsed_ms=%.1f "
              "atlas_glyphs=%zu atlas_bytes=%zu image_draws=%u total_draws=%u\n",
    labels, frames, unique ? 1 : 0, elapsedMs, stats.atlasGlyphs, atlasBytes,
    surface->lastFrameImageDrawCallCount(), surface->lastFrameDrawCallCount());
  return 0;
}
