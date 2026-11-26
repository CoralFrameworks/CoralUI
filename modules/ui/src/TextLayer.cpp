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

#include <ui/SystemFonts.h>
#include <ui/TextBidi.h>
#include <ui/TextLayer.h>
#include <ui/Theme.h>

#include "TextPainter.h"
#include "unicode/GraphemeBreak.h"

#include <graphics/FontFallback.h>
#include <graphics/Paint.h>
#include <graphics/Typeface.h>

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

namespace coral::ui {

namespace {

uint32_t firstCodePoint(const std::string_view text) {
  if (text.empty()) return 0;
  const auto lead = static_cast<unsigned char>(text[0]);
  if (lead < 0x80u) return lead;
  if ((lead & 0xE0u) == 0xC0u && text.size() >= 2) {
    return (static_cast<uint32_t>(lead & 0x1Fu) << 6) |
           static_cast<uint32_t>(static_cast<unsigned char>(text[1]) & 0x3Fu);
  }
  if ((lead & 0xF0u) == 0xE0u && text.size() >= 3) {
    return (static_cast<uint32_t>(lead & 0x0Fu) << 12) |
           (static_cast<uint32_t>(static_cast<unsigned char>(text[1]) & 0x3Fu) << 6) |
           static_cast<uint32_t>(static_cast<unsigned char>(text[2]) & 0x3Fu);
  }
  if ((lead & 0xF8u) == 0xF0u && text.size() >= 4) {
    return (static_cast<uint32_t>(lead & 0x07u) << 18) |
           (static_cast<uint32_t>(static_cast<unsigned char>(text[1]) & 0x3Fu) << 12) |
           (static_cast<uint32_t>(static_cast<unsigned char>(text[2]) & 0x3Fu) << 6) |
           static_cast<uint32_t>(static_cast<unsigned char>(text[3]) & 0x3Fu);
  }
  return 0xFFFDu;
}

bool isUprightGlyph(const uint32_t codePoint) {
  return codePoint >= 0x2E80u;
}

/** Common CJK punctuation mapped to its Unicode vertical presentation form. */
uint32_t verticalPunctuationForm(const uint32_t codePoint) {
  switch (codePoint) {
    case 0x3001: return 0xFE11; // 、
    case 0x3002: return 0xFE12; // 。
    case 0xFF0C: return 0xFE10; // ，
    case 0xFF1A: return 0xFE13; // ：
    case 0xFF1B: return 0xFE14; // ；
    case 0xFF01: return 0xFE15; // ！
    case 0xFF1F: return 0xFE16; // ？
    case 0x300C: return 0xFE41; // 「
    case 0x300D: return 0xFE42; // 」
    case 0x300E: return 0xFE43; // 『
    case 0x300F: return 0xFE44; // 』
    case 0xFF08: return 0xFE35; // （
    case 0xFF09: return 0xFE36; // ）
    default: return 0;
  }
}

std::string expandTabs(const std::string_view text, const uint32_t tabSize) {
  if (tabSize == 0 || text.find('\t') == std::string_view::npos) return std::string(text);
  std::string expanded;
  expanded.reserve(text.size() + tabSize);
  for (const char character: text) {
    if (character == '\t') expanded.append(static_cast<size_t>(tabSize), ' ');
    else expanded.push_back(character);
  }
  return expanded;
}

std::string encodeUtf8(const uint32_t codePoint) {  std::string text;
  if (codePoint < 0x80u) {
    text.push_back(static_cast<char>(codePoint));
  } else if (codePoint < 0x800u) {
    text.push_back(static_cast<char>(0xC0u | (codePoint >> 6)));
    text.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
  } else {
    text.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
    text.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
    text.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
  }
  return text;
}

} // namespace anonymous

TextLayer::TextLayer(std::string text, std::shared_ptr<const graphics::Font> font, const graphics::Color color)
        : mText(std::move(text)),
          mFont(std::move(font)),
          mColor(color) {
  setAccessibleRole(AccessibilityRole::Label);
  setAccessibleName(mText);
}

TextLayer::~TextLayer() = default;

void TextLayer::setText(std::string text) {
  if (mText == text) {
    return;
  }
  if (accessibleName() == mText) setAccessibleName(text);
  mText = std::move(text);
  mLayout = TextLayout{};
  setNeedsLayout();
  setNeedsPaint();
}

void TextLayer::setFont(std::shared_ptr<const graphics::Font> font) {
  if (mFont == font) return;
  mFont = std::move(font);
  markFontChanged();
  if (mSystemFallback) mFallback = systemFontFallback(mFont);
  mLayout = TextLayout{};
  mPainters.clear();
  mRunPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

void TextLayer::setSystemFontFallback(const bool enabled) {
  if (mSystemFallback == enabled) return;
  mSystemFallback = enabled;
  setFontFallback(enabled ? systemFontFallback(mFont) : nullptr);
}

void TextLayer::setFontFallback(std::shared_ptr<const graphics::FontFallback> fallback) {
  mFallback = std::move(fallback);
  mLayout = TextLayout{};
  mPainters.clear();
  mRunPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

float TextLayer::measureText(const std::string_view text, const bool rightToLeft) const {
  if (mFont == nullptr) return 0.0f;
  const std::string expanded = expandTabs(text, mTabSize);
  const std::string_view measured = expanded.empty() ? text : std::string_view(expanded);
  const graphics::Font::Direction direction = rightToLeft
    ? graphics::Font::Direction::RightToLeft : graphics::Font::Direction::LeftToRight;
  if (!mFontFeatures.empty()) {
    return mFont->measureWidth(measured, direction, mFontFeatures);
  }
  if (mFallback != nullptr) return mFallback->measure(measured);
  return detail::TextShapeCache::instance().measure(*mFont, measured, direction);
}

bool TextLayer::setFontVariations(std::vector<graphics::FontVariation> variations,
    std::string* error) {
  if (mFont == nullptr) return false;
  std::shared_ptr<graphics::Font> varied = mFont->withVariations(variations, error);
  if (varied == nullptr) return false;
  setFont(std::move(varied));
  return true;
}

void TextLayer::setFontFeatures(std::vector<graphics::FontFeature> features) {
  mFontFeatures = std::move(features);
  mLayout = TextLayout{};
  mPainters.clear();
  mRunPainters.clear();
  mHyphenPainter.reset();
  setNeedsLayout();
  setNeedsPaint();
}

float TextLayer::xForLineOffset(const TextLine& line, const size_t offset) const {
  if (line.text.empty()) return 0.0f;
  const size_t lineEnd = line.start + line.text.size();
  const size_t clamped = std::clamp(offset, line.start, lineEnd);
  if (line.visualRuns.empty()) {
    const std::string_view prefix = line.text.substr(0, clamped - line.start);
    const auto spaces = static_cast<float>(std::count(prefix.begin(), prefix.end(), ' '));
    return measureText(prefix, false) + spaces * line.justifySpacing;
  }
  const std::vector<uint32_t>& order = line.visualRuns.order();
  float x = 0.0f;
  for (size_t position = 0; position < order.size(); ++position) {
    const TextVisualRun run = line.visualRuns[order[position]];
    if (run.start < line.start || run.end > lineEnd || run.end <= run.start) continue;
    const std::string runText(line.text.substr(run.start - line.start, run.end - run.start));
    const bool rightToLeft = run.level % 2 != 0;
    const std::shared_ptr<const std::string> paintedShared =
      rightToLeft ? mirrorBidiTextShared(runText) : nullptr;
    const std::string_view painted = paintedShared != nullptr
      ? std::string_view(*paintedShared) : std::string_view(runText);
    const float width = measureText(painted, rightToLeft);
    if (clamped <= run.start) return x;
    if (clamped >= run.end) {
      x += width;
      continue;
    }
    const std::string prefix(runText.substr(0, clamped - run.start));
    const std::shared_ptr<const std::string> prefixShared =
      rightToLeft ? mirrorBidiTextShared(prefix) : nullptr;
    const std::string_view paintedPrefix = prefixShared != nullptr
      ? std::string_view(*prefixShared) : std::string_view(prefix);
    const float prefixWidth = measureText(paintedPrefix, rightToLeft);
    return rightToLeft ? x + width - prefixWidth : x + prefixWidth;
  }
  return x;
}

size_t TextLayer::indexAtLine(const TextLine& line, const float localX) const {
  const size_t lineEnd = line.start + line.text.size();
  if (line.text.empty()) return line.start;
  if (line.visualRuns.empty()) {
    const std::vector<size_t> boundaries = unicode::computeGraphemeBreaks(line.text);
    size_t best = line.start;
    float bestDistance = std::abs(localX);
    float advance = 0.0f;
    size_t previous = 0;
    for (const size_t boundary: boundaries) {
      if (boundary == 0) continue;
      const std::string_view grapheme = line.text.substr(previous, boundary - previous);
      advance += measureText(grapheme, false);
      if (grapheme == " ") advance += line.justifySpacing;
      const float distance = std::abs(localX - advance);
      if (distance < bestDistance) {
        bestDistance = distance;
        best = line.start + boundary;
      }
      previous = boundary;
    }
    return best;
  }

  const std::vector<uint32_t>& order = line.visualRuns.order();
  float x = 0.0f;
  for (size_t position = 0; position < order.size(); ++position) {
    const TextVisualRun run = line.visualRuns[order[position]];
    if (run.start < line.start || run.end > lineEnd || run.end <= run.start) continue;
    const std::string runText(line.text.substr(run.start - line.start, run.end - run.start));
    const bool rightToLeft = run.level % 2 != 0;
    const std::shared_ptr<const std::string> paintedShared =
      rightToLeft ? mirrorBidiTextShared(runText) : nullptr;
    const std::string_view painted = paintedShared != nullptr
      ? std::string_view(*paintedShared) : std::string_view(runText);
    const float width = measureText(painted, rightToLeft);
    if (localX > x + width && position + 1 < order.size()) {
      x += width;
      continue;
    }
    float inside = std::clamp(localX - x, 0.0f, width);
    if (rightToLeft) inside = width - inside;
    const std::vector<size_t> boundaries = unicode::computeGraphemeBreaks(runText);
    size_t best = run.start;
    float bestDistance = std::abs(inside);
    float advance = 0.0f;
    size_t previous = 0;
    for (const size_t boundary: boundaries) {
      if (boundary == 0) continue;
      const std::string_view grapheme = std::string_view(runText).substr(previous, boundary - previous);
      if (rightToLeft) {
        const std::shared_ptr<const std::string> paintedGrapheme =
          mirrorBidiTextShared(grapheme);
        advance += measureText(*paintedGrapheme, true);
      } else {
        advance += measureText(grapheme, false);
      }
      const float candidate = advance;
      const float distance = std::abs(inside - candidate);
      if (distance < bestDistance) {
        bestDistance = distance;
        best = run.start + boundary;
      }
      previous = boundary;
    }
    return best;
  }
  return lineEnd;
}

void TextLayer::setColor(const graphics::Color color) {
  mColor = color;
  setNeedsPaint();
}

void TextLayer::setExactSubpixelPhase(const bool exact) {
  if (mExactSubpixelPhase == exact) return;
  mExactSubpixelPhase = exact;
  setNeedsPaint();
}

void TextLayer::setLinearLightLayer(const bool enabled) {
  if (mLinearLightLayer == enabled) return;
  mLinearLightLayer = enabled;
  setNeedsPaint();
}

void TextLayer::setTextBackgroundColor(const graphics::Color color) {
  mTextBackground = color;
  mTextBackgroundBitmap.reset();
  setNeedsPaint();
}

void TextLayer::setTextBackgroundBitmap(std::shared_ptr<const graphics::Bitmap> bitmap) {
  mTextBackgroundBitmap = std::move(bitmap);
  mTextBackground.reset();
  setNeedsPaint();
}

void TextLayer::clearTextBackgroundBitmap() {
  if (mTextBackgroundBitmap == nullptr) return;
  mTextBackgroundBitmap.reset();
  setNeedsPaint();
}

void TextLayer::clearTextBackgroundColor() {
  if (!mTextBackground.has_value() && mTextBackgroundBitmap == nullptr) return;
  mTextBackground.reset();
  mTextBackgroundBitmap.reset();
  setNeedsPaint();
}

void TextLayer::setSelectable(const bool selectable) {
  if (mSelectable == selectable) return;
  mSelectable = selectable;
  if (!mSelectable) clearSelection();
  setNeedsPaint();
}

void TextLayer::setSelection(const size_t start, const size_t end) {
  mSelectionStart = std::min(start, mText.size());
  mSelectionEnd = std::min(end, mText.size());
  setNeedsPaint();
}

void TextLayer::clearSelection() {
  mSelectionStart = 0;
  mSelectionEnd = 0;
  mSelectionAnchor = 0;
  mSelecting = false;
  setNeedsPaint();
}

void TextLayer::setSelectionColor(const graphics::Color color) {
  mSelectionColor = color;
  setNeedsPaint();
}

float TextLayer::lineHeight() const {
  if (mLayout.lineHeight > 0.0f) return mLayout.lineHeight;
  if (mFont == nullptr) return 0.0f;
  return mFont->metrics().ascent + mFont->metrics().descent + mFont->metrics().lineGap;
}

void TextLayer::buildLayoutAt(const float wrap) {
  TextLayoutOptions options{ wrap, mMaxLines, mEllipsis };
  options.policy = mPolicy;
  options.tabSize = mTabSize;
  options.align = mAlign;
  options.lineHeight = mConfiguredLineHeight;
  options.orientation = mOrientation;
  options.direction = mDirection;
  mLayout = mFallback != nullptr
                  ? layoutText(mText, *mFont, *mFallback, options)
                  : layoutText(mText, *mFont, options);
  mLayoutWrapWidth = wrap;
  mPainters.clear();
  mRunPainters.clear();
}

namespace {

/**
 * Whether a bounds width close to the current layout still needs no re-break.
 *
 * A text is measured with the parent's available width, and a Row/table parent then assigns exactly
 * the measured width back as bounds. Shaping at that same width can report the line a fraction of a
 * pixel wider than the measured width, which used to re-break e.g. "Task 1" into "Task" / "1" and
 * overflow the one-line row. If every existing line fits and the box is the tight measured box, there
 * is nothing to re-break. A materially wider box still re-lays out so centre/right/justify offsets use
 * the new width.
 */
bool layoutFitsTightBox(const TextLayout& layout, const float width) {
  if (layout.lines.empty() || layout.size.width <= 0.0f) return false;
  return width + 0.5f >= layout.size.width && width <= layout.size.width + 8.0f;
}

} // namespace anonymous

void TextLayer::ensureLayout() {
  if (mFont == nullptr) return;
  const float wrap = mWrapWidth > 0.0f ? mWrapWidth : bounds().getWidth();
  if (mLayout.lines.empty()) {
    buildLayoutAt(wrap);
    return;
  }
  if (std::abs(wrap - mLayoutWrapWidth) <= 0.01f) return;
  if (layoutFitsTightBox(mLayout, wrap)) return;
  buildLayoutAt(wrap);
}

void TextLayer::onBoundsSizeChanged() {
  if (mFont == nullptr || mWrapWidth > 0.0f) return;
  const float width = bounds().getWidth();
  if (width <= 0.0f || std::abs(width - mLayoutWrapWidth) <= 0.01f) return;
  // `measure()` was called with the parent's available width; the assigned bounds can be narrower
  // (a fixed-width dock panel, a table column, a shrunk flex item), and without this the text keeps
  // the old line breaks and overflows.
  if (layoutFitsTightBox(mLayout, width)) return;
  buildLayoutAt(width);
  setNeedsPaint();
}

void TextLayer::setLineBreakPolicy(const LineBreakPolicy& policy) {
  mPolicyExplicit = true;
  if (mPolicy == policy) return;
  mPolicy = policy;
  mLayout = TextLayout{};
  mPainters.clear();
  mRunPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

void TextLayer::setTabSize(const uint32_t spaces) {
  if (mTabSize == spaces) return;
  mTabSize = spaces;
  mLayout = TextLayout{};
  mPainters.clear();
  mRunPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

void TextLayer::applyTheme(const Theme& theme) {
  if (mPolicyExplicit) return;
  if (mPolicy == theme.lineBreakPolicy) return;
  mPolicy = theme.lineBreakPolicy;
  mLayout = TextLayout{};
  mPainters.clear();
  mRunPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

size_t TextLayer::indexAt(const Point& point) const {
  if (mFont == nullptr || mLayout.lines.empty()) return 0;
  if (mLayout.orientation != TextOrientation::Horizontal) {
    const float columnWidth = std::max(1.0f, lineHeight());
    const bool rightToLeft = mLayout.orientation == TextOrientation::VerticalRl;
    const float localX = rightToLeft
      ? bounds().getRight() - point.x
      : point.x - bounds().getLeft();
    const int column = std::clamp(
      static_cast<int>(localX / columnWidth), 0,
      static_cast<int>(mLayout.lines.size()) - 1);
    const TextLine& line = mLayout.lines[static_cast<size_t>(column)];
    const float local = point.y - bounds().getTop();
    if (local <= 0.0f) {
      if (line.visualRuns.empty()) return line.start;
      const auto& order = line.visualRuns.order();
      if (order.empty()) return line.start;
      const TextVisualRun first = line.visualRuns[order.front()];
      return first.level % 2 != 0 ? first.end : first.start;
    }
    if (!line.visualRuns.empty()) {
      float runTop = 0.0f;
      size_t lastStart = line.start;
      size_t lastEnd = line.end;
      bool lastRtl = false;
      for (const uint32_t runIndex: line.visualRuns.order()) {
        const TextVisualRun run = line.visualRuns[runIndex];
        if (run.start < line.start || run.end > line.start + line.text.size() ||
            run.end <= run.start) {
          continue;
        }
        lastStart = run.start;
        lastEnd = run.end;
        lastRtl = run.level % 2 != 0;
        const std::string_view runText = line.text.substr(run.start - line.start, run.end - run.start);
        const std::vector<size_t> runBoundaries = unicode::computeGraphemeBreaks(runText);
        float width = 0.0f;
        for (size_t position = 0; position < runBoundaries.size(); ++position) {
          const size_t previous = position == 0 ? 0 : runBoundaries[position - 1];
          width += measureText(runText.substr(previous, runBoundaries[position] - previous), false);
        }
        if (local > runTop + width) {
          runTop += width;
          continue;
        }
        const bool rtl = run.level % 2 != 0;
        const float inside = std::clamp(local - runTop, 0.0f, width);
        float advance = 0.0f;
        size_t best = run.start;
        float bestDistance = std::abs(inside - (rtl ? width : 0.0f));
        size_t previous = 0;
        for (const size_t boundary: runBoundaries) {
          advance += measureText(runText.substr(previous, boundary - previous), false);
          const float candidate = rtl ? width - advance : advance;
          const float distance = std::abs(inside - candidate);
          if (distance < bestDistance) {
            bestDistance = distance;
            best = run.start + boundary;
          }
          previous = boundary;
        }
        return best;
      }
      return lastRtl ? lastStart : lastEnd;
    }
    const std::vector<size_t> boundaries = unicode::computeGraphemeBreaks(line.text);
    size_t best = line.start;
    float bestDistance = std::abs(local);
    for (const size_t boundary: boundaries) {
      if (boundary == 0) continue;
      const float advance = measureText(line.text.substr(0, boundary), false);
      const float distance = std::abs(local - advance);
      if (distance < bestDistance) {
        bestDistance = distance;
        best = line.start + boundary;
      }
    }
    return best;
  }
  const int lineIndex = std::clamp(static_cast<int>((point.y - bounds().getTop()) / lineHeight()), 0,
          static_cast<int>(mLayout.lines.size()) - 1);
  const TextLine& line = mLayout.lines[static_cast<size_t>(lineIndex)];
  const float local = point.x - bounds().getLeft() - line.xOffset;
  if (local <= 0.0f) return line.start;
  return indexAtLine(line, local);
}

bool TextLayer::onPointerDown(const PointerEvent& event) {
  if (!mSelectable) return false;
  ensureLayout();
  const size_t index = indexAt(event.position);
  mSelectionAnchor = index;
  mSelectionStart = index;
  mSelectionEnd = index;
  mSelecting = true;
  setNeedsPaint();
  return true;
}

bool TextLayer::onPointerMove(const PointerEvent& event) {
  if (!mSelectable || !mSelecting) return false;
  ensureLayout();
  mSelectionEnd = indexAt(event.position);
  setNeedsPaint();
  return true;
}

bool TextLayer::onPointerUp(const PointerEvent& event) {
  (void) event;
  if (!mSelecting) return false;
  mSelecting = false;
  return true;
}

void TextLayer::setMaxLines(const int lines) {
  const int next = std::max(0, lines);
  if (mMaxLines == next) return;
  mMaxLines = next;
  mLayout = TextLayout{};
  setNeedsLayout();
  setNeedsPaint();
}

void TextLayer::setEllipsis(const bool ellipsis) {
  if (mEllipsis == ellipsis) return;
  mEllipsis = ellipsis;
  mLayout = TextLayout{};
  setNeedsLayout();
  setNeedsPaint();
}

void TextLayer::setTextAlign(const TextAlign align) {
  if (mAlign == align) return;
  mAlign = align;
  mLayout = TextLayout{};
  setNeedsLayout();
  setNeedsPaint();
}

void TextLayer::setLineHeight(const float height) {
  const float next = std::max(0.0f, height);
  if (mConfiguredLineHeight == next) return;
  mConfiguredLineHeight = next;
  mLayout = TextLayout{};
  setNeedsLayout();
  setNeedsPaint();
}

void TextLayer::setOrientation(const TextOrientation orientation) {
  if (mOrientation == orientation) return;
  mOrientation = orientation;
  mLayout = TextLayout{};
  setNeedsLayout();
  setNeedsPaint();
}

void TextLayer::setDirection(const TextDirectionMode direction) {
  if (mDirection == direction) return;
  mDirection = direction;
  mLayout = TextLayout{};
  setNeedsLayout();
  setNeedsPaint();
}

void TextLayer::setWrapWidth(const float width) {
  if (mWrapWidth == width) {
    return;
  }
  mWrapWidth = std::max(0.0f, width);
  mLayout = TextLayout{};
  setNeedsLayout();
  setNeedsPaint();
}

Size TextLayer::measure(const Size& available) {
  if (mFont == nullptr) {
    mLayout = TextLayout{};
    setMeasuredSize({ 0.0f, 0.0f });
    return measuredSize();
  }
  float wrap = mWrapWidth;
  if (wrap <= 0.0f && available.width > 0.0f) {
    wrap = available.width;
  }
  // A layer with a fixed width wraps to that width even when the parent offers infinity.
  if (wrap <= 0.0f && bounds().getWidth() > 0.0f) {
    wrap = bounds().getWidth();
  }
  buildLayoutAt(wrap);
  setMeasuredSize(mLayout.size);
  return measuredSize();
}

void TextLayer::drawSelf(RenderContext& context) {
  const bool linearLayer = mLinearLightLayer;
  if (!linearLayer) {
    drawSelfContent(context);
    return;
  }
  const graphics::SubpixelRenderingParams params = graphics::subpixelRenderingParams();
  graphics::Paint layerPaint;
  layerPaint.color = mColor;
  layerPaint.blendMode = graphics::BlendMode::kPlus;
  layerPaint.coverageOnly = true;
  layerPaint.linearLightLayer = true;
  layerPaint.linearLightGamma = params.gamma;
  layerPaint.linearLightContrast = params.contrast;
  context.canvas().saveLayer(bounds(), layerPaint);
  drawSelfContent(context);
  context.canvas().restore();
}

void TextLayer::drawSelfContent(RenderContext& context) {
  if (mFont == nullptr || mText.empty()) {
    return;
  }
  ensureLayout();
  const bool linearLayer = mLinearLightLayer;
  const float lineHeightValue = lineHeight();
  if (mLayout.orientation != TextOrientation::Horizontal) {
    const graphics::FontMetrics& metrics = mFont->metrics();
    const bool rightToLeft = mLayout.orientation == TextOrientation::VerticalRl;
    const bool mixed = mLayout.orientation != TextOrientation::Vertical;
    graphics::Paint paint;
    paint.color = mColor;
    paint.style = graphics::Paint::Style::kFill;
    if (mTextBackground.has_value() || mTextBackgroundBitmap != nullptr) {
      paint.hasTextBackground = true;
      if (mTextBackground.has_value()) paint.textBackground = *mTextBackground;
      paint.textBackgroundBitmap = mTextBackgroundBitmap.get();
    }
    paint.exactSubpixelPhase = mExactSubpixelPhase;
    if (linearLayer) {
      paint.coverageOnly = true;
      paint.blendMode = graphics::BlendMode::kPlus;
      paint.hasTextBackground = false;
      paint.textBackgroundBitmap = nullptr;
    }
    graphics::Paint highlight;
    highlight.color = mSelectionColor;
    highlight.style = graphics::Paint::Style::kFill;
    for (size_t index = 0; index < mLayout.lines.size(); ++index) {
      const TextLine& line = mLayout.lines[index];
      const float columnX = rightToLeft
        ? bounds().getRight() - lineHeightValue * static_cast<float>(index + 1)
        : bounds().getLeft() + lineHeightValue * static_cast<float>(index);
      if (index >= mPainters.size()) mPainters.resize(index + 1);
      if (mPainters[index] == nullptr) mPainters[index] = std::make_unique<TextPainter>();
      float y = bounds().getTop();
      const auto drawRun = [&](const std::string_view runText, const size_t runStart,
          const bool rtl) {
        const std::vector<size_t> boundaries = unicode::computeGraphemeBreaks(runText);
        const auto drawGrapheme = [&](const size_t previous, const size_t boundary) {
          if (boundary == 0) return;
          const std::string_view grapheme = runText.substr(previous, boundary - previous);
          const uint32_t codePoint = firstCodePoint(grapheme);
          const bool tab = grapheme == "\t";
          const bool upright = !mixed || isUprightGlyph(codePoint);
          std::string shown(grapheme);
          if (upright) {
            const uint32_t form = verticalPunctuationForm(codePoint);
            if (form != 0 && mFont->typeface()->hasGlyphForCodePoint(form)) {
              shown = encodeUtf8(form);
            }
          }
          const float advance = tab && mTabSize > 0
            ? measureText(" ", false) * static_cast<float>(mTabSize)
            : measureText(shown, false) + (grapheme == " " ? line.justifySpacing : 0.0f);
          if (hasSelection() && selectionStart() < runStart + boundary &&
              selectionEnd() > runStart + previous) {
            context.canvas().drawRect(
              Rect::MakeXYWH(columnX, y, std::max(1.0f, lineHeightValue), std::max(1.0f, advance)),
              highlight);
          }
          if (!tab) {
            if (upright) {
              mPainters[index]->setFeatures(mFontFeatures);
              mPainters[index]->draw(context, shown, columnX, y + metrics.ascent, *mFont, paint);
            } else {
              // text-orientation: mixed rotates Latin/digits 90° clockwise.
              context.canvas().save();
              context.canvas().translate(columnX + metrics.ascent, y);
              context.canvas().rotate(90.0f);
              mPainters[index]->setFeatures(mFontFeatures);
              mPainters[index]->draw(context, shown, 0.0f, 0.0f, *mFont, paint);
              context.canvas().restore();
            }
          }
          y += advance;
        };
        if (rtl) {
          for (size_t position = boundaries.size(); position-- > 0;) {
            drawGrapheme(position == 0 ? 0 : boundaries[position - 1], boundaries[position]);
          }
        } else {
          size_t previous = 0;
          for (const size_t boundary: boundaries) {
            drawGrapheme(previous, boundary);
            previous = boundary;
          }
        }
      };
      if (line.visualRuns.empty()) {
        drawRun(line.text, line.start, false);
      } else {
        for (const uint32_t runIndex: line.visualRuns.order()) {
          const TextVisualRun run = line.visualRuns[runIndex];
          if (run.start < line.start || run.end > line.start + line.text.size() ||
              run.end <= run.start) {
            continue;
          }
          drawRun(line.text.substr(run.start - line.start, run.end - run.start),
            run.start, run.level % 2 != 0);
        }
      }
    }
    return;
  }
  if (hasSelection()) {
    graphics::Paint selection;
    selection.color = mSelectionColor;
    selection.style = graphics::Paint::Style::kFill;
    for (size_t index = 0; index < mLayout.lines.size(); ++index) {
      const TextLine& line = mLayout.lines[index];
      const size_t begin = std::max(selectionStart(), line.start);
      const size_t end = std::min(selectionEnd(), line.end);
      if (begin >= end) continue;
      const float top = bounds().getTop() + static_cast<float>(index) * lineHeightValue;
      const auto paintSpan = [&](const size_t spanBegin, const size_t spanEnd) {
        if (spanBegin >= spanEnd) return;
        const float left = bounds().getLeft() + line.xOffset + xForLineOffset(line, spanBegin);
        const float right = bounds().getLeft() + line.xOffset + xForLineOffset(line, spanEnd);
        context.canvas().drawRect(Rect::MakeXYWH(std::min(left, right), top,
                                          std::max(1.0f, std::abs(right - left)), lineHeightValue),
                selection);
      };
      if (line.visualRuns.empty()) {
        paintSpan(begin, end);
        continue;
      }
      // A visual line can consist of several reordered pieces; draw each
      // intersection with its own visual x range.
      for (const uint32_t runIndex: line.visualRuns.order()) {
        const TextVisualRun run = line.visualRuns[runIndex];
        paintSpan(std::max(begin, run.start), std::min(end, run.end));
      }
    }
  }
  graphics::Paint paint;
  paint.color = mColor;
  paint.style = graphics::Paint::Style::kFill;
  if (mTextBackground.has_value() || mTextBackgroundBitmap != nullptr) {
    paint.hasTextBackground = true;
    if (mTextBackground.has_value()) paint.textBackground = *mTextBackground;
    paint.textBackgroundBitmap = mTextBackgroundBitmap.get();
  }
  paint.exactSubpixelPhase = mExactSubpixelPhase;
  if (linearLayer) {
    paint.coverageOnly = true;
    paint.blendMode = graphics::BlendMode::kPlus;
    paint.hasTextBackground = false;
    paint.textBackgroundBitmap = nullptr;
  }
  const graphics::FontMetrics& metrics = mFont->metrics();
  const float naturalLineHeight = metrics.ascent + metrics.descent + metrics.lineGap;
  const float drawingLineHeight = lineHeight();
  const float leading = std::max(0.0f, drawingLineHeight - naturalLineHeight);
  const std::string hyphenText = "\xE2\x80\x90";
  const float hyphenWidth = mFallback != nullptr
    ? mFallback->measure(hyphenText)
    : detail::TextShapeCache::instance().measure(*mFont, hyphenText);
  const auto drawHyphen = [&](const TextLine& line, const float baseline) {
    if (!line.hyphenated) return;
    if (mHyphenPainter == nullptr) mHyphenPainter = std::make_unique<TextPainter>();
    const auto spaces = static_cast<float>(std::count(line.text.begin(), line.text.end(), ' '));
    mHyphenPainter->setFeatures(mFontFeatures);
    mHyphenPainter->draw(context, hyphenText,
      bounds().getLeft() + line.xOffset + line.width +
        spaces * line.justifySpacing - hyphenWidth,
      baseline, *mFont, paint);
  };
  if (mPainters.size() < mLayout.lines.size()) {
    mPainters.resize(mLayout.lines.size());
  }
  float baseline = bounds().getTop() + leading * 0.5f + metrics.ascent;
  for (size_t index = 0; index < mLayout.lines.size(); ++index) {
    const TextLine& line = mLayout.lines[index];
    if (!line.visualRuns.empty()) {
      const std::vector<uint32_t>& order = line.visualRuns.order();
      float x = bounds().getLeft() + line.xOffset;
      for (const size_t runIndex: order) {
        const TextVisualRun& run = line.visualRuns[runIndex];
        if (run.start < line.start || run.end > line.start + line.text.size()) continue;
        const std::string runText(line.text.substr(run.start - line.start, run.end - run.start));
        const bool rightToLeft = run.level % 2 != 0;
        const std::shared_ptr<const std::string> paintedShared =
          rightToLeft ? mirrorBidiTextShared(runText) : nullptr;
        const std::string_view paintedText = paintedShared != nullptr
          ? std::string_view(*paintedShared) : std::string_view(runText);
        const std::string paintedDisplay = expandTabs(paintedText, mTabSize);
        const graphics::Font::Direction direction =
                rightToLeft ? graphics::Font::Direction::RightToLeft : graphics::Font::Direction::LeftToRight;
        if (mFallback != nullptr) {
          const std::vector<graphics::FontRun> runs = mFallback->shape(paintedDisplay);
          for (const graphics::FontRun& fontRun: runs) {
            if (fontRun.font == nullptr) continue;
            TextPainter painter;
            painter.setFeatures(mFontFeatures);
            painter.draw(context, fontRun.text, x, baseline, *fontRun.font, paint, direction);
            x += fontRun.width;
          }
        } else {
          if (mPainters[index] == nullptr) {
            mPainters[index] = std::make_unique<TextPainter>();
          }
          std::shared_ptr<const graphics::TextBlob> runBlob;
          const auto lazyRun = line.visualRuns.lazy(runIndex);
          if (runBlob == nullptr && lazyRun != nullptr) runBlob = lazyRun->get();
          mPainters[index]->setFeatures(mFontFeatures);
          mPainters[index]->draw(context, paintedDisplay, x, baseline, *mFont, paint, direction, runBlob);
          x += detail::TextShapeCache::instance().measure(*mFont, paintedDisplay, direction);
        }
      }
      drawHyphen(line, baseline);
      baseline += drawingLineHeight;
      continue;
    }
    if (mFallback != nullptr) {
      const std::vector<graphics::FontRun> runs = mFallback->shape(line.text);
      if (mRunPainters.size() <= index) mRunPainters.resize(index + 1);
      if (mRunPainters[index].size() < runs.size()) mRunPainters[index].resize(runs.size());
      float x = bounds().getLeft() + line.xOffset;
      for (size_t runIndex = 0; runIndex < runs.size(); ++runIndex) {
        const graphics::FontRun& run = runs[runIndex];
        if (run.font == nullptr) continue;
        if (mRunPainters[index][runIndex] == nullptr) {
          mRunPainters[index][runIndex] = std::make_unique<TextPainter>();
        }
        mRunPainters[index][runIndex]->setFeatures(mFontFeatures);
        mRunPainters[index][runIndex]->draw(context, run.text, x, baseline, *run.font, paint);
        x += run.width;
      }
    } else {
      if (mPainters[index] == nullptr) {
        mPainters[index] = std::make_unique<TextPainter>();
      }
      if (line.justifySpacing > 0.0f) {
        // Draw word by word so the justification advance can be inserted after
        // each space; shape each token instead of reusing the whole-line blob.
        float x = bounds().getLeft() + line.xOffset;
        size_t begin = 0;
        while (begin <= line.text.size()) {
          const size_t space = line.text.find(' ', begin);
          const std::string_view token = line.text.substr(begin,
            space == std::string_view::npos ? std::string_view::npos : space - begin);
          if (!token.empty()) {
            const std::string display = expandTabs(token, mTabSize);
            mPainters[index]->setFeatures(mFontFeatures);
            mPainters[index]->draw(context, display, x, baseline, *mFont, paint);
            x += measureText(token, false);
          }
          if (space == std::string_view::npos) break;
          x += measureText(" ", false) + line.justifySpacing;
          begin = space + 1;
        }
      } else {
        const std::string display = expandTabs(line.text, mTabSize);
        mPainters[index]->setFeatures(mFontFeatures);
        if (line.rtl) {
          mPainters[index]->draw(context, display, bounds().getLeft() + line.xOffset,
            baseline, *mFont, paint, graphics::Font::Direction::RightToLeft);
        } else {
          mPainters[index]->draw(context, display, bounds().getLeft() + line.xOffset, baseline, *mFont, paint,
              graphics::Font::Direction::Auto, line.shaped);
        }
      }
    }
    drawHyphen(line, baseline);
    baseline += drawingLineHeight;
  }
}


Size TextLayer::intrinsicSize() const {
  if (mFont == nullptr) return Size{ -1.0f, -1.0f };
  if (mFallback != nullptr) {
    const float width = mFallback->measure(mText);
    return Size{ width, mFont->metrics().ascent + mFont->metrics().descent };
  }
  return Size{ mFont->measureWidth(mText),
    mFont->metrics().ascent + mFont->metrics().descent };
}

float TextLayer::baselineOffset() const {
  return mFont != nullptr ? mFont->metrics().ascent : -1.0f;
}
} // namespace coral::ui
