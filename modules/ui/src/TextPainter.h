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

#ifndef CORAL_UI_TEXTPAINTER_H
#define CORAL_UI_TEXTPAINTER_H

#include <ui/RenderContext.h>
#include <ui/CanvasBackend.h>

#include "TextShapeCache.h"

#include <graphics/Bitmap.h>
#include <graphics/Font.h>
#include <graphics/GlyphAtlas.h>
#include <graphics/Paint.h>
#include <graphics/Pixmap.h>
#include <graphics/TextBlob.h>
#include <ui/TextRendererStats.h>

#include <algorithm>
#include <span>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace coral::ui {

/**
 * Draws one line of text at device resolution while the layer tree stays in logical units.
 *
 * A logical font would be rasterised at the design size and then enlarged by the canvas transform,
 * which is the blurry-text failure mode of scaled UIs. This painter keeps a device-sized font
 * (`size * uiScale`), shapes the line once into an immutable `TextBlob`, snaps the pen to the physical
 * pixel grid, and draws under an inverse local scale so the glyphs land at the logical position at
 * full device resolution.
 */
class TextPainter {
  public:
  struct RunBitmap {
    graphics::Bitmap bitmap;
    /**
     * Per-channel alpha strips for subpixel text.
     *
     * Each strip is premultiplied with the text colour and stores the channel's coverage in its
     * alpha, so drawing it with the matching `Paint::channelMask` is an ordinary source-over blend
     * that only touches that channel.
     */
    std::array<graphics::Bitmap, 3> subpixelMasks;
    int originX = 0;
    int originY = 0;
    /** ARGB of the text colour the subpixel strips were baked with, or 0 for a grayscale run. */
    uint32_t colorKey = 0;
    bool subpixel = false;
  };

  /**
   * Sets per-run OpenType feature settings. Tags are borrowed from the caller and must outlive the
   * painter; the UI layer owns the strings.
   */
  void setFeatures(std::span<const graphics::FontFeature> features) {
    if (mFeatures.size() == features.size() &&
        std::equal(mFeatures.begin(), mFeatures.end(), features.begin(),
          [](const graphics::FontFeature& a, const graphics::FontFeature& b) {
            return a.tag == b.tag && a.value == b.value && a.start == b.start && a.end == b.end;
          })) {
      return;
    }
    mFeatures.assign(features.begin(), features.end());
    mFeaturesDirty = true;
    mText.clear();
    mBlob.reset();
    mRunAttempted = false;
    mSubpixelAttempted = false;
  }

  [[nodiscard]] std::span<const graphics::FontFeature> features() const noexcept {
    return mFeatures;
  }

