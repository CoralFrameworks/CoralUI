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

/**
 * Windows ClearType reference comparison.
 *
 * The same HarfBuzz-shaped glyph run is rendered twice: through the Coral UI pipeline
 * (`TextLayer` -> `TextPainter` -> `Paint::channelMask`) and through DirectWrite's
 * `IDWriteBitmapRenderTarget::DrawGlyphRun`, which is the platform reference. Both use an opaque
 * black background and white text, at 9/10/11/12 px and 100%/125%/150% scale. The tool writes ours,
 * the reference, an amplified difference and a side-by-side PNG per case, plus `report.txt` with
 * per-channel error metrics.
 *
 * This is an acceptance aid, not a unit test: exact parity depends on DirectWrite's gamma/contrast
 * and on how the UI compositor blends over the destination.
 */

#if !defined(_WIN32)
#include <cstdio>
int main() {
  std::fprintf(stderr, "cleartype_reference is Windows-only\n");
  return 77;
}
#else

#define NOMINMAX
#include <dwrite.h>
#include <windows.h>

#include <ui/ColorLayer.h>
#include <ui/TextLayer.h>
#include <ui/Window.h>

#include "TextShapeCache.h"

#include <graphics/Bitmap.h>
#include <graphics/Font.h>
#include <graphics/Image.h>
#include <graphics/TextBlob.h>
#include <graphics/Typeface.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr float kFontSizes[] = { 9.0f, 10.0f, 11.0f, 12.0f };
constexpr float kScales[] = { 1.0f, 1.25f, 1.5f };
constexpr int kLogicalWidth = 320;
constexpr int kLogicalHeight = 64;
constexpr float kTextX = 8.25f;
constexpr float kTextTop = 8.0f;

std::wstring toWide(const std::string& utf8) {
  if (utf8.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
    static_cast<int>(utf8.size()), nullptr, 0);
  if (length <= 0) return {};
  std::wstring result(static_cast<size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()),
    result.data(), length);
  return result;
}

bool writePng(const graphics::Bitmap& bitmap, const std::string& path) {
  std::error_code error;
  std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);
  std::string encodeError;
  const graphics::Data encoded = graphics::Image::fromBitmap(bitmap).encode(
    graphics::ImageFormat::PNG, 100, &encodeError);
  if (encoded.size() == 0) {
    std::fprintf(stderr, "could not encode %s: %s\n", path.c_str(), encodeError.c_str());
    return false;
  }
  std::ofstream file(path, std::ios::binary);
  if (!file.good()) return false;
  file.write(reinterpret_cast<const char*>(encoded.bytes()), static_cast<std::streamsize>(encoded.size()));
  return file.good();
}

IDWriteFontFace* createFontFace(IDWriteFactory* factory, const std::string& path) {
  const std::wstring wide = toWide(path);
  IDWriteFontFile* file = nullptr;
  if (wide.empty() || FAILED(factory->CreateFontFileReference(wide.c_str(), nullptr, &file)) ||
      file == nullptr) {
    return nullptr;
  }
  IDWriteFontFace* face = nullptr;
  for (const DWRITE_FONT_FACE_TYPE type: { DWRITE_FONT_FACE_TYPE_CFF, DWRITE_FONT_FACE_TYPE_TRUETYPE }) {
    if (SUCCEEDED(factory->CreateFontFace(type, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face)) &&
        face != nullptr) {
      break;
    }
    face = nullptr;
  }
  file->Release();
  return face;
}

/** The 32-bpp BGRA pixels of a bitmap render target, top-down. */
bool readTargetPixels(IDWriteBitmapRenderTarget* target, const int width, const int height,
    std::vector<uint32_t>& pixels) {
  const HDC dc = target->GetMemoryDC();
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  pixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0xFF000000u);
  const HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(dc, OBJ_BITMAP));
  return bitmap != nullptr &&
         GetDIBits(dc, bitmap, 0, static_cast<UINT>(height), pixels.data(), &info,
           DIB_RGB_COLORS) == height;
}

} // namespace

