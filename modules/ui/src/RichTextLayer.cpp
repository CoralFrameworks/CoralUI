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

#include <ui/RichTextLayer.h>
#include <ui/Theme.h>
#include <ui/SystemFonts.h>
#include <ui/TextBidi.h>

#include "TextPainter.h"
#include "unicode/GraphemeBreak.h"

#include <graphics/Paint.h>

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

uint32_t verticalPunctuationForm(const uint32_t codePoint) {
  switch (codePoint) {
    case 0x3001: return 0xFE11;
    case 0x3002: return 0xFE12;
    case 0xFF0C: return 0xFE10;
    case 0xFF1A: return 0xFE13;
    case 0xFF1B: return 0xFE14;
    case 0xFF01: return 0xFE15;
    case 0xFF1F: return 0xFE16;
    case 0x300C: return 0xFE41;
    case 0x300D: return 0xFE42;
    case 0x300E: return 0xFE43;
    case 0x300F: return 0xFE44;
    case 0xFF08: return 0xFE35;
    case 0xFF09: return 0xFE36;
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
  } else if (codePoint < 0x10000u) {
    text.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
    text.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
    text.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
  } else {
    text.push_back(static_cast<char>(0xF0u | (codePoint >> 18)));
    text.push_back(static_cast<char>(0x80u | ((codePoint >> 12) & 0x3Fu)));
    text.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
    text.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
  }
  return text;
}

} // namespace

RichTextLayer::RichTextLayer(std::vector<RichTextRun> runs)
        : mRuns(std::move(runs)) {}

RichTextLayer::~RichTextLayer() = default;