  void draw(RenderContext& context, const std::string_view text, const float x, const float baseline,
          const graphics::Font& font, const graphics::Paint& paint,
          const graphics::Font::Direction direction = graphics::Font::Direction::Auto,
          const std::shared_ptr<const graphics::TextBlob> shaped = nullptr) {
    graphics::Canvas& canvas = detail::nativeCanvas(context.canvas());
    const float scale = context.uiScale();
    if (mSourceFont != &font || mScale != scale || mDirection != direction) {
      mSourceFont = &font;
      mScale = scale;
      mDirection = direction;
      mDeviceFont = font.withSize(font.size() * scale);
      mBlob.reset();
      mText.clear();
      mRunAttempted = false;
      mSubpixelAttempted = false;
      mSubpixelValid = false;
      mSubpixelRun.reset();
    }
    if (mDeviceFont == nullptr) return;
    if (shaped != nullptr) {
      mBlob = shaped;
      mText.assign(text.data(), text.size());
      mRunAttempted = false;
      mSubpixelAttempted = false;
    } else if (mText != text || mFeaturesDirty) {
      mText.assign(text.data(), text.size());
      mBlob = mFeatures.empty()
        ? detail::TextShapeCache::instance().shape(*mDeviceFont, text, direction)
        : std::make_shared<const graphics::TextBlob>(mDeviceFont->shape(text, direction, mFeatures));
      mFeaturesDirty = false;
      mRunAttempted = false;
      mSubpixelAttempted = false;
    }
    if (mBlob == nullptr || mBlob->glyphs().empty()) return;

    // The frame canvas is scaled by `scale`, so an inverse scale brings it back to device space. Drawing
    // the device-sized blob at rounded device coordinates there gives both a full-resolution glyph and
    // a pen on the physical pixel grid.
    const float deviceX = std::round(x * scale);
    const float deviceBaseline = std::round(baseline * scale);

    // Platform ClearType/CoreText coverage is composited as three channel-masked passes. The fallback
    // path below stays grayscale when the placement, blend or paint cannot support per-channel
    // coverage.
    if (context.isOpaque() && canDrawSubpixel(canvas, paint) &&
        buildSubpixelRunBitmap(paint, x * scale, deviceBaseline)) {
      drawSubpixelRun(canvas, x * scale, deviceBaseline, paint, scale);
      return;
    }
    if (mHinted && canvas.isRecording()) {
      // The GPU recording path uses one shared atlas page per glyph size. Each glyph is a sub-rectangle command
      // against that page; the recorder registers the page once and the renderer batches the commands because they
      // share one texture. The CPU path below keeps the cheaper one-bitmap-per-line behaviour.
      if (scale != 1.0f) {
        canvas.save();
        canvas.scale(1.0f / scale);
      }
      const auto& typeface = mDeviceFont->typeface();
      const float em = typeface->unitsPerEm() > 0 ? static_cast<float>(typeface->unitsPerEm()) : 1.0f;
      const float fontToDevice = mDeviceFont->size() / em;
      float penX = 0.0f;
      for (const graphics::Glyph& glyph: mBlob->glyphs()) {
        const graphics::GlyphBitmap bitmap = typeface->glyphBitmap(glyph.id, mDeviceFont->size());
        const float advance = glyph.advance.x * fontToDevice;
        if (!bitmap.isValid()) {
          penX += advance;
          continue;
        }
        const float left = deviceX + penX + glyph.offset.x * fontToDevice + bitmap.left;
        const float top = deviceBaseline - glyph.offset.y * fontToDevice - bitmap.top;
        if (bitmap.color) {
          canvas.drawBitmap(bitmap.pixels, left, top, paint);
        } else if (const auto entry = graphics::GlyphAtlas::instance().add(bitmap, glyph.id, mDeviceFont->size(),
                           typeface.get())) {
          canvas.drawBitmapRect(graphics::GlyphAtlas::instance().bitmap(entry->page), entry->source,
                  Rect::MakeXYWH(left, top, bitmap.width, bitmap.height), paint);
          detail::recordAtlasGlyph();
        } else {
          canvas.drawBitmap(bitmap.pixels, left, top, paint);
        }
        penX += advance;
      }
      if (scale != 1.0f) canvas.restore();
      return;
    }
    if (mHinted) {
      if (buildRunBitmap()) {
        if (scale != 1.0f) {
          canvas.save();
          canvas.scale(1.0f / scale);
        }
        canvas.drawBitmap(mRunBitmap->bitmap, deviceX + static_cast<float>(mRunBitmap->originX),
                deviceBaseline + static_cast<float>(mRunBitmap->originY), paint);
        detail::recordRunDrawCall();
        if (scale != 1.0f) canvas.restore();
        return;
      }
      // A colour glyph (emoji) cannot be baked into a tinted alpha strip; fall back to one bitmap
      // draw per glyph for that line.
      if (scale != 1.0f) {
        canvas.save();
        canvas.scale(1.0f / scale);
      }
      const auto& typeface = mDeviceFont->typeface();
      const float em = typeface->unitsPerEm() > 0 ? static_cast<float>(typeface->unitsPerEm()) : 1.0f;
      const float fontToDevice = mDeviceFont->size() / em;
      float penX = 0.0f;
      for (const graphics::Glyph& glyph: mBlob->glyphs()) {
        const graphics::GlyphBitmap bitmap = typeface->glyphBitmap(glyph.id, mDeviceFont->size());
        const float advance = glyph.advance.x * fontToDevice;
        if (!bitmap.isValid()) {
          penX += advance;
          continue;
        }
        graphics::Paint glyphPaint = paint;
        if (bitmap.color) glyphPaint.color = graphics::Color::White();
        canvas.drawBitmap(bitmap.pixels, deviceX + penX + glyph.offset.x * fontToDevice + bitmap.left,
                deviceBaseline - glyph.offset.y * fontToDevice - bitmap.top, glyphPaint);
        penX += advance;
      }
      if (scale != 1.0f) canvas.restore();
      return;
    }
    if (scale == 1.0f) {
      canvas.drawTextBlob(*mBlob, deviceX, deviceBaseline, *mDeviceFont, paint);
      return;
    }
    canvas.save();
    canvas.scale(1.0f / scale);
    canvas.drawTextBlob(*mBlob, deviceX, deviceBaseline, *mDeviceFont, paint);
    canvas.restore();
  }

  /** Hinted FreeType coverage glyphs; disable to return to the outline-path debug route. */
  void setHinted(bool hinted) noexcept {
    mHinted = hinted;
    mRunAttempted = false;
    mRunBitmapValid = false;
    mRunBitmap = {};
  }

