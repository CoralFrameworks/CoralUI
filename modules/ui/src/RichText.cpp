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

#include <ui/RichText.h>
#include <ui/TextBidi.h>

#include "RichTextLayoutCache.h"
#include "ShapedAdvances.h"
#include "TextShapeCache.h"
#include "unicode/GraphemeBreak.h"
#include "unicode/LineBreakIterator.h"

#include <graphics/FontFallback.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_map>

namespace coral::ui {

namespace {

struct Glyph {
  std::string_view text;
  float width = 0.0f;
  std::shared_ptr<const graphics::Font> font;
  std::vector<graphics::FontFeature> features;
  graphics::Color color{};
  bool underline = false;
  bool space = false;
  size_t offset = 0;
};

bool sameFeatures(const std::vector<graphics::FontFeature>& left,
    const std::vector<graphics::FontFeature>& right) {
  if (left.size() != right.size()) return false;
  for (size_t index = 0; index < left.size(); ++index) {
    if (left[index].tag != right[index].tag || left[index].value != right[index].value ||
        left[index].start != right[index].start || left[index].end != right[index].end) {
      return false;
    }
  }
  return true;
}

size_t sequenceLength(const std::string& text, const size_t offset) {
  const auto lead = static_cast<unsigned char>(text[offset]);
  if ((lead & 0x80u) == 0) return 1;
  if ((lead & 0xE0u) == 0xC0u) return 2;
  if ((lead & 0xF0u) == 0xE0u) return 3;
  if ((lead & 0xF8u) == 0xF0u) return 4;
  return 1;
}

uint32_t codePointAt(const std::string& text, const size_t offset, const size_t length) {
  const auto lead = static_cast<unsigned char>(text[offset]);
  if (length == 1) return lead < 0x80u ? lead : 0xFFFDu;
  if (length == 2) {
    return (static_cast<uint32_t>(lead & 0x1Fu) << 6) |
           static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 1]) & 0x3Fu);
  }
  if (length == 3) {
    return (static_cast<uint32_t>(lead & 0x0Fu) << 12) |
           (static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 1]) & 0x3Fu) << 6) |
           static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 2]) & 0x3Fu);
  }
  return (static_cast<uint32_t>(lead & 0x07u) << 18) |
         (static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 1]) & 0x3Fu) << 12) |
         (static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 2]) & 0x3Fu) << 6) |
         static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 3]) & 0x3Fu);
}

float alignmentOffset(const TextAlign align, const float available, const float width) {
  if (available <= 0.0f) return 0.0f;
  const float slack = std::max(0.0f, available - width);
  switch (align) {
    case TextAlign::Center:
      return slack * 0.5f;
    case TextAlign::Right:
      return slack;
    case TextAlign::Justify:
    case TextAlign::Left:
      break;
  }
  return 0.0f;
}

float lineHeight(const RichTextLine& line) {
  float height = 0.0f;
  for (const RichTextSegment& segment: line.segments) {
    if (segment.font == nullptr) continue;
    const graphics::FontMetrics& metrics = segment.font->metrics();
    height = std::max(height, metrics.ascent + metrics.descent + metrics.lineGap);
  }
  return height;
}

RichTextLine makeLine(const std::vector<Glyph>& glyphs, const size_t begin, const size_t end,
    const bool hyphenated = false, const float hyphenWidth = 0.0f) {
  RichTextLine line;
  if (begin < end) {
    line.start = glyphs[begin].offset;
    line.end = glyphs[begin].offset + glyphs[begin].text.size();
  }
  const char* pendingData = nullptr;
  size_t pendingLength = 0;
  const auto flushPending = [&]() {
    if (pendingLength != 0 && !line.segments.empty()) {
      // One append per contiguous style run instead of one per glyph.
      line.segments.back().text.append(pendingData, pendingLength);
    }
    pendingData = nullptr;
    pendingLength = 0;
  };
  const auto startPending = [&](const Glyph& glyph) {
    pendingData = glyph.text.data();
    pendingLength = glyph.text.size();
  };
  for (size_t index = begin; index < end; ++index) {
    const Glyph& glyph = glyphs[index];
    if (glyph.space && index + 1 == end) {
      break; // trailing spaces do not affect a line's measured width
    }
    if (!line.segments.empty() && line.segments.back().font == glyph.font &&
            sameFeatures(line.segments.back().features, glyph.features) &&
            line.segments.back().color.red == glyph.color.red &&
            line.segments.back().color.green == glyph.color.green &&
            line.segments.back().color.blue == glyph.color.blue &&
            line.segments.back().color.alpha == glyph.color.alpha &&
            line.segments.back().underline == glyph.underline) {
      if (pendingData != nullptr && glyph.text.data() == pendingData + pendingLength) {
        pendingLength += glyph.text.size();
      } else {
        flushPending();
        startPending(glyph);
      }
      line.segments.back().width += glyph.width;
      line.segments.back().end = glyph.offset + glyph.text.size();
    } else {
      flushPending();
      RichTextSegment segment;
      segment.font = glyph.font;
      segment.features = glyph.features;
      segment.color = glyph.color;
      segment.underline = glyph.underline;
      segment.start = glyph.offset;
      segment.end = glyph.offset + glyph.text.size();
      segment.width = glyph.width;
      line.segments.push_back(std::move(segment));
      startPending(glyph);
    }
    line.width += glyph.width;
    line.end = glyph.offset + glyph.text.size();
  }
  flushPending();
  if (hyphenated) {
    line.hyphenated = true;
    line.width += hyphenWidth;
  }
  line.height = lineHeight(line);
  return line;
}