void RichTextLayer::setRuns(std::vector<RichTextRun> runs) {
  mRuns = std::move(runs);
  mLayout = RichTextLayout{};
  mPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

void RichTextLayer::setSelectable(const bool selectable) {
  if (mSelectable == selectable) return;
  mSelectable = selectable;
  if (!mSelectable) clearSelection();
  setNeedsPaint();
}

void RichTextLayer::setSelection(const size_t start, const size_t end) {
  mSelectionStart = start;
  mSelectionEnd = end;
  setNeedsPaint();
}

void RichTextLayer::clearSelection() {
  mSelectionStart = 0;
  mSelectionEnd = 0;
  mSelectionAnchor = 0;
  mSelecting = false;
  setNeedsPaint();
}

void RichTextLayer::setSelectionColor(const graphics::Color color) {
  mSelectionColor = color;
  setNeedsPaint();
}

size_t RichTextLayer::indexAt(const Point& point) const {
  if (mLayout.lines.empty()) return 0;
  if (mLayout.orientation != TextOrientation::Horizontal) {
    const float columnWidth = mLayout.lineHeight > 0.0f ? mLayout.lineHeight : 1.0f;
    const bool rightToLeft = mLayout.orientation == TextOrientation::VerticalRl;
    const float localX = rightToLeft
      ? bounds().getRight() - point.x
      : point.x - bounds().getLeft();
    const int column = std::clamp(static_cast<int>(localX / columnWidth), 0,
      static_cast<int>(mLayout.lines.size()) - 1);
    const RichTextLine& line = mLayout.lines[static_cast<size_t>(column)];
    const float localY = point.y - bounds().getTop();
    if (localY <= 0.0f) return line.start;
    if (!line.visualRuns.empty()) {
      float runTop = 0.0f;
      size_t lastStart = line.start;
      size_t lastEnd = line.end;
      bool lastRtl = false;
      for (const uint32_t runIndex: line.visualOrder) {
        const TextVisualRun& run = line.visualRuns[runIndex];
        const auto found = std::upper_bound(line.segments.begin(), line.segments.end(), run.start,
          [](const size_t value, const RichTextSegment& segment) { return value < segment.start; });
        if (found == line.segments.begin()) continue;
        const RichTextSegment& segment = *(found - 1);
        if (segment.font == nullptr || run.start < segment.start || run.end > segment.end) continue;
        lastStart = run.start;
        lastEnd = run.end;
        lastRtl = run.level % 2 != 0;
        const std::string runText = segment.text.substr(run.start - segment.start, run.end - run.start);
        const std::vector<size_t> runBoundaries = unicode::computeGraphemeBreaks(runText);
        float width = 0.0f;
        for (size_t position = 0; position < runBoundaries.size(); ++position) {
          const size_t previous = position == 0 ? 0 : runBoundaries[position - 1];
          width += detail::TextShapeCache::instance().measure(*segment.font,
            std::string_view(runText).substr(previous, runBoundaries[position] - previous));
        }
        if (localY > runTop + width) {
          runTop += width;
          continue;
        }
        const float inside = std::clamp(localY - runTop, 0.0f, width);
        float advance = 0.0f;
        size_t best = run.start;
        float bestDistance = std::abs(inside - (lastRtl ? width : 0.0f));
        size_t previous = 0;
        for (const size_t boundary: runBoundaries) {
          advance += detail::TextShapeCache::instance().measure(*segment.font,
            std::string_view(runText).substr(previous, boundary - previous));
          const float candidate = lastRtl ? width - advance : advance;
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
    size_t best = line.start;
    float bestDistance = std::abs(localY);
    float advance = 0.0f;
    for (const RichTextSegment& segment: line.segments) {
      if (segment.font == nullptr) continue;
      const std::vector<size_t> boundaries = unicode::computeGraphemeBreaks(segment.text);
      size_t previous = 0;
      for (const size_t boundary: boundaries) {
        if (boundary == 0) continue;
        const std::string_view grapheme = std::string_view(segment.text).substr(previous, boundary - previous);
        advance += detail::TextShapeCache::instance().measure(*segment.font, grapheme);
        const float distance = std::abs(localY - advance);
        if (distance < bestDistance) {
          bestDistance = distance;
          best = segment.start + boundary;
        }
        previous = boundary;
      }
    }
    return best;
  }
  float top = bounds().getTop();
  size_t lineIndex = 0;
  for (; lineIndex < mLayout.lines.size(); ++lineIndex) {
    const RichTextLine& line = mLayout.lines[lineIndex];
    if (point.y < top + line.height || lineIndex + 1 == mLayout.lines.size()) break;
    top += line.height;
  }
  const RichTextLine& line = mLayout.lines[std::min(lineIndex, mLayout.lines.size() - 1)];
  float local = point.x - bounds().getLeft() - line.xOffset;
  if (local <= 0.0f) return line.start;
  if (!line.visualRuns.empty()) {
    const std::vector<uint32_t>& order = line.visualOrder;
    float x = 0.0f;
    for (size_t position = 0; position < order.size(); ++position) {
      const TextVisualRun& run = line.visualRuns[order[position]];
      const auto found = std::upper_bound(line.segments.begin(), line.segments.end(), run.start,
              [](const size_t value, const RichTextSegment& segment) { return value < segment.start; });
      if (found == line.segments.begin()) continue;
      const RichTextSegment& segment = *(found - 1);
      if (segment.font == nullptr || run.start < segment.start || run.end > segment.end) continue;
      const std::string runText = segment.text.substr(run.start - segment.start, run.end - run.start);
      const bool rightToLeft = run.level % 2 != 0;
      const graphics::Font::Direction direction =
              rightToLeft ? graphics::Font::Direction::RightToLeft : graphics::Font::Direction::LeftToRight;
      const std::string paintedText = rightToLeft ? mirrorBidiText(runText) : runText;
      const float width = detail::TextShapeCache::instance().measure(*segment.font, paintedText, direction);
      if (local > x + width && position + 1 < order.size()) {
        x += width;
        continue;
      }
      float inside = std::clamp(local - x, 0.0f, width);
      if (rightToLeft) inside = width - inside;
      size_t index = 0;
      size_t best = 0;
      float bestDistance = std::abs(inside);
      while (index < runText.size()) {
        size_t next = index + 1;
        while (next < runText.size() && (static_cast<unsigned char>(runText[next]) & 0xC0u) == 0x80u) ++next;
        const std::string prefix = runText.substr(0, next);
        const std::string paintedPrefix = rightToLeft ? mirrorBidiText(prefix) : prefix;
        const float prefixWidth = detail::TextShapeCache::instance().measure(*segment.font, paintedPrefix, direction);
        const float distance = std::abs(inside - prefixWidth);
        if (distance < bestDistance) {
          bestDistance = distance;
          best = next;
        }
        index = next;
      }
      return run.start + best;
    }
    return line.end;
  }

  for (const RichTextSegment& segment: line.segments) {
    if (segment.font == nullptr) continue;
    const auto segmentSpaces = static_cast<size_t>(
      std::count(segment.text.begin(), segment.text.end(), ' '));
    const float segmentSpan = segment.width +
      static_cast<float>(segmentSpaces) * line.justifySpacing;
    if (local <= segmentSpan || &segment == &line.segments.back()) {
      size_t index = 0;
      size_t best = 0;
      float bestDistance = std::abs(local);
      while (index < segment.text.size()) {
        size_t next = index + 1;
        while (next < segment.text.size() && (static_cast<unsigned char>(segment.text[next]) & 0xC0u) == 0x80u) ++next;
        const std::string_view prefix = std::string_view(segment.text).substr(0, next);
        const auto spaces = static_cast<float>(std::count(prefix.begin(), prefix.end(), ' '));
        const float width = segment.font->measureWidth(prefix) + spaces * line.justifySpacing;
        const float distance = std::abs(local - width);
        if (distance < bestDistance) {
          bestDistance = distance;
          best = next;
        }
        index = next;
      }
      return segment.start + best;
    }
    local -= segmentSpan;
  }
  return line.end;
}

bool RichTextLayer::onPointerDown(const PointerEvent& event) {
  if (!mSelectable) return false;
  if (mLayout.lines.empty()) {
    const std::vector<RichTextRun> runs = layoutRuns();
    mLayout = layoutRichText(runs,
      layoutOptions(mWrapWidth > 0.0f ? mWrapWidth : bounds().getWidth()));
  }
  const size_t index = indexAt(event.position);
  mSelectionAnchor = index;
  mSelectionStart = index;
  mSelectionEnd = index;
  mSelecting = true;
  setNeedsPaint();
  return true;
}

bool RichTextLayer::onPointerMove(const PointerEvent& event) {
  if (!mSelectable || !mSelecting) return false;
  mSelectionEnd = indexAt(event.position);
  setNeedsPaint();
  return true;
}

bool RichTextLayer::onPointerUp(const PointerEvent& event) {
  (void) event;
  if (!mSelecting) return false;
  mSelecting = false;
  return true;
}

void RichTextLayer::setWrapWidth(const float width) {
  if (mWrapWidth == width) return;
  mWrapWidth = std::max(0.0f, width);
  mLayout = RichTextLayout{};
  mPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

void RichTextLayer::setLineBreakPolicy(const LineBreakPolicy& policy) {
  mPolicyExplicit = true;
  if (mPolicy == policy) return;
  mPolicy = policy;
  mLayout = RichTextLayout{};
  mPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

void RichTextLayer::setTabSize(const uint32_t spaces) {
  if (mTabSize == spaces) return;
  mTabSize = spaces;
  mLayout = RichTextLayout{};
  mPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

void RichTextLayer::applyTheme(const Theme& theme) {
  if (mPolicyExplicit) return;
  if (mPolicy == theme.lineBreakPolicy) return;
  mPolicy = theme.lineBreakPolicy;
  mLayout = RichTextLayout{};
  mPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

std::vector<RichTextRun> RichTextLayer::layoutRuns() const {
  if (!mSystemFallback) return mRuns;
  std::vector<RichTextRun> runs = mRuns;
  for (RichTextRun& run: runs) {
    if (run.font != nullptr && run.fallback == nullptr) {
      run.fallback = systemFontFallback(run.font);
    }
  }
  return runs;
}

void RichTextLayer::setSystemFontFallback(const bool enabled) {
  if (mSystemFallback == enabled) return;
  mSystemFallback = enabled;
  mLayout = RichTextLayout{};
  mPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

void RichTextLayer::setLineHeight(const float height) {
  if (mLineHeight == height) return;
  mLineHeight = height;
  mLayout = RichTextLayout{};
  mPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

void RichTextLayer::setTextAlign(const TextAlign align) {
  if (mAlign == align) return;
  mAlign = align;
  mLayout = RichTextLayout{};
  mPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

void RichTextLayer::setOrientation(const TextOrientation orientation) {
  if (mOrientation == orientation) return;
  mOrientation = orientation;
  mLayout = RichTextLayout{};
  mPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

void RichTextLayer::setDirection(const TextDirectionMode direction) {
  if (mDirection == direction) return;
  mDirection = direction;
  mLayout = RichTextLayout{};
  mPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

void RichTextLayer::setMaxLines(const int lines) {
  const int next = std::max(0, lines);
  if (mMaxLines == next) return;
  mMaxLines = next;
  mLayout = RichTextLayout{};
  mPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

void RichTextLayer::setEllipsis(const bool ellipsis) {
  if (mEllipsis == ellipsis) return;
  mEllipsis = ellipsis;
  mLayout = RichTextLayout{};
  mPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

RichTextLayoutOptions RichTextLayer::layoutOptions(const float wrapWidth) const {
  RichTextLayoutOptions options;
  options.maxWidth = wrapWidth;
  options.lineHeight = mLineHeight;
  options.align = mAlign;
  options.policy = mPolicy;
  options.tabSize = mTabSize;
  options.maxLines = mMaxLines;
  options.ellipsis = mEllipsis;
  options.orientation = mOrientation;
  options.direction = mDirection;
  return options;
}

Size RichTextLayer::measure(const Size& available) {
  float wrap = mWrapWidth;
  if (wrap <= 0.0f && available.width > 0.0f) wrap = available.width;
  if (wrap <= 0.0f && bounds().getWidth() > 0.0f) wrap = bounds().getWidth();
  const std::vector<RichTextRun> runs = layoutRuns();
  mLayout = layoutRichText(runs, layoutOptions(wrap));
  setMeasuredSize(mLayout.size);
  return measuredSize();
}

void RichTextLayer::drawSelf(RenderContext& context) {
  if (mLayout.empty()) {
    const std::vector<RichTextRun> runs = layoutRuns();
    mLayout = layoutRichText(runs,
      layoutOptions(mWrapWidth > 0.0f ? mWrapWidth : bounds().getWidth()));
  }
  auto& canvas = context.canvas();
  if (mPainters.size() < mLayout.lines.size()) {
    mPainters.resize(mLayout.lines.size());
  }
  if (mLayout.orientation != TextOrientation::Horizontal) {
    const float columnWidth = mLayout.lineHeight > 0.0f ? mLayout.lineHeight : 1.0f;
    const bool rightToLeft = mLayout.orientation == TextOrientation::VerticalRl;
    const bool mixed = mLayout.orientation != TextOrientation::Vertical;
    graphics::Paint paint;
    paint.style = graphics::Paint::Style::kFill;
    graphics::Paint highlight;
    highlight.color = mSelectionColor;
    highlight.style = graphics::Paint::Style::kFill;
    for (size_t lineIndex = 0; lineIndex < mLayout.lines.size(); ++lineIndex) {
      const RichTextLine& line = mLayout.lines[lineIndex];
      const float columnX = rightToLeft
        ? bounds().getRight() - columnWidth * static_cast<float>(lineIndex + 1)
        : bounds().getLeft() + columnWidth * static_cast<float>(lineIndex);
      float y = bounds().getTop();
      if (mPainters[lineIndex].empty()) mPainters[lineIndex].resize(1);
      const auto drawGrapheme = [&](const RichTextSegment& segment,
          const std::string_view grapheme, const size_t beginOffset, const size_t endOffset) {
        if (segment.font == nullptr) return;
        const graphics::FontMetrics& metrics = segment.font->metrics();
        const bool upright = !mixed || isUprightGlyph(firstCodePoint(grapheme));
        std::string shown(grapheme);
        if (upright) {
          const uint32_t form = verticalPunctuationForm(firstCodePoint(grapheme));
          if (form != 0 && segment.font->typeface()->hasGlyphForCodePoint(form)) {
            shown = encodeUtf8(form);
          }
        }
        const bool tab = grapheme == "\t";
        const float advance = tab && mTabSize > 0
          ? detail::TextShapeCache::instance().measure(*segment.font, " ") *
              static_cast<float>(mTabSize)
          : detail::TextShapeCache::instance().measure(*segment.font, shown) +
              (grapheme == " " ? line.justifySpacing : 0.0f);
        if (hasSelection() && selectionStart() < endOffset && selectionEnd() > beginOffset) {
          context.canvas().drawRect(
            Rect::MakeXYWH(columnX, y, std::max(1.0f, columnWidth), std::max(1.0f, advance)),
            highlight);
        }
        paint.color = segment.color;
        if (mPainters[lineIndex][0] == nullptr) {
          mPainters[lineIndex][0] = std::make_unique<TextPainter>();
        }
        TextPainter* painter = mPainters[lineIndex][0].get();
        if (upright && !tab) {
          painter->draw(context, shown, columnX, y + metrics.ascent, *segment.font, paint);
        } else {
          context.canvas().save();
          context.canvas().translate(columnX + metrics.ascent, y);
          context.canvas().rotate(90.0f);
          painter->draw(context, grapheme, 0.0f, 0.0f, *segment.font, paint);
          context.canvas().restore();
        }
        y += advance;
      };
      const auto drawSegment = [&](const RichTextSegment& segment, const bool reverse) {
        const std::vector<size_t> boundaries = unicode::computeGraphemeBreaks(segment.text);
        if (reverse) {
          for (size_t position = boundaries.size(); position-- > 0;) {
            const size_t previous = position == 0 ? 0 : boundaries[position - 1];
            drawGrapheme(segment, std::string_view(segment.text).substr(previous, boundaries[position] - previous),
              segment.start + previous, segment.start + boundaries[position]);
          }
        } else {
          size_t previous = 0;
          for (const size_t boundary: boundaries) {
            drawGrapheme(segment, std::string_view(segment.text).substr(previous, boundary - previous),
              segment.start + previous, segment.start + boundary);
            previous = boundary;
          }
        }
      };
      if (line.visualRuns.empty()) {
        for (const RichTextSegment& segment: line.segments) drawSegment(segment, false);
      } else {
        for (const uint32_t runIndex: line.visualOrder) {
          const TextVisualRun& run = line.visualRuns[runIndex];
          const auto found = std::upper_bound(line.segments.begin(), line.segments.end(), run.start,
            [](const size_t value, const RichTextSegment& segment) { return value < segment.start; });
          if (found == line.segments.begin()) continue;
          const RichTextSegment& segment = *(found - 1);
          if (segment.font == nullptr || run.start < segment.start || run.end > segment.end) continue;
          RichTextSegment slice = segment;
          slice.text = segment.text.substr(run.start - segment.start, run.end - run.start);
          slice.start = run.start;
          slice.end = run.end;
          drawSegment(slice, run.level % 2 != 0);
        }
      }
    }
    return;
  }
  float top = bounds().getTop();
  const std::string hyphenText = "\xE2\x80\x90";
  const auto drawHyphen = [&](const RichTextLine& line, const float x, const float top) {
    if (!line.hyphenated || line.segments.empty() || line.segments.back().font == nullptr) return;
    const RichTextSegment& segment = line.segments.back();
    if (mHyphenPainter == nullptr) mHyphenPainter = std::make_unique<TextPainter>();
    graphics::Paint paint;
    paint.color = segment.color;
    paint.style = graphics::Paint::Style::kFill;
    mHyphenPainter->draw(context, hyphenText, x, top + segment.font->metrics().ascent,
      *segment.font, paint);
  };
  for (size_t lineIndex = 0; lineIndex < mLayout.lines.size(); ++lineIndex) {
    const RichTextLine& line = mLayout.lines[lineIndex];
    if (mPainters[lineIndex].size() < line.segments.size()) {
      mPainters[lineIndex].resize(line.segments.size());
    }
    if (!line.visualRuns.empty()) {
      const std::vector<uint32_t>& order = line.visualOrder;
      float x = bounds().getLeft() + line.xOffset;
      for (const size_t runIndex: order) {
        const TextVisualRun& run = line.visualRuns[runIndex];
        const auto found = std::upper_bound(line.segments.begin(), line.segments.end(), run.start,
                [](const size_t value, const RichTextSegment& segment) { return value < segment.start; });
        if (found == line.segments.begin()) continue;
        const RichTextSegment& segment = *(found - 1);
        if (segment.font == nullptr || run.start < segment.start || run.end > segment.end) continue;
        const size_t segmentIndex = static_cast<size_t>(&segment - line.segments.data());
        const std::string runText = segment.text.substr(run.start - segment.start, run.end - run.start);
        const bool rightToLeft = run.level % 2 != 0;
        const std::string paintedText = rightToLeft ? mirrorBidiText(runText) : runText;
        const graphics::Font::Direction direction =
                rightToLeft ? graphics::Font::Direction::RightToLeft : graphics::Font::Direction::LeftToRight;
        const graphics::FontMetrics& metrics = segment.font->metrics();
        const float leading = std::max(0.0f,
          line.height - (metrics.ascent + metrics.descent + metrics.lineGap));
        const float baseline = top + leading * 0.5f + metrics.ascent;
        const float width = detail::TextShapeCache::instance().measure(*segment.font, paintedText, direction);
        if (hasSelection()) {
          const size_t begin = std::max(selectionStart(), run.start);
          const size_t end = std::min(selectionEnd(), run.end);
          if (begin < end) {
            const std::string prefix = runText.substr(0, begin - run.start);
            const std::string suffix = runText.substr(0, end - run.start);
            const std::string paintedPrefix = rightToLeft ? mirrorBidiText(prefix) : prefix;
            const std::string paintedSuffix = rightToLeft ? mirrorBidiText(suffix) : suffix;
            const float prefixWidth = detail::TextShapeCache::instance().measure(
              *segment.font, paintedPrefix, direction);
            const float suffixWidth = detail::TextShapeCache::instance().measure(
              *segment.font, paintedSuffix, direction);
            const float spanLeft = rightToLeft ? x + width - prefixWidth : x + prefixWidth;
            const float spanRight = rightToLeft ? x + width - suffixWidth : x + suffixWidth;
            graphics::Paint selection;
            selection.color = mSelectionColor;
            selection.style = graphics::Paint::Style::kFill;
            canvas.drawRect(Rect::MakeXYWH(std::min(spanLeft, spanRight), top,
                                      std::max(1.0f, std::abs(spanRight - spanLeft)),
                                      std::max(1.0f, line.height)),
                    selection);
          }
        }
        graphics::Paint paint;
        paint.color = segment.color;
        paint.style = graphics::Paint::Style::kFill;
        if (mPainters[lineIndex][segmentIndex] == nullptr) {
          mPainters[lineIndex][segmentIndex] = std::make_unique<TextPainter>();
        }
        mPainters[lineIndex][segmentIndex]->draw(context, paintedText, x, baseline, *segment.font, paint, direction);
        if (segment.underline) {
          graphics::Paint linePaint;
          linePaint.color = segment.color;
          linePaint.style = graphics::Paint::Style::kFill;
          const float offset = segment.font->metrics().underlineOffset;
          const float thickness = std::max(1.0f, segment.font->metrics().underlineThickness);
          canvas.drawRect(Rect::MakeXYWH(x, baseline + offset, width, thickness), linePaint);
        }
        x += width;
      }
      drawHyphen(line, x, top);
      top += line.height;
      continue;
    }
    float x = bounds().getLeft() + line.xOffset;
    float baseline = top;
    for (size_t segmentIndex = 0; segmentIndex < line.segments.size(); ++segmentIndex) {
      const RichTextSegment& segment = line.segments[segmentIndex];
      if (segment.font == nullptr) continue;
      const auto segmentSpaces = static_cast<float>(
        std::count(segment.text.begin(), segment.text.end(), ' '));
      const float spacing = line.justifySpacing;
      const graphics::FontMetrics& metrics = segment.font->metrics();
      const float leading = std::max(0.0f,
        line.height - (metrics.ascent + metrics.descent + metrics.lineGap));
      baseline = top + leading * 0.5f + metrics.ascent;
      if (hasSelection()) {
        const size_t begin = std::max(selectionStart(), segment.start);
        const size_t end = std::min(selectionEnd(), segment.end);
        if (begin < end) {
          const std::string prefix = segment.text.substr(0, begin - segment.start);
          const std::string value = segment.text.substr(0, end - segment.start);
          const auto prefixSpaces = static_cast<float>(
            std::count(prefix.begin(), prefix.end(), ' '));
          const auto valueSpaces = static_cast<float>(
            std::count(value.begin(), value.end(), ' '));
          const float left = x + segment.font->measureWidth(prefix) + prefixSpaces * spacing;
          const float right = x + segment.font->measureWidth(value) + valueSpaces * spacing;
          graphics::Paint selection;
          selection.color = mSelectionColor;
          selection.style = graphics::Paint::Style::kFill;
          canvas.drawRect(Rect::MakeXYWH(left, top, std::max(1.0f, right - left), std::max(1.0f, line.height)),
                  selection);
        }
      }
      graphics::Paint paint;
      paint.color = segment.color;
      paint.style = graphics::Paint::Style::kFill;
      if (mPainters[lineIndex][segmentIndex] == nullptr) {
        mPainters[lineIndex][segmentIndex] = std::make_unique<TextPainter>();
      }
      if (spacing > 0.0f && segmentSpaces > 0.0f) {
        size_t begin = 0;
        while (begin <= segment.text.size()) {
          const size_t space = segment.text.find(' ', begin);
          const std::string_view token = std::string_view(segment.text).substr(begin,
            space == std::string::npos ? std::string::npos : space - begin);
          if (!token.empty()) {
            const std::string_view prefix = std::string_view(segment.text).substr(0, begin);
            const auto prefixSpaces = static_cast<float>(
              std::count(prefix.begin(), prefix.end(), ' '));
            const float tokenX = x + segment.font->measureWidth(prefix) + prefixSpaces * spacing;
            const std::string display = expandTabs(token, mTabSize);
            mPainters[lineIndex][segmentIndex]->draw(context, display, tokenX, baseline,
              *segment.font, paint);
          }
          if (space == std::string::npos) break;
          begin = space + 1;
        }
      } else {
        const std::string display = expandTabs(segment.text, mTabSize);
        mPainters[lineIndex][segmentIndex]->draw(context, display, x, baseline,
          *segment.font, paint);
      }
      if (segment.underline) {
        graphics::Paint linePaint;
        linePaint.color = segment.color;
        linePaint.style = graphics::Paint::Style::kFill;
        const float offset = segment.font->metrics().underlineOffset;
        const float thickness = std::max(1.0f, segment.font->metrics().underlineThickness);
        canvas.drawRect(Rect::MakeXYWH(x, baseline + offset,
          segment.width + segmentSpaces * spacing, thickness), linePaint);
      }
      x += segment.width + segmentSpaces * spacing;
    }
    drawHyphen(line, x, top);
    top += line.height;
  }
}

} // namespace coral::ui