  private:
  /**
   * @return true when a subpixel run can be drawn at this canvas position.
   *
   * ClearType colour fringing assumes an axis-aligned, uniformly scaled placement. A rotated, skewed
   * or non-uniformly scaled canvas would fringe in the wrong direction, so those cases keep the
   * grayscale run; the same is true for a non-source-over blend, a colour filter, or an SDF request.
   */
  bool canDrawSubpixel(const graphics::Canvas& canvas, const graphics::Paint& paint) const {
    if (!mHinted || paint.sdfText || paint.hasColorFilter() || paint.channelMask != 0x0F) return false;
    if (paint.coverageOnly) {
      if (paint.blendMode != graphics::BlendMode::kPlus) return false;
    } else if (paint.blendMode != graphics::BlendMode::kSrcOver) {
      return false;
    }
    const graphics::Matrix transform = canvas.transform();
    constexpr float kEpsilon = 1.0e-4f;
    return std::fabs(transform.vec[0].y) < kEpsilon && std::fabs(transform.vec[1].x) < kEpsilon &&
           std::fabs(transform.vec[0].x - transform.vec[1].y) < kEpsilon && transform.vec[0].x > 0.0f;
  }

  /** @return the 1/16-pixel bucket containing @p value's fractional position. */
  static int phaseSteps(const float value) {
    return phaseSteps(value, 16);
  }

  static int phaseSteps(const float value, const int steps) {
    const float fraction = value - std::floor(value);
    return std::min(steps - 1, static_cast<int>(std::floor(fraction * steps + 0.5f)));
  }