float hyphenWidthFor(const Glyph& glyph) {
  if (glyph.font == nullptr) return 0.0f;
  return detail::TextShapeCache::instance().measure(*glyph.font, "\xE2\x80\x90");
}

uint32_t firstCodePoint(const std::string_view text) {
  if (text.empty()) return 0;
  const auto lead = static_cast<unsigned char>(text[0]);
  if (lead < 0x80u) return lead;
  if ((lead & 0xE0u) == 0xC0u && text.size() >= 2) {
    return ((lead & 0x1Fu) << 6) | (static_cast<unsigned char>(text[1]) & 0x3Fu);
  }
  if ((lead & 0xF0u) == 0xE0u && text.size() >= 3) {
    return ((lead & 0x0Fu) << 12) | ((static_cast<unsigned char>(text[1]) & 0x3Fu) << 6) |
           (static_cast<unsigned char>(text[2]) & 0x3Fu);
  }
  if ((lead & 0xF8u) == 0xF0u && text.size() >= 4) {
    return ((lead & 0x07u) << 18) | ((static_cast<unsigned char>(text[1]) & 0x3Fu) << 12) |
           ((static_cast<unsigned char>(text[2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(text[3]) & 0x3Fu);
  }
  return 0;
}


} // namespace

namespace {

std::optional<TextDirection> bidiDirection(const TextDirectionMode mode) {
  if (mode == TextDirectionMode::Ltr) return TextDirection::Ltr;
  if (mode == TextDirectionMode::Rtl) return TextDirection::Rtl;
  return std::nullopt;
}

void attachVisualRuns(const std::string& text, std::vector<RichTextLine>& lines,
    const TextDirectionMode mode, const std::span<const BidiSpan> bidiSpans) {
  const std::optional<TextDirection> direction = bidiDirection(mode);
  if (text.empty() || (mode == TextDirectionMode::Auto && bidiSpans.empty() && !requiresBidi(text))) {
    return;
  }
  BidiParagraph paragraph = bidiSpans.empty()
    ? resolveBidiParagraph(text, direction)
    : resolveBidiParagraphWithSpans(text, direction, bidiSpans);
  if (paragraph.byteOffsets.size() != paragraph.levels.size()) return;

  for (RichTextLine& line: lines) {
    line.rtl = paragraph.baseLevel % 2 != 0;
    if (line.ellipsis) {
      // The synthetic ellipsis line no longer corresponds to plainText after
      // the grapheme cut; give it one visual run so hit-testing and RTL
      // drawing use the same path as natural lines.
      line.visualRuns.clear();
      line.visualOrder.clear();
      if (line.end > line.start) {
        line.visualRuns.push_back({ line.start, line.end,
          static_cast<uint8_t>(line.rtl ? 1 : 0) });
        line.visualOrder.push_back(0);
      }
      continue;
    }
    std::vector<TextVisualRun> runs;
    bool needsReordering = false;
    for (const RichTextSegment& segment: line.segments) {
      const auto first = std::lower_bound(paragraph.byteOffsets.begin(), paragraph.byteOffsets.end(), segment.start);
      if (first == paragraph.byteOffsets.end()) continue;
      size_t index = static_cast<size_t>(first - paragraph.byteOffsets.begin());
      size_t runStart = segment.start;
      uint8_t runLevel = 0;
      bool hasRun = false;
      for (; index < paragraph.byteOffsets.size() && paragraph.byteOffsets[index] < segment.end; ++index) {
        const uint8_t level = paragraph.levels[index];
        if (level == 0xFF) continue;
        if (level != 0) needsReordering = true;
        if (!hasRun) {
          runStart = paragraph.byteOffsets[index];
          runLevel = level;
          hasRun = true;
        } else if (level != runLevel) {
          runs.push_back({ runStart, paragraph.byteOffsets[index], runLevel });
          runStart = paragraph.byteOffsets[index];
          runLevel = level;
        }
      }
      if (hasRun) runs.push_back({ runStart, segment.end, runLevel });
    }
    if (needsReordering && !runs.empty()) {
      line.visualRuns = std::move(runs);
      const std::vector<size_t> order = visualRunOrder(line.visualRuns);
      line.visualOrder.assign(order.begin(), order.end());
    }
  }
}

RichTextLayout layoutRichTextUncached(const std::vector<RichTextRun>& runs,
    const RichTextLayoutOptions& options) {
  const float maxWidth = options.maxWidth;
  const LineBreakPolicy& policy = options.policy;
  const uint32_t tabSize = options.tabSize;
  RichTextLayout layout;
  layout.align = options.align;
  layout.lineHeight = options.lineHeight;
  std::vector<RichTextLine> builtLines;
  std::vector<Glyph> glyphs;
  std::string plainText;
  size_t textOffset = 0;
  size_t totalBytes = 0;
  for (const RichTextRun& run: runs) {
    if (run.font != nullptr) totalBytes += run.text.size();
  }
  glyphs.reserve(totalBytes);
  plainText.reserve(totalBytes);
  // Measure each (face, codepoint) once. ASCII gets a direct table; other
  // codepoints go through a small hash map. Without this a 1 MiB paragraph pays
  // one TextShapeCache lookup (and hash of a temporary string) per glyph.
  std::array<const graphics::Font*, 128> asciiFace{};
  std::array<float, 128> asciiWidth{};
  std::array<bool, 128> asciiKnown{};
  std::unordered_map<uint64_t, float> widthCache;
  widthCache.reserve(1024);
  const auto glyphWidth = [&](const uint32_t codePoint, const std::string_view text,
      const graphics::Font* face) {
    if (codePoint < 128) {
      if (!asciiKnown[codePoint] || asciiFace[codePoint] != face) {
        asciiFace[codePoint] = face;
        asciiWidth[codePoint] = detail::TextShapeCache::instance().measure(*face, text);
        asciiKnown[codePoint] = true;
      }
      return asciiWidth[codePoint];
    }
    const uint64_t key = (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(face)) << 21) |
                         codePoint;
    const auto found = widthCache.find(key);
    if (found != widthCache.end()) return found->second;
    const float width = detail::TextShapeCache::instance().measure(*face, text);
    widthCache.emplace(key, width);
    return width;
  };
  for (const RichTextRun& run: runs) {
    if (run.font == nullptr) continue;
    plainText += run.text;
    // One shaping pass per run, so the break decision and the drawn width agree. The per-glyph
    // fallback below (TextShapeCache) stays for tabs and for a run whose shaping has no metrics.
    const detail::ShapedAdvances advances = run.fallback != nullptr
      ? detail::ShapedAdvances::fromFallback(*run.fallback, run.text)
      : detail::ShapedAdvances::fromFont(*run.font, run.text,
          graphics::Font::Direction::Auto, run.features);
    size_t offset = 0;
    while (offset < run.text.size()) {
      if (run.text[offset] == '\n') {
        Glyph newline;
        newline.text = "\n";
        newline.font = run.font;
        newline.features = run.features;
        newline.color = run.color;
        newline.offset = textOffset;
        glyphs.push_back(std::move(newline));
        ++offset;
        ++textOffset;
        continue;
      }
      const size_t length = std::max<size_t>(1, sequenceLength(run.text, offset));
      std::shared_ptr<const graphics::Font> face = run.font;
      if (run.fallback != nullptr) {
        const auto& chosen = run.fallback->fontFor(codePointAt(run.text, offset, length));
        if (chosen != nullptr) face = chosen;
      }
      Glyph glyph;
      glyph.text = std::string_view(run.text).substr(offset, length);
      glyph.width = advances.valid()
                      ? advances.advance(offset, offset + length)
                      : glyphWidth(codePointAt(run.text, offset, length), glyph.text, face.get());
      if (glyph.text == "\t" && tabSize > 0) {
        glyph.width = detail::TextShapeCache::instance().measure(*face, " ") *
                      static_cast<float>(tabSize);
      }
      glyph.font = face;
      glyph.features = run.features;
      glyph.color = run.color;
      glyph.underline = run.underline;
      glyph.space = glyph.text == " ";
      glyph.offset = textOffset;
      glyphs.push_back(std::move(glyph));
      offset += length;
      textOffset += length;
    }
  }

  const bool wrap = maxWidth > 0.0f;
  struct BreakPoint {
    size_t offset = 0;
    uint8_t action = 0;
    bool hyphen = false;
  };
  std::vector<BreakPoint> breaks;
  const auto isSoftHyphen = [&](const size_t offset) {
    return offset >= 2 && offset <= plainText.size() &&
           static_cast<unsigned char>(plainText[offset - 2]) == 0xC2u &&
           static_cast<unsigned char>(plainText[offset - 1]) == 0xADu;
  };
  const auto pushBreak = [&](const size_t offset, const uint8_t action, const bool hyphenate) {
    if (offset < plainText.size()) {
      breaks.push_back({ offset, action, hyphenate || isSoftHyphen(offset) });
    }
  };
  if (policy.isDefault()) {
    unicode::forEachLineBreakOpportunity(plainText,
      [&](const size_t offset, const unicode::BreakAction action) {
        pushBreak(offset, static_cast<uint8_t>(action), false);
      });
  } else {
    for (const TailoredLineBreak& opportunity: computeTailoredLineBreaks(plainText, policy)) {
      pushBreak(opportunity.byteOffset, opportunity.mandatory ? 2 : 1, opportunity.hyphenate);
    }
  }
  std::sort(breaks.begin(), breaks.end(), [](const BreakPoint& left, const BreakPoint& right) {
    if (left.offset != right.offset) return left.offset < right.offset;
    if (left.action != right.action) return left.action > right.action;
    return left.hyphen && !right.hyphen;
  });
  breaks.erase(std::unique(breaks.begin(), breaks.end(),
                 [](const BreakPoint& left, const BreakPoint& right) {
                   return left.offset == right.offset;
                 }),
    breaks.end());
  const auto findBreak = [&](const size_t offset) -> const BreakPoint* {
    const auto found = std::lower_bound(breaks.begin(), breaks.end(), offset,
      [](const BreakPoint& point, const size_t value) { return point.offset < value; });
    return found != breaks.end() && found->offset == offset ? &*found : nullptr;
  };
  std::vector<Glyph> lineGlyphs;
  float lineWidth = 0.0f;
  size_t consecutiveHyphenated = 0;
  const auto pushLine = [&](RichTextLine line) {
    consecutiveHyphenated = line.hyphenated ? consecutiveHyphenated + 1 : 0;
    builtLines.push_back(std::move(line));
  };
  const auto hyphenAllowed = [&](const float prefixWidth, const float hyphenWidth) {
    const size_t lineLimit = policy.hyphenateLimitLines;
    if (lineLimit != 0 && consecutiveHyphenated >= lineLimit) return false;
    const float zone = policy.hyphenateLimitZone;
    if (zone > 0.0f && wrap) {
      const float slack = maxWidth - prefixWidth - hyphenWidth;
      if (slack > zone) return false;
    }
    return true;
  };
  const auto flush = [&](const bool hardBreak) {
    if (lineGlyphs.empty()) {
      if (hardBreak) {
        RichTextLine empty;
        empty.height = 0.0f;
        empty.start = textOffset;
        empty.end = textOffset;
        pushLine(std::move(empty));
      }
      return;
    }
    pushLine(makeLine(lineGlyphs, 0, lineGlyphs.size()));
    lineGlyphs.clear();
    lineWidth = 0.0f;
  };

  size_t index = 0;
  unicode::GraphemeBreakState graphemeState;
  const bool allowEmergency = policy.overflowWrap != OverflowWrap::Normal;
  while (index < glyphs.size()) {
    const bool graphemeBoundary = graphemeState.isBoundaryBefore(firstCodePoint(glyphs[index].text));
    if (glyphs[index].text == "\n") {
      flush(true);
      ++index;
      continue;
    }
    const size_t currentOffset = glyphs[index].offset;
    const BreakPoint* const direct = findBreak(currentOffset);
    if (direct != nullptr && direct->action == 2) {
      // Mandatory break before this character; the separator itself is not drawn.
      flush(false);
      ++index;
      continue;
    }
    const bool currentHyphen = direct != nullptr && direct->hyphen;
    const float reserve = currentHyphen && !lineGlyphs.empty()
      ? hyphenWidthFor(lineGlyphs.back())
      : 0.0f;
    if (wrap && !lineGlyphs.empty() &&
        lineWidth + glyphs[index].width + reserve > maxWidth) {
      bool directBreak = direct != nullptr && direct->action == 1;
      const float directHyphenWidth = direct != nullptr && direct->hyphen
        ? hyphenWidthFor(lineGlyphs.back())
        : 0.0f;
      if (directBreak && direct->hyphen && !hyphenAllowed(lineWidth, directHyphenWidth)) {
        directBreak = false;
      }
      if (directBreak) {
        // The opportunity sits directly before the overflowing glyph: the whole
        // buffered line is complete. This is the common CJK/space case and must
        // not depend on overflow-wrap: normal allowing emergency breaks.
        pushLine(makeLine(lineGlyphs, 0, lineGlyphs.size(), direct->hyphen, directHyphenWidth));
        lineGlyphs.clear();
        lineWidth = 0.0f;
      }
      if (!directBreak) {
        size_t breakAt = lineGlyphs.size();
        for (size_t back = lineGlyphs.size(); back > 0; --back) {
          const size_t offset = lineGlyphs[back - 1].offset;
          const BreakPoint* const found = findBreak(offset);
          if (found == nullptr || found->action == 0) continue;
          if (found->hyphen && back > 1) {
            // A back-scanned hyphen must fit together with the line prefix.
            float prefix = 0.0f;
            for (size_t indexBefore = 0; indexBefore + 1 < back; ++indexBefore) {
              prefix += lineGlyphs[indexBefore].width;
            }
            const float hyphenWidth = hyphenWidthFor(lineGlyphs[back - 2]);
            if (prefix + hyphenWidth > maxWidth) continue;
            if (!hyphenAllowed(prefix, hyphenWidth)) continue;
          }
          breakAt = back - 1;
          break;
        }
        if (breakAt == 0 || breakAt == lineGlyphs.size()) {
          // No legal opportunity: use the grapheme boundary.
          if (graphemeBoundary && allowEmergency) flush(false);
        } else {
          const BreakPoint* const found = findBreak(lineGlyphs[breakAt].offset);
          const bool hyphen = found != nullptr && found->hyphen;
          pushLine(makeLine(lineGlyphs, 0, breakAt, hyphen,
            hyphen ? hyphenWidthFor(lineGlyphs[breakAt - 1]) : 0.0f));
          std::vector<Glyph> remainder(lineGlyphs.begin() + static_cast<std::ptrdiff_t>(breakAt), lineGlyphs.end());
          lineGlyphs = std::move(remainder);
          lineWidth = 0.0f;
          for (const Glyph& glyph: lineGlyphs) lineWidth += glyph.width;
        }
      }
    }
    lineGlyphs.push_back(glyphs[index]);
    lineWidth += glyphs[index].width;
    ++index;
  }
  flush(false);

  if (options.maxLines > 0 && static_cast<int>(builtLines.size()) > options.maxLines) {
    builtLines.resize(static_cast<size_t>(options.maxLines));
    layout.truncated = true;
    if (options.ellipsis && !builtLines.empty()) {
      RichTextLine& last = builtLines.back();
      const std::string ellipsis = "\xE2\x80\xA6";
      bool applied = false;
      for (size_t segmentIndex = last.segments.size(); segmentIndex-- > 0 && !applied;) {
        RichTextSegment& segment = last.segments[segmentIndex];
        if (segment.font == nullptr) {
          last.segments.resize(segmentIndex);
          continue;
        }
        const std::vector<size_t> boundaries = unicode::computeGraphemeBreaks(segment.text);
        for (size_t boundaryIndex = boundaries.size(); boundaryIndex-- > 0;) {
          const size_t cut = boundaries[boundaryIndex];
          const std::string candidate = segment.text.substr(0, cut) + ellipsis;
          const float candidateWidth =
                  detail::TextShapeCache::instance().measure(*segment.font, candidate);
          float total = candidateWidth;
          for (size_t prior = 0; prior < segmentIndex; ++prior) total += last.segments[prior].width;
          if (options.maxWidth > 0.0f && total > options.maxWidth) continue;
          last.segments.resize(segmentIndex + 1);
          segment.text = candidate;
          segment.width = candidateWidth;
          segment.end = segment.start + cut + ellipsis.size();
          last.width = total;
          last.end = segment.end;
          applied = true;
          break;
        }
        if (!applied) last.segments.resize(segmentIndex);
      }
      if (!applied) {
        // Nothing of the line fits beside the ellipsis; keep just the ellipsis.
        std::shared_ptr<const graphics::Font> font;
        graphics::Color color = graphics::Color::White();
        for (const RichTextRun& run: runs) {
          if (run.font != nullptr) {
            font = run.font;
            color = run.color;
            break;
          }
        }
        if (font != nullptr) {
          RichTextSegment segment;
          segment.text = ellipsis;
          segment.font = font;
          segment.color = color;
          segment.start = last.start;
          segment.end = last.start + ellipsis.size();
          segment.width = detail::TextShapeCache::instance().measure(*font, ellipsis);
          last.segments.clear();
          last.segments.push_back(std::move(segment));
          last.width = last.segments.front().width;
          last.end = last.segments.front().end;
        }
      }
      builtLines.back().ellipsis = true;
    }
  }

  float width = 0.0f;
  float height = 0.0f;
  for (RichTextLine& line: builtLines) {
    if (options.lineHeight > 0.0f) line.height = options.lineHeight;
    line.xOffset = alignmentOffset(options.align, options.maxWidth, line.width);
    width = std::max(width, line.width);
    height += line.height;
  }
  attachVisualRuns(plainText, builtLines, options.direction, options.bidiSpans);
  if (options.align == TextAlign::Justify && options.maxWidth > 0.0f) {
    for (size_t lineIndex = 0; lineIndex < builtLines.size(); ++lineIndex) {
      RichTextLine& line = builtLines[lineIndex];
      line.justifySpacing = 0.0f;
      if (lineIndex + 1 >= builtLines.size()) continue;
      if (!line.visualRuns.empty()) continue;
      if (line.end < plainText.size() &&
          (plainText[line.end] == '\n' || plainText[line.end] == '\r')) {
        continue;
      }
      size_t spaces = 0;
      for (const RichTextSegment& segment: line.segments) {
        spaces += static_cast<size_t>(std::count(segment.text.begin(), segment.text.end(), ' '));
      }
      if (spaces == 0) continue;
      const float slack = options.maxWidth - line.width;
      if (slack > 0.0f) line.justifySpacing = slack / static_cast<float>(spaces);
    }
  }
  layout.orientation = options.orientation;
  layout.direction = options.direction;
  if (options.orientation != TextOrientation::Horizontal) {
    float columnWidth = options.lineHeight;
    if (columnWidth <= 0.0f) {
      for (const RichTextLine& line: builtLines) columnWidth = std::max(columnWidth, line.height);
    }
    layout.size = { columnWidth * static_cast<float>(builtLines.size()), width };
    layout.lineHeight = columnWidth;
    for (RichTextLine& line: builtLines) {
      line.xOffset = 0.0f;
    }
  } else {
    layout.size = { width, height };
  }
  for (RichTextLine& line: builtLines) {
    line.segments.seal();
  }
  layout.lines = RichTextLineList(
    std::make_shared<const RichTextLineList::Container>(std::move(builtLines)));
  layout.maxWidth = options.maxWidth;
  layout.text = std::make_shared<const std::string>(std::move(plainText));
  return layout;
}

} // namespace anonymous

RichTextLayout layoutRichText(const std::vector<RichTextRun>& runs, const float maxWidth) {
  return layoutRichText(runs, RichTextLayoutOptions{ maxWidth });
}

RichTextLayout layoutRichText(const std::vector<RichTextRun>& runs, const float maxWidth,
    const LineBreakPolicy& policy) {
  RichTextLayoutOptions options;
  options.maxWidth = maxWidth;
  options.policy = policy;
  return layoutRichText(runs, options);
}

RichTextLayout layoutRichText(const std::vector<RichTextRun>& runs,
    const RichTextLayoutOptions& options) {
  if (!options.bidiSpans.empty()) return layoutRichTextUncached(runs, options);
  if (const std::optional<RichTextLayout> cached =
              detail::RichTextLayoutCache::instance().find(runs, options)) {
    return *cached;
  }
  RichTextLayout layout = layoutRichTextUncached(runs, options);
  detail::RichTextLayoutCache::instance().store(runs, options, layout);
  return layout;
}

RichTextLayout updateRichTextLayout(const RichTextLayout& previous,
    const std::vector<RichTextRun>& runs, const RichTextLayoutOptions& options,
    const TextEditRange* edit) {
  (void) edit;
  const auto& oldLines = previous.lines;
  if (previous.text == nullptr || oldLines.empty() || options.maxLines != 0 ||
      options.ellipsis || options.align == TextAlign::Justify ||
      options.orientation != TextOrientation::Horizontal ||
      options.direction != TextDirectionMode::Auto ||
      !options.bidiSpans.empty() ||
      previous.direction != options.direction ||
      previous.align != options.align || previous.maxWidth != options.maxWidth ||
      previous.lineHeight != options.lineHeight) {
    return layoutRichText(runs, options);
  }

  std::string newText;
  size_t totalBytes = 0;
  for (const RichTextRun& run: runs) {
    if (run.font != nullptr) totalBytes += run.text.size();
  }
  newText.reserve(totalBytes);
  for (const RichTextRun& run: runs) {
    if (run.font != nullptr) newText += run.text;
  }
  const std::string& oldText = *previous.text;
  if (newText == oldText) return previous;

  const size_t common = std::min(oldText.size(), newText.size());
  size_t prefix = 0;
  while (prefix < common && oldText[prefix] == newText[prefix]) ++prefix;
  size_t suffix = 0;
  while (suffix < common - prefix &&
         oldText[oldText.size() - 1 - suffix] == newText[newText.size() - 1 - suffix]) {
    ++suffix;
  }

  // Clamp to UTF-8 code point boundaries; full grapheme segmentation of a 1 MiB
  // paragraph would dominate the incremental path.
  while (prefix > 0 && prefix < newText.size() &&
         (static_cast<unsigned char>(newText[prefix]) & 0xC0u) == 0x80u) {
    --prefix;
  }
  size_t suffixStart = newText.size() - suffix;
  while (suffixStart < newText.size() &&
         (static_cast<unsigned char>(newText[suffixStart]) & 0xC0u) == 0x80u) {
    ++suffixStart;
  }
  suffix = newText.size() - suffixStart;
  if (prefix > suffixStart) {
    suffixStart = prefix;
    suffix = newText.size() - suffixStart;
  }

  size_t firstMiddle = 0;
  while (firstMiddle < oldLines.size() && oldLines[firstMiddle].end <= prefix) ++firstMiddle;
  size_t lastMiddle = oldLines.size();
  const size_t oldSuffixStart = oldText.size() - suffix;
  while (lastMiddle > firstMiddle && oldLines[lastMiddle - 1].start >= oldSuffixStart) {
    --lastMiddle;
  }
  if (firstMiddle == 0 && lastMiddle == oldLines.size()) {
    return layoutRichText(runs, options);
  }

  const size_t delta = newText.size() - oldText.size();
  const size_t middleBegin = firstMiddle < oldLines.size()
    ? oldLines[firstMiddle].start
    : oldText.size();
  const size_t middleEndOld = lastMiddle < oldLines.size()
    ? oldLines[lastMiddle].start
    : oldText.size();
  const std::ptrdiff_t middleEndCandidate =
    static_cast<std::ptrdiff_t>(middleEndOld) + static_cast<std::ptrdiff_t>(delta);
  const size_t middleEndNew = middleEndCandidate < 0
    ? 0
    : std::min(static_cast<size_t>(middleEndCandidate), newText.size());
  if (middleBegin > newText.size() || middleBegin > middleEndNew) {
    return layoutRichText(runs, options);
  }

  std::vector<RichTextRun> middleRuns;
  size_t runOffset = 0;
  for (const RichTextRun& run: runs) {
    const size_t runBegin = runOffset;
    const size_t runEnd = runOffset + run.text.size();
    runOffset = runEnd;
    if (run.font == nullptr) continue;
    const size_t begin = std::max(runBegin, middleBegin);
    const size_t end = std::min(runEnd, middleEndNew);
    if (begin >= end) continue;
    RichTextRun sliced = run;
    sliced.text = run.text.substr(begin - runBegin, end - begin);
    middleRuns.push_back(std::move(sliced));
  }

  RichTextLayout middle;
  if (middleBegin < middleEndNew) {
    middle = layoutRichText(middleRuns, options);
  }

  // Pure tail append: every old line is reusable and no suffix needs an offset
  // shift, so the result can share the previous line block instead of copying it.
  if (firstMiddle == oldLines.size() && lastMiddle == oldLines.size()) {
    RichTextLayout result;
    result.align = options.align;
    result.lineHeight = options.lineHeight;
    result.maxWidth = options.maxWidth;
    result.direction = options.direction;
    result.lines.appendRange(previous.lines);
    if (middle.lines.size() != 0) {
      auto appended = std::make_shared<RichTextLineList::Container>();
      appended->reserve(middle.lines.size());
      for (RichTextLine line: middle.lines) {
        line.start += middleBegin;
        line.end += middleBegin;
        for (RichTextSegment& segment: line.segments) {
          segment.start += middleBegin;
          segment.end += middleBegin;
        }
        appended->push_back(std::move(line));
      }
      result.lines.appendContainer(std::move(appended));
    }
    result.size = { std::max(previous.size.width, middle.size.width),
      previous.size.height + middle.size.height };
    result.text = std::make_shared<const std::string>(std::move(newText));
    detail::RichTextLayoutCache::instance().store(runs, options, result);
    return result;
  }

  RichTextLineList combined;
  combined.appendSubrange(previous.lines, 0, firstMiddle);
  if (!middle.lines.empty()) {
    auto middleContainer = std::make_shared<RichTextLineList::Container>();
    middleContainer->reserve(middle.lines.size());
    for (RichTextLine line: middle.lines) {
      line.start += middleBegin;
      line.end += middleBegin;
      for (RichTextSegment& segment: line.segments) {
        segment.start += middleBegin;
        segment.end += middleBegin;
      }
      for (TextVisualRun& run: line.visualRuns) {
        run.start += middleBegin;
        run.end += middleBegin;
      }
      middleContainer->push_back(std::move(line));
    }
    combined.appendContainer(std::move(middleContainer));
  }
  if (delta == 0) {
    // Same-length replacement: the suffix offsets are unchanged, so share it.
    combined.appendSubrange(previous.lines, lastMiddle, oldLines.size());
  } else {
    auto suffixContainer = std::make_shared<RichTextLineList::Container>();
    suffixContainer->reserve(oldLines.size() - lastMiddle);
    for (size_t index = lastMiddle; index < oldLines.size(); ++index) {
      RichTextLine line = oldLines[index];
      line.start = static_cast<size_t>(
        static_cast<std::ptrdiff_t>(line.start) + static_cast<std::ptrdiff_t>(delta));
      line.end = static_cast<size_t>(
        static_cast<std::ptrdiff_t>(line.end) + static_cast<std::ptrdiff_t>(delta));
      for (RichTextSegment& segment: line.segments) {
        segment.start = static_cast<size_t>(
          static_cast<std::ptrdiff_t>(segment.start) + static_cast<std::ptrdiff_t>(delta));
        segment.end = static_cast<size_t>(
          static_cast<std::ptrdiff_t>(segment.end) + static_cast<std::ptrdiff_t>(delta));
      }
      for (TextVisualRun& run: line.visualRuns) {
        run.start = static_cast<size_t>(
          static_cast<std::ptrdiff_t>(run.start) + static_cast<std::ptrdiff_t>(delta));
        run.end = static_cast<size_t>(
          static_cast<std::ptrdiff_t>(run.end) + static_cast<std::ptrdiff_t>(delta));
      }
      suffixContainer->push_back(std::move(line));
    }
    combined.appendContainer(std::move(suffixContainer));
  }

  RichTextLayout result;
  result.align = options.align;
  result.lineHeight = options.lineHeight;
  result.maxWidth = options.maxWidth;
  result.direction = options.direction;
  float width = 0.0f;
  float height = 0.0f;
  for (const RichTextLine& line: combined) {
    width = std::max(width, line.width);
    height += line.height;
  }
  result.size = { width, height };
  result.lines = std::move(combined);
  result.text = std::make_shared<const std::string>(std::move(newText));
  detail::RichTextLayoutCache::instance().store(runs, options, result);
  return result;
}

} // namespace coral::ui