int main(int argc, char** argv) {
  std::string outDir = "cleartype-reference";
  std::string fontPath = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  bool lightBackground = false;
  bool knownTextBackground = true;
  bool exactPhase = true;
  bool linearLayer = false;
  float gammaOverride = -1.0f;
  float contrastOverride = -1.0f;
  float levelOverride = -1.0f;
  int geometryOverride = -1;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--out" && index + 1 < argc) outDir = argv[++index];
    else if (argument == "--font" && index + 1 < argc) fontPath = argv[++index];
    else if (argument == "--background" && index + 1 < argc) {
      lightBackground = std::string(argv[++index]) == "light";
    } else if (argument == "--text-background") {
      knownTextBackground = true;
    } else if (argument == "--no-text-background") {
      knownTextBackground = false;
    } else if (argument == "--exact-phase") {
      exactPhase = true;
    } else if (argument == "--no-exact-phase") {
      exactPhase = false;
    } else if (argument == "--linear-layer") {
      linearLayer = true;
    } else if (argument == "--gamma" && index + 1 < argc) {
      gammaOverride = static_cast<float>(std::atof(argv[++index]));
    } else if (argument == "--contrast" && index + 1 < argc) {
      contrastOverride = static_cast<float>(std::atof(argv[++index]));
    } else if (argument == "--level" && index + 1 < argc) {
      levelOverride = static_cast<float>(std::atof(argv[++index]));
    } else if (argument == "--geometry" && index + 1 < argc) {
      const std::string value = argv[++index];
      geometryOverride = value == "flat" ? 0 : (value == "bgr" ? 2 : (value == "rgb" ? 1 : -1));
    } else if (argument == "--help") {
      std::printf("usage: cleartype_reference [--out DIR] [--font FILE] [--background dark|light]\n"
                  "                            [--text-background|--no-text-background]\n"
                  "                            [--exact-phase|--no-exact-phase] [--linear-layer]\n"
                  "                            [--gamma G] [--contrast C] [--level L] [--geometry flat|rgb|bgr]\n");
      return 0;
    }
  }
  std::error_code directoryError;
  std::filesystem::create_directories(outDir, directoryError);

  IDWriteFactory* factory = nullptr;
  if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&factory))) ||
      factory == nullptr) {
    std::fprintf(stderr, "could not create a DirectWrite factory\n");
    return 1;
  }
  IDWriteRenderingParams* params = nullptr;
  if (FAILED(factory->CreateRenderingParams(&params)) || params == nullptr) {
    factory->Release();
    std::fprintf(stderr, "could not create system ClearType rendering params\n");
    return 1;
  }
  std::printf("DirectWrite system params: gamma=%.3f contrast=%.3f level=%.3f geometry=%d mode=%d\n",
    static_cast<double>(params->GetGamma()), static_cast<double>(params->GetEnhancedContrast()),
    static_cast<double>(params->GetClearTypeLevel()), static_cast<int>(params->GetPixelGeometry()),
    static_cast<int>(params->GetRenderingMode()));

  // Optional overrides let a reviewer compare a custom IDWriteRenderingParams profile against the
  // UI's system-parameter path without rebuilding.
  if (gammaOverride > 0.0f || contrastOverride >= 0.0f || levelOverride >= 0.0f ||
      geometryOverride >= 0) {
    const float gamma = gammaOverride > 0.0f ? gammaOverride : params->GetGamma();
    const float contrast = contrastOverride >= 0.0f ? contrastOverride : params->GetEnhancedContrast();
    const float level = levelOverride >= 0.0f ? levelOverride : params->GetClearTypeLevel();
    const DWRITE_PIXEL_GEOMETRY geometry = geometryOverride >= 0
      ? static_cast<DWRITE_PIXEL_GEOMETRY>(geometryOverride)
      : params->GetPixelGeometry();
    IDWriteRenderingParams* custom = nullptr;
    if (SUCCEEDED(factory->CreateCustomRenderingParams(gamma, contrast, level, geometry,
          DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL, &custom)) &&
        custom != nullptr) {
      params->Release();
      params = custom;
      std::printf("DirectWrite custom params: gamma=%.3f contrast=%.3f level=%.3f geometry=%d\n",
        static_cast<double>(gamma), static_cast<double>(contrast), static_cast<double>(level),
        static_cast<int>(geometry));
    }
  }

  const auto typeface = graphics::Typeface::fromFile(fontPath);
  if (typeface == nullptr) {
    params->Release();
    factory->Release();
    std::fprintf(stderr, "could not open %s\n", fontPath.c_str());
    return 1;
  }

  const std::string text = "Hamburgefontsiv 0123";
  std::ofstream report(outDir + "/report.txt");
  report << "size scale mean_abs max_diff over16_pct\n";

  for (const float size: kFontSizes) {
    for (const float scale: kScales) {
      const int width = static_cast<int>(std::lround(kLogicalWidth * scale));
      const int height = static_cast<int>(std::lround(kLogicalHeight * scale));
      const auto font = graphics::Font::create(typeface, size);
      if (font == nullptr) continue;
      const auto deviceFont = font->withSize(size * scale);
      if (deviceFont == nullptr) continue;
      const std::shared_ptr<const graphics::TextBlob> blob =
        coral::ui::detail::TextShapeCache::instance().shape(*deviceFont, text,
          graphics::Font::Direction::Auto);
      if (blob == nullptr || blob->glyphs().empty()) continue;

      coral::ui::Window::Options options;
      options.headless = true;
      options.width = width;
      options.height = height;
      options.uiScale = scale;
      options.frameLimit = 1;
      options.clearColor = lightBackground ? graphics::Color::MakeRGBA8(255, 255, 255, 255)
                                           : graphics::Color::MakeRGBA8(0, 0, 0, 255);
      auto window = coral::ui::Window::create(options);
      if (window == nullptr) {
        std::fprintf(stderr, "no headless window for %.2f px @ %.2f\n", size, scale);
        continue;
      }
      auto root = std::make_shared<coral::ui::ColorLayer>(
        lightBackground ? graphics::Color::MakeRGBA8(255, 255, 255, 255)
                        : graphics::Color::MakeRGBA8(0, 0, 0, 255));
      root->setBounds(coral::ui::Rect::MakeWH(static_cast<float>(kLogicalWidth),
        static_cast<float>(kLogicalHeight)));
      auto label = std::make_shared<coral::ui::TextLayer>(text, font,
        lightBackground ? graphics::Color::MakeRGBA8(0, 0, 0, 255)
                        : graphics::Color::MakeRGBA8(255, 255, 255, 255));
      label->setExactSubpixelPhase(exactPhase);
      if (linearLayer) {
        label->clearTextBackgroundColor();
        label->setLinearLightLayer(true);
      }
      if (knownTextBackground) {
        label->setTextBackgroundColor(lightBackground
          ? graphics::Color::MakeRGBA8(255, 255, 255, 255)
          : graphics::Color::MakeRGBA8(0, 0, 0, 255));
      }
      label->setBounds(coral::ui::Rect::MakeLTRB(kTextX, kTextTop,
        static_cast<float>(kLogicalWidth), static_cast<float>(kLogicalHeight)));
      root->addChild(label);
      window->setRoot(root);
      if (!window->renderFrame()) continue;
      graphics::Bitmap oursBitmap;
      if (!window->snapshot(&oursBitmap)) continue;

      const float baseline = std::round((kTextTop + font->metrics().ascent) * scale);
      const float fontToDevice = deviceFont->size() / static_cast<float>(typeface->unitsPerEm());
      std::vector<UINT16> glyphIndices;
      std::vector<FLOAT> glyphAdvances;
      std::vector<DWRITE_GLYPH_OFFSET> glyphOffsets;
      for (const graphics::Glyph& glyph: blob->glyphs()) {
        glyphIndices.push_back(static_cast<UINT16>(glyph.id));
        glyphAdvances.push_back(glyph.advance.x * fontToDevice);
        glyphOffsets.push_back({ glyph.offset.x * fontToDevice, glyph.offset.y * fontToDevice });
      }

      std::vector<uint32_t> referencePixels;
      IDWriteFontFace* face = createFontFace(factory, fontPath);
      IDWriteGdiInterop* gdi = nullptr;
      IDWriteBitmapRenderTarget* target = nullptr;
      if (face != nullptr && SUCCEEDED(factory->GetGdiInterop(&gdi)) && gdi != nullptr &&
          SUCCEEDED(gdi->CreateBitmapRenderTarget(nullptr, static_cast<UINT32>(width),
            static_cast<UINT32>(height), &target)) &&
          target != nullptr) {
        const HDC dc = target->GetMemoryDC();
        PatBlt(dc, 0, 0, width, height, lightBackground ? WHITENESS : BLACKNESS);
        target->SetPixelsPerDip(1.0f);
        DWRITE_GLYPH_RUN run{};
        run.fontFace = face;
        run.fontEmSize = deviceFont->size();
        run.glyphCount = static_cast<UINT32>(glyphIndices.size());
        run.glyphIndices = glyphIndices.data();
        run.glyphAdvances = glyphAdvances.data();
        run.glyphOffsets = glyphOffsets.data();
        run.isSideways = FALSE;
        run.bidiLevel = 0;
        target->DrawGlyphRun(kTextX * scale, baseline, DWRITE_MEASURING_MODE_NATURAL,
          &run, params, lightBackground ? RGB(0, 0, 0) : RGB(255, 255, 255), nullptr);
        GdiFlush();
        readTargetPixels(target, width, height, referencePixels);
      }
      if (target != nullptr) target->Release();
      if (gdi != nullptr) gdi->Release();
      if (face != nullptr) face->Release();
      if (referencePixels.empty()) {
        report << size << " " << scale << " missing-reference\n";
        continue;
      }

      graphics::Bitmap referenceBitmap = graphics::Bitmap::allocate(width, height);
      if (referenceBitmap.isEmpty()) continue;
      graphics::Pixmap referencePixmap = referenceBitmap.lockPixels();
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          const uint32_t pixel = referencePixels[static_cast<size_t>(y) * width + x];
          referencePixmap.setPixelARGB32(x, y, pixel | 0xFF000000u);
        }
      }

      const graphics::Pixmap ours = oursBitmap.makePixmap();
      const graphics::Pixmap reference = referenceBitmap.makePixmap();
      graphics::Bitmap diffBitmap = graphics::Bitmap::allocate(width, height);
      graphics::Bitmap sideBitmap = graphics::Bitmap::allocate(width * 2, height);
      if (diffBitmap.isEmpty() || sideBitmap.isEmpty()) continue;
      graphics::Pixmap diff = diffBitmap.lockPixels();
      graphics::Pixmap side = sideBitmap.lockPixels();
      double sum = 0.0;
      int maximum = 0;
      int over16 = 0;
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          const uint32_t a = ours.getPixelARGB32(x, y);
          const uint32_t b = reference.getPixelARGB32(x, y);
          int pixelDiff = 0;
          for (int shift = 0; shift < 24; shift += 8) {
            const int delta = std::abs(static_cast<int>((a >> shift) & 0xFFu) -
                                       static_cast<int>((b >> shift) & 0xFFu));
            sum += delta;
            maximum = std::max(maximum, delta);
            if (delta > 16) ++over16;
            pixelDiff = std::max(pixelDiff, delta);
          }
          const uint32_t amplified = static_cast<uint32_t>(std::min(255, pixelDiff * 4));
          diff.setPixelARGB32(x, y, 0xFF000000u | (amplified << 16) | (amplified << 8) | amplified);
          side.setPixelARGB32(x, y, a);
          side.setPixelARGB32(width + x, y, b);
        }
      }
      const double mean = sum / (static_cast<double>(width) * height * 3.0);
      const double overPercent = 100.0 * over16 / (static_cast<double>(width) * height * 3.0);
      const std::string stem = outDir + "/" + std::to_string(static_cast<int>(size)) + "px_" +
                               std::to_string(static_cast<int>(std::lround(scale * 100.0f))) + "pct";
      writePng(oursBitmap, stem + "_ours.png");
      writePng(referenceBitmap, stem + "_reference.png");
      writePng(diffBitmap, stem + "_diff.png");
      writePng(sideBitmap, stem + "_side.png");
      report << size << " " << scale << " " << mean << " " << maximum << " " << overPercent << "\n";
      std::printf("%2.0f px @ %3.0f%%: mean_abs=%.3f max=%d over16=%.3f%%\n", size, scale * 100.0f,
        mean, maximum, overPercent);
    }
  }

  report.close();
  params->Release();
  factory->Release();
  std::printf("wrote comparison images to %s\n", outDir.c_str());
  return 0;
}

#endif