  /** Builds the three per-channel strips for one run, or returns false to fall back to grayscale. */
  bool buildSubpixelRunBitmap(const graphics::Paint& paint, const float deviceX,
      const float deviceBaseline) {
    if (!mFeatures.empty()) return false;
    const auto channel = [](const float value) {
      return static_cast<uint32_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    };
    const uint32_t textAlpha = channel(paint.color.alpha);
    const uint32_t colorKey = (textAlpha << 24) | (channel(paint.color.red) << 16) |
                              (channel(paint.color.green) << 8) | channel(paint.color.blue);
    const uint32_t backgroundKey = paint.hasTextBackground
      ? (channel(paint.textBackground.red) << 16) |
        (channel(paint.textBackground.green) << 8) | channel(paint.textBackground.blue)
      : 0;
    const auto backgroundImageKey = reinterpret_cast<uintptr_t>(paint.textBackgroundBitmap);
    const int basePhase = paint.exactSubpixelPhase ? phaseSteps(deviceX, 256) : phaseSteps(deviceX);
    const graphics::SubpixelRenderingParams renderingParams = graphics::subpixelRenderingParams();
    const int gammaKey = static_cast<int>(std::lround(renderingParams.gamma * 100.0f));
    const int contrastKey = static_cast<int>(std::lround(renderingParams.contrast * 100.0f));
    const int levelKey = static_cast<int>(std::lround(renderingParams.clearTypeLevel * 100.0f));
    const uint64_t baseParamKey = (static_cast<uint64_t>(gammaKey & 0xFFFF) << 40) |
                                  (static_cast<uint64_t>(contrastKey & 0xFFFF) << 24) |
                                  (static_cast<uint64_t>(levelKey & 0xFFFF) << 8) |
                                  static_cast<uint64_t>(renderingParams.pixelGeometry & 0xFF);
    const uint64_t backgroundSalt = paint.hasTextBackground
      ? (0x8000000000000000ull | (static_cast<uint64_t>(backgroundKey) << 8) |
         (static_cast<uint64_t>(backgroundImageKey >> 4) & 0x3FFFFFFFFFFFFFFFull))
      : 0ull;
    const uint64_t paramKey = baseParamKey ^ backgroundSalt ^
                              (paint.exactSubpixelPhase ? (1ull << 62) : 0ull) ^
                              (paint.coverageOnly ? (1ull << 61) : 0ull);
    if (mSubpixelAttempted && mSubpixelColor == colorKey && mSubpixelPhase == basePhase &&
        mSubpixelParamKey == paramKey) {
      return mSubpixelValid;
    }

    mSubpixelAttempted = true;
    mSubpixelValid = false;
    mSubpixelColor = colorKey;
    mSubpixelPhase = basePhase;
    mSubpixelParamKey = paramKey;
    mSubpixelRun.reset();
    if (mBlob == nullptr || mDeviceFont == nullptr || textAlpha == 0) return false;

    const auto& typeface = mDeviceFont->typeface();
    if (typeface == nullptr || typeface->unitsPerEm() == 0) return false;
    const std::string cacheKey = std::to_string(reinterpret_cast<uintptr_t>(typeface.get())) + ":" +
                                 std::to_string(static_cast<uint64_t>(std::llround(mDeviceFont->size() * 64.0))) + ":" +
                                 std::to_string(colorKey) + ":" + std::to_string(basePhase) + ":" +
                                 (paint.exactSubpixelPhase ? "exact:" : "quant:") +
                                 (paint.coverageOnly ? "coverage:" : "tinted:") +
                                 std::to_string(gammaKey) + ":" + std::to_string(contrastKey) + ":" +
                                 std::to_string(levelKey) + ":" +
                                 std::to_string(renderingParams.pixelGeometry) + ":" +
                                 (paint.hasTextBackground ? "bg" + std::to_string(backgroundKey) + ":" +
                                   std::to_string(backgroundImageKey) : "nobg") +
                                 ":lcd:" + mText;
    {
      if (const auto cached = runCache().find(cacheKey)) {
        mSubpixelRun = cached;
        mSubpixelValid = true;
        detail::recordRunCacheHit();
        return true;
      }
    }
    const float fontToDevice = mDeviceFont->size() / static_cast<float>(typeface->unitsPerEm());
    const float baseFloor = std::floor(deviceX);
    const graphics::FontMetrics& metrics = mDeviceFont->metrics();
    float pen = 0.0f;
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = -metrics.ascent - 2.0f;
    float maxY = metrics.descent + 2.0f;
    for (const graphics::Glyph& glyph: mBlob->glyphs()) {
      const float absolutePen = deviceX + pen + glyph.offset.x * fontToDevice;
      const int steps = phaseSteps(absolutePen);
      const float phase = paint.exactSubpixelPhase
        ? absolutePen - std::floor(absolutePen)
        : steps * 0.0625f;
      const graphics::GlyphBitmap bitmap = paint.exactSubpixelPhase
        ? typeface->glyphBitmapExact(glyph.id, mDeviceFont->size(), true, phase)
        : typeface->glyphBitmap(glyph.id, mDeviceFont->size(), true, phase);
      const float advance = glyph.advance.x * fontToDevice;
      if (bitmap.isValid()) {
        // A glyph the platform did not rasterise as subpixel keeps its grayscale coverage; its RGB
        // channels are all the same alpha in that case, so it participates without dropping the whole
        // run back to grayscale. A colour glyph cannot be tinted and forces the fallback.
        if (bitmap.color) return false;
        const float left = std::floor(absolutePen) - baseFloor + bitmap.left;
        const float top = -glyph.offset.y * fontToDevice - bitmap.top;
        minX = std::min(minX, left);
        maxX = std::max(maxX, left + bitmap.width);
        minY = std::min(minY, top);
        maxY = std::max(maxY, top + bitmap.height);
      }
      pen += advance;
    }

    auto run = std::make_shared<RunBitmap>();
    run->originX = static_cast<int>(std::floor(minX)) - 1;
    run->originY = static_cast<int>(std::floor(minY)) - 1;
    const int width = static_cast<int>(std::ceil(maxX)) - run->originX + 1;
    const int height = static_cast<int>(std::ceil(maxY)) - run->originY + 1;
    if (width <= 0 || height <= 0 || width > 16384 || height > 4096) return false;
    for (graphics::Bitmap& mask: run->subpixelMasks) {
      mask = graphics::Bitmap::allocate(width, height);
      if (mask.isEmpty()) return false;
    }

    const uint8_t colorBytes[3] = {
      static_cast<uint8_t>(channel(paint.color.red)),
      static_cast<uint8_t>(channel(paint.color.green)),
      static_cast<uint8_t>(channel(paint.color.blue)),
    };
    const float gamma = std::max(renderingParams.gamma, 0.1f);
    const float contrast = std::clamp(renderingParams.contrast, 0.0f, 1.0f);
    const float level = std::clamp(renderingParams.clearTypeLevel, 0.0f, 1.0f);
    const float textAlphaFraction = static_cast<float>(textAlpha) / 255.0f;
    const float textComponents[3] = { paint.color.red, paint.color.green, paint.color.blue };
    const float backgroundComponents[3] = {
      paint.textBackground.red, paint.textBackground.green, paint.textBackground.blue,
    };
    const bool hasTextBackground = paint.hasTextBackground;
    const auto shapeCoverage = [gamma, contrast, level, textAlphaFraction, hasTextBackground,
        coverageOnly = paint.coverageOnly, textComponents](const uint32_t red, const uint32_t green,
        const uint32_t blue, const float* background, uint32_t* out) {
      float values[3] = { static_cast<float>(red) / 255.0f, static_cast<float>(green) / 255.0f,
        static_cast<float>(blue) / 255.0f };
      const float average = (values[0] + values[1] + values[2]) / 3.0f;
      for (int index = 0; index < 3; ++index) {
        // `clearTypeLevel` blends the per-channel coverage back toward grayscale.
        float coverage = average + (values[index] - average) * level;
        if (coverageOnly) {
          // The layer's RGB is the raw coverage; the layer composite applies the gamma/contrast
          // against the actual destination it reads.
          out[index] = static_cast<uint32_t>(
            std::lround(std::clamp(coverage * textAlphaFraction, 0.0f, 1.0f) * 255.0f));
          continue;
        }
        if (hasTextBackground) {
          // Solve a per-channel alpha so source-over over the known backdrop produces the
          // linear-light composite. The text colour is the source endpoint, the sampled backdrop
          // the destination endpoint; the three channel passes can use different alphas.
          coverage = std::clamp(coverage * textAlphaFraction, 0.0f, 1.0f);
          const float text = std::clamp(textComponents[index], 0.0f, 1.0f);
          const float bg = std::clamp(background[index], 0.0f, 1.0f);
          const float linear = std::pow(text, gamma) * coverage +
                               std::pow(bg, gamma) * (1.0f - coverage);
          float value = std::pow(std::max(linear, 0.0f), 1.0f / gamma);
          value = (value * (1.0f + contrast)) / (value * contrast + 1.0f);
          const float denominator = text - bg;
          float alpha = std::fabs(denominator) > 1e-5f ? (value - bg) / denominator : 0.0f;
          out[index] = static_cast<uint32_t>(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));
        } else {
          // No backdrop is known: keep the single-alpha gamma/contrast approximation. It is exact
          // for a light glyph over a dark destination and approximate elsewhere.
          float value = std::pow(std::max(coverage, 0.0f), 1.0f / gamma);
          value = (value * (1.0f + contrast)) / (value * contrast + 1.0f);
          out[index] = static_cast<uint32_t>(
            std::lround(std::clamp(value, 0.0f, 1.0f) * textAlphaFraction * 255.0f));
        }
      }
    };
    const graphics::Pixmap backgroundPixels = paint.textBackgroundBitmap != nullptr
      ? paint.textBackgroundBitmap->makePixmap()
      : graphics::Pixmap{};
    pen = 0.0f;
    for (const graphics::Glyph& glyph: mBlob->glyphs()) {
      const float absolutePen = deviceX + pen + glyph.offset.x * fontToDevice;
      const int steps = phaseSteps(absolutePen);
      const float phase = paint.exactSubpixelPhase
        ? absolutePen - std::floor(absolutePen)
        : steps * 0.0625f;
      const graphics::GlyphBitmap bitmap = paint.exactSubpixelPhase
        ? typeface->glyphBitmapExact(glyph.id, mDeviceFont->size(), true, phase)
        : typeface->glyphBitmap(glyph.id, mDeviceFont->size(), true, phase);
      const float advance = glyph.advance.x * fontToDevice;
      if (bitmap.isValid()) {
        const float left = std::floor(absolutePen) - baseFloor + bitmap.left;
        const int originX = static_cast<int>(std::lround(left)) - run->originX;
        const int originY = static_cast<int>(std::lround(-glyph.offset.y * fontToDevice - bitmap.top)) -
                            run->originY;
        const graphics::Pixmap source = bitmap.pixels.makePixmap();
        for (int y = 0; y < static_cast<int>(bitmap.height); ++y) {
          const int targetY = originY + y;
          if (targetY < 0 || targetY >= height) continue;
          for (int x = 0; x < static_cast<int>(bitmap.width); ++x) {
            const int targetX = originX + x;
            if (targetX < 0 || targetX >= width) continue;
            const uint32_t src = source.getPixelARGB32(x, y);
            uint32_t coverage[3] = { (src >> 16) & 0xFFu, (src >> 8) & 0xFFu, src & 0xFFu };
            if (renderingParams.pixelGeometry == 2) {
              // BGR subpixel order: the platform's coverage is still three channels, but the panel's
              // physical order is reversed relative to the RGB texture.
              std::swap(coverage[0], coverage[2]);
            }
            float background[3] = {
              backgroundComponents[0], backgroundComponents[1], backgroundComponents[2],
            };
            if (backgroundPixels.isValid()) {
              const int backgroundX = static_cast<int>(std::floor(deviceX)) + targetX;
              const int backgroundY = static_cast<int>(deviceBaseline) + run->originY + targetY;
              if (backgroundX >= 0 && backgroundY >= 0 && backgroundX < backgroundPixels.width() &&
                  backgroundY < backgroundPixels.height()) {
                const uint32_t pixel = backgroundPixels.getPixelARGB32(backgroundX, backgroundY);
                background[0] = static_cast<float>((pixel >> 16) & 0xFFu) / 255.0f;
                background[1] = static_cast<float>((pixel >> 8) & 0xFFu) / 255.0f;
                background[2] = static_cast<float>(pixel & 0xFFu) / 255.0f;
              }
            }
            shapeCoverage(coverage[0], coverage[1], coverage[2], background, coverage);
            for (int channelIndex = 0; channelIndex < 3; ++channelIndex) {
              const uint32_t alpha = coverage[channelIndex];
              if (alpha == 0) continue;
              graphics::Pixmap target = run->subpixelMasks[channelIndex].lockPixels();
              if (!target.isValid()) return false;
              const uint32_t dst = target.getPixelARGB32(targetX, targetY);
              if (((dst >> 24) & 0xFFu) >= alpha) continue;
              // Premultiplied text colour: the source-over pass writes one channel, so the other
              // channels of this pixel are ignored by the pipeline's write mask.
              const uint32_t red = channelIndex == 0 ? (paint.coverageOnly ? alpha : (colorBytes[0] * alpha + 127u) / 255u) : 0u;
              const uint32_t green = channelIndex == 1 ? (paint.coverageOnly ? alpha : (colorBytes[1] * alpha + 127u) / 255u) : 0u;
              const uint32_t blue = channelIndex == 2 ? (paint.coverageOnly ? alpha : (colorBytes[2] * alpha + 127u) / 255u) : 0u;
              target.setPixelARGB32(targetX, targetY,
                (alpha << 24) | (red << 16) | (green << 8) | blue);
            }
          }
        }
      }
      pen += advance;
    }

    run->subpixel = true;
    run->colorKey = colorKey;
    const std::size_t bytes = runBytes(*run);
    runCache().insert(cacheKey, run, bytes);
    detail::recordRunBuild(bytes);
    mSubpixelRun = std::move(run);
    mSubpixelValid = true;
    return true;
  }

  /** Draws the three premultiplied strips with one channel-write mask each. */
  void drawSubpixelRun(graphics::Canvas& canvas, const float deviceX, const float deviceBaseline,
      const graphics::Paint& paint, const float scale) const {
    if (mSubpixelRun == nullptr) return;
    if (scale != 1.0f) {
      canvas.save();
      canvas.scale(1.0f / scale);
    }
    for (int channelIndex = 0; channelIndex < 3; ++channelIndex) {
      graphics::Paint channelPaint = paint;
      channelPaint.color = graphics::Color::White();
      channelPaint.channelMask = static_cast<uint8_t>((1u << channelIndex) |
        (paint.coverageOnly ? 0x8u : 0u));
      channelPaint.blendMode = paint.blendMode;
      canvas.drawBitmap(mSubpixelRun->subpixelMasks[channelIndex],
        std::floor(deviceX) + static_cast<float>(mSubpixelRun->originX),
        deviceBaseline + static_cast<float>(mSubpixelRun->originY), channelPaint);
      detail::recordRunDrawCall();
    }
    if (scale != 1.0f) canvas.restore();
  }

  /**
   * Fills one device-size alpha strip with the hinted glyph coverage and draws it as a single bitmap.
   *
   * This is the per-line glyph atlas: rasterisation still happens once per glyph (and Typeface caches
   * it), but a frame with unchanged text issues one image draw instead of one draw per glyph. Colour
   * glyphs are refused because the strip is tinted by the paint colour.
   */
  bool buildRunBitmap() {
    if (!mFeatures.empty()) return false;
    if (mRunAttempted) return mRunBitmapValid;
    mRunAttempted = true;
    mRunBitmapValid = false;
    if (mBlob == nullptr || mDeviceFont == nullptr) return false;
    const auto& typeface = mDeviceFont->typeface();
    if (typeface == nullptr || typeface->unitsPerEm() == 0) return false;
    const std::string cacheKey = std::to_string(reinterpret_cast<uintptr_t>(typeface.get())) + ":" +
                                 std::to_string(static_cast<uint64_t>(std::llround(mDeviceFont->size() * 64.0))) + ":" +
                                 mText;
    {
      if (const auto cached = runCache().find(cacheKey)) {
        mRunBitmap = cached;
        mRunBitmapValid = true;
        detail::recordRunCacheHit();
        return true;
      }
    }
    const float fontToDevice = mDeviceFont->size() / static_cast<float>(typeface->unitsPerEm());
    const graphics::FontMetrics& metrics = mDeviceFont->metrics();
    float pen = 0.0f;
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = -metrics.ascent - 2.0f;
    float maxY = metrics.descent + 2.0f;
    for (const graphics::Glyph& glyph: mBlob->glyphs()) {
      const graphics::GlyphBitmap bitmap = typeface->glyphBitmap(glyph.id, mDeviceFont->size());
      const float advance = glyph.advance.x * fontToDevice;
      if (bitmap.isValid()) {
        if (bitmap.color) return false;
        const float left = pen + glyph.offset.x * fontToDevice + bitmap.left;
        const float top = -glyph.offset.y * fontToDevice - bitmap.top;
        minX = std::min(minX, left);
        maxX = std::max(maxX, left + bitmap.width);
        minY = std::min(minY, top);
        maxY = std::max(maxY, top + bitmap.height);
      }
      pen += advance;
    }
    auto run = std::make_shared<RunBitmap>();
    run->originX = static_cast<int>(std::floor(minX)) - 1;
    run->originY = static_cast<int>(std::floor(minY)) - 1;
    const int width = static_cast<int>(std::ceil(maxX)) - run->originX + 1;
    const int height = static_cast<int>(std::ceil(maxY)) - run->originY + 1;
    if (width <= 0 || height <= 0 || width > 16384 || height > 4096) return false;
    run->bitmap = graphics::Bitmap::allocate(width, height);
    if (run->bitmap.isEmpty()) return false;
    graphics::Pixmap target = run->bitmap.lockPixels();
    if (!target.isValid()) return false;
    pen = 0.0f;
    for (const graphics::Glyph& glyph: mBlob->glyphs()) {
      const graphics::GlyphBitmap bitmap = typeface->glyphBitmap(glyph.id, mDeviceFont->size());
      const float advance = glyph.advance.x * fontToDevice;
      if (bitmap.isValid()) {
        const int originX =
                static_cast<int>(std::lround(pen + glyph.offset.x * fontToDevice + bitmap.left)) - run->originX;
        const int originY = static_cast<int>(std::lround(-glyph.offset.y * fontToDevice - bitmap.top)) - run->originY;
        const graphics::Pixmap source = bitmap.pixels.makePixmap();
        for (int y = 0; y < static_cast<int>(bitmap.height); ++y) {
          const int targetY = originY + y;
          if (targetY < 0 || targetY >= height) continue;
          for (int x = 0; x < static_cast<int>(bitmap.width); ++x) {
            const int targetX = originX + x;
            if (targetX < 0 || targetX >= width) continue;
            const uint32_t src = source.getPixelARGB32(x, y);
            const uint32_t alpha = src >> 24;
            if (alpha == 0) continue;
            const uint32_t dst = target.getPixelARGB32(targetX, targetY);
            const uint32_t inverse = 255u - alpha;
            const auto blend = [alpha, inverse](const uint32_t a, const uint32_t b) {
              return std::min(255u, (a * alpha + b * inverse + 127u) / 255u);
            };
            const uint32_t out = (blend(src >> 24, dst >> 24) << 24) |
                                 (blend((src >> 16) & 0xFFu, (dst >> 16) & 0xFFu) << 16) |
                                 (blend((src >> 8) & 0xFFu, (dst >> 8) & 0xFFu) << 8) | blend(src & 0xFFu, dst & 0xFFu);
            target.setPixelARGB32(targetX, targetY, out);
          }
        }
      }
      pen += advance;
    }
    const std::size_t bytes = runBytes(*run);
    runCache().insert(cacheKey, run, bytes);
    mRunBitmap = std::move(run);
    mRunBitmapValid = true;
    detail::recordRunBuild(bytes);
    return true;
  }

  /**
   * Shared cache of rasterised line runs, bounded by **bytes** and evicted least-recently-used.
   *
   * It used to be an `unordered_map` capped at 2048 entries and **cleared entirely** when full. That is
   * a cliff rather than a bound: a list with a couple of thousand distinct labels fills it in one frame,
   * the next frame clears it and rebuilds every line, and the cost jumps about 15x (measured at 1080p:
   * 2010 rows 12 ms, 2012 rows 170 ms, one rebuild per row per frame). Entry count was never a memory
   * bound either -- one run can be hundreds of KiB.
   *
   * A byte budget keeps what the drawn rows actually need, makes the memory ceiling honest, and turns a
   * rebuild into a one-time cost per line. A scene whose whole working set exceeds the budget still
   * rebuilds (that is a culling problem, not a cache problem), but it degrades smoothly instead of
   * clearing everything.
   */
  class RunCache {
   public:
    struct Entry {
      std::shared_ptr<const RunBitmap> run;
      std::list<std::string>::iterator position;
      std::size_t bytes = 0;
    };
    struct Snapshot {
      std::size_t entries = 0;
      std::size_t bytes = 0;
      std::size_t evictions = 0;
    };

    [[nodiscard]] std::shared_ptr<const RunBitmap> find(const std::string& key) {
      const std::lock_guard lock(mMutex);
      const auto found = mEntries.find(key);
      if (found == mEntries.end()) return nullptr;
      mOrder.splice(mOrder.end(), mOrder, found->second.position);
      return found->second.run;
    }

    void insert(std::string key, std::shared_ptr<const RunBitmap> run, const std::size_t bytes) {
      // A run larger than the whole budget is not cacheable: storing it would evict every line that
      // might be reused and leave the cache holding one entry. The caller still has its local copy.
      const std::size_t budget = TextPainter::runCacheBudgetBytes();
      if (budget == 0 || bytes > budget) return;
      const std::lock_guard lock(mMutex);
      const auto found = mEntries.find(key);
      if (found != mEntries.end()) {
        mBytes -= found->second.bytes;
        found->second.run = std::move(run);
        found->second.bytes = bytes;
        mBytes += bytes;
        mOrder.splice(mOrder.end(), mOrder, found->second.position);
      } else {
        mOrder.push_back(key);
        mBytes += bytes;
        mEntries.emplace(std::move(key), Entry{ std::move(run), std::prev(mOrder.end()), bytes });
      }
      while (mEntries.size() > 1 && (mBytes > budget || mEntries.size() > kMaxEntries)) {
        const std::string victim = std::move(mOrder.front());
        mOrder.pop_front();
        const auto entry = mEntries.find(victim);
        if (entry == mEntries.end()) continue;
        mBytes -= entry->second.bytes;
        mEntries.erase(entry);
        ++mEvictions;
      }
    }

    [[nodiscard]] Snapshot snapshot() const {
      const std::lock_guard lock(mMutex);
      return Snapshot{ mEntries.size(), mBytes, mEvictions };
    }

    void clear() {
      const std::lock_guard lock(mMutex);
      mEntries.clear();
      mOrder.clear();
      mBytes = 0;
    }

   private:
    /** Bookkeeping bound: entries are cheap, but an unbounded map is still a leak. */
    static constexpr std::size_t kMaxEntries = 16384;
    mutable std::mutex mMutex;
    std::unordered_map<std::string, Entry> mEntries;
    /** Front is the least recently used key, back the most recently used. */
    std::list<std::string> mOrder;
    std::size_t mBytes = 0;
    std::size_t mEvictions = 0;
  };

  static RunCache& runCache() {
    static RunCache cache;
    return cache;
  }

  /** Bytes a run holds, including the three subpixel strips when it has them. */
  static std::size_t runBytes(const RunBitmap& run) {
    std::size_t total = run.bitmap.data().size();
    for (const graphics::Bitmap& mask: run.subpixelMasks) total += mask.data().size();
    return total;
  }

  const graphics::Font* mSourceFont = nullptr;
  float mScale = 0.0f;
  graphics::Font::Direction mDirection = graphics::Font::Direction::Auto;
  std::shared_ptr<const graphics::Font> mDeviceFont;
  std::string mText;
  std::shared_ptr<const graphics::TextBlob> mBlob;
  std::vector<graphics::FontFeature> mFeatures;
  bool mFeaturesDirty = false;
  bool mHinted = true;
  bool mRunAttempted = false;
  bool mRunBitmapValid = false;
  std::shared_ptr<const RunBitmap> mRunBitmap;
  bool mSubpixelAttempted = false;
  bool mSubpixelValid = false;
  uint32_t mSubpixelColor = 0;
  int mSubpixelPhase = -1;
  uint64_t mSubpixelParamKey = 0;
  std::shared_ptr<const RunBitmap> mSubpixelRun;

 public:
  /** Bytes the shared run cache may hold; 0 disables it (every draw rebuilds its line). */
  static void setRunCacheBudgetBytes(const std::size_t bytes) {
    budgetStorage().store(bytes, std::memory_order_release);
  }
  [[nodiscard]] static std::size_t runCacheBudgetBytes() {
    return budgetStorage().load(std::memory_order_acquire);
  }
  struct RunCacheSnapshot {
    std::size_t entries = 0;
    std::size_t bytes = 0;
    std::size_t evictions = 0;
  };
  [[nodiscard]] static RunCacheSnapshot runCacheSnapshot() {
    const RunCache::Snapshot snapshot = runCache().snapshot();
    return RunCacheSnapshot{ snapshot.entries, snapshot.bytes, snapshot.evictions };
  }
  static void clearRunCache() { runCache().clear(); }

 private:
  static std::atomic<std::size_t>& budgetStorage() {
    // 64 MiB holds roughly three thousand editor-sized lines: enough for the visible rows plus what the
    // recording pass touches, bounded so a pathological scene cannot pin hundreds of megabytes.
    static std::atomic<std::size_t> budget{ 64u * 1024u * 1024u };
    return budget;
  }
};

} // namespace coral::ui

#endif // CORAL_UI_TEXTPAINTER_H
