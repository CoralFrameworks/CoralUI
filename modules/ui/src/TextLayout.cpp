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

#include <ui/TextBidi.h>
#include <ui/TextLayout.h>

#include "TextLayoutCache.h"
#include "ShapedAdvances.h"
#include "TextShapeCache.h"
#include "unicode/BidiProperties.generated.h"
#include "unicode/GraphemeBreak.h"
#include "unicode/LineBreakIterator.h"

#include <graphics/FontFallback.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <cstdio>
#include <iterator>
#include <cstdint>
#include <numeric>
#include <string_view>
#include <utility>
#include <unordered_map>

namespace coral::ui {

LazyTextBlob::LazyTextBlob(std::shared_ptr<const graphics::Font> font, std::string text,
    const graphics::Font::Direction direction, const bool mirror)
    : mFont(std::move(font)), mText(std::move(text)), mDirection(direction), mMirror(mirror) {}

std::shared_ptr<const LazyTextBlob> LazyTextBlob::create(std::shared_ptr<const graphics::Font> font,
    std::string text, const graphics::Font::Direction direction, const bool mirror) {
  if (font == nullptr) return nullptr;
  struct Key {
    const graphics::Typeface* typeface = nullptr;
    uint32_t sizeBits = 0;
    graphics::Font::Direction direction = graphics::Font::Direction::Auto;
    bool mirror = false;
    std::string text;

    bool operator==(const Key& other) const noexcept {
      return typeface == other.typeface && sizeBits == other.sizeBits &&
             direction == other.direction && mirror == other.mirror && text == other.text;
    }
  };
  struct Hash {
    size_t operator()(const Key& key) const noexcept {
      size_t hash = std::hash<const void*>{}(key.typeface);
      hash ^= std::hash<uint32_t>{}(key.sizeBits) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(key.direction)) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<bool>{}(key.mirror) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<std::string>{}(key.text) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      return hash;
    }
  };
  static std::mutex mutex;
  static std::unordered_map<Key, std::weak_ptr<const LazyTextBlob>, Hash> cache;
  uint32_t sizeBits = 0;
  const float fontSize = font->size();
  std::memcpy(&sizeBits, &fontSize, sizeof(sizeBits));
  const Key key{ font->typeface().get(), sizeBits, direction, mirror, text };
  const std::lock_guard lock(mutex);
  if (const auto found = cache.find(key); found != cache.end()) {
    if (const auto existing = found->second.lock()) return existing;
  }
  auto created = std::make_shared<const LazyTextBlob>(std::move(font), std::move(text), direction, mirror);
  if (cache.size() > 4096) {
    for (auto iterator = cache.begin(); iterator != cache.end();) {
      iterator = iterator->second.expired() ? cache.erase(iterator) : std::next(iterator);
    }
  }
  cache[key] = created;
  return created;
}

std::shared_ptr<const graphics::TextBlob> LazyTextBlob::get() const {
  std::lock_guard lock(mMutex);
  if (mBlob == nullptr) {
    const std::string painted = mMirror ? mirrorBidiText(mText) : mText;
    mBlob = detail::TextShapeCache::instance().shape(*mFont, painted, mDirection);
  }
  return mBlob;
}

VisualRunRef::VisualRunRef(const TextVisualRun& run, const std::ptrdiff_t offset,
    std::shared_ptr<const LazyTextBlob> lazy)
    : start(static_cast<size_t>(static_cast<std::ptrdiff_t>(run.start) + offset)),
      end(static_cast<size_t>(static_cast<std::ptrdiff_t>(run.end) + offset)),
      level(run.level), shaped(nullptr), lazyShaped(std::move(lazy)) {}

VisualRunRef VisualRunList::operator[](const size_t index) const {
  if (mBlock == nullptr || index >= mBlock->runs.size()) return VisualRunRef();
  return VisualRunRef(mBlock->runs[index].run, mOffset, mBlock->runs[index].lazy);
}

LineRef::LineRef(const TextLine& line, const std::ptrdiff_t offset)
    : text(line.text), width(line.width), xOffset(line.xOffset),
      justifySpacing(line.justifySpacing), start(static_cast<size_t>(
        static_cast<std::ptrdiff_t>(line.start) + offset)),
      end(static_cast<size_t>(static_cast<std::ptrdiff_t>(line.end) + offset)),
      hyphenated(line.hyphenated), rtl(line.rtl),
      visualRuns(line.visualRuns.block(), line.visualRuns.offset() + offset),
      ownedText(line.ownedText), ownedSource(line.ownedSource), shaped(line.shaped) {}

float LineList::maxWidth() const {
  float width = 0.0f;
  for (const Segment& segment: mSegments) {
    for (size_t index = segment.begin; index < segment.end; ++index) {
      width = std::max(width, (*segment.data)[index].width);
    }
  }
  if (mBuild != nullptr) {
    for (const TextLine& line: *mBuild) width = std::max(width, line.width);
  }
  return width;
}

LineRef LineList::refer(const size_t index) const {
  size_t remaining = index;
  for (const Segment& segment: mSegments) {
    const size_t length = segment.end - segment.begin;
    if (remaining < length) return LineRef((*segment.data)[segment.begin + remaining], segment.offset);
    remaining -= length;
  }
  if (mBuild != nullptr && remaining < mBuild->size()) return LineRef((*mBuild)[remaining], 0);
  return LineRef();
}

LineRef LineList::operator[](const size_t index) const {
  return refer(index);
}

TextLine& LineList::operator[](const size_t index) {
  materialize();
  return (*mBuild)[index];
}

void LineList::materialize() {
  if (mSegments.empty()) {
    if (mBuild == nullptr) mBuild = std::make_shared<std::vector<TextLine>>();
    return;
  }
  auto combined = std::make_shared<std::vector<TextLine>>();
  combined->reserve(mSize);
  for (const Segment& segment: mSegments) {
    for (size_t index = segment.begin; index < segment.end; ++index) {
      const TextLine& source = (*segment.data)[index];
      if (segment.offset == 0) {
        combined->push_back(source);
      } else {
        TextLine line = source;
        line.start = static_cast<size_t>(static_cast<std::ptrdiff_t>(line.start) + segment.offset);
        line.end = static_cast<size_t>(static_cast<std::ptrdiff_t>(line.end) + segment.offset);
        line.visualRuns = VisualRunList(line.visualRuns.block(),
          line.visualRuns.offset() + segment.offset);
        combined->push_back(std::move(line));
      }
    }
  }
  if (mBuild != nullptr) combined->insert(combined->end(), mBuild->begin(), mBuild->end());
  mSegments.clear();
  mBuild = std::move(combined);
}

std::vector<TextLine>::iterator LineList::begin() {
  materialize();
  return mBuild->begin();
}

std::vector<TextLine>::iterator LineList::end() {
  materialize();
  return mBuild->end();
}

void LineList::reserve(const size_t capacity) {
  if (mBuild == nullptr) mBuild = std::make_shared<std::vector<TextLine>>();
  mBuild->reserve(capacity);
}

void LineList::push_back(TextLine line) {
  if (mBuild == nullptr) mBuild = std::make_shared<std::vector<TextLine>>();
  mBuild->push_back(std::move(line));
  ++mSize;
}

void LineList::pop_back() {
  if (mSize == 0) return;
  if (mBuild != nullptr && !mBuild->empty()) {
    mBuild->pop_back();
  } else if (!mSegments.empty()) {
    Segment& last = mSegments.back();
    if (last.end > last.begin) {
      --last.end;
    }
    if (last.end == last.begin) mSegments.pop_back();
  }
  --mSize;
}

void LineList::resize(const size_t size) {
  materialize();
  mBuild->resize(size);
  mSize = size;
}

void LineList::clear() noexcept {
  mSegments.clear();
  if (mBuild != nullptr) mBuild->clear();
  mSize = 0;
}

void LineList::appendShared(const LineList& source, const size_t begin, const size_t end) {
  appendSharedShifted(source, begin, end, 0);
}

void LineList::appendSharedShifted(const LineList& source, size_t begin, size_t end,
    const std::ptrdiff_t offset) {
  end = std::min(end, source.size());
  if (begin >= end) return;
  size_t position = 0;
  const auto add = [&](const std::shared_ptr<const std::vector<TextLine>>& data,
                       const size_t dataBegin, const size_t length, const std::ptrdiff_t base) {
    const size_t overlapBegin = std::max(begin, position);
    const size_t overlapEnd = std::min(end, position + length);
    if (overlapBegin < overlapEnd) {
      mSegments.push_back({ data, dataBegin + overlapBegin - position,
        dataBegin + overlapEnd - position, base + offset });
      mSize += overlapEnd - overlapBegin;
    }
    position += length;
  };
  for (const Segment& segment: source.mSegments) {
    add(segment.data, segment.begin, segment.end - segment.begin, segment.offset);
  }
  if (source.mBuild != nullptr) {
    add(source.mBuild, 0, source.mBuild->size(), 0);
  }
}

void LineList::seal() {
  if (mBuild == nullptr || mBuild->empty()) return;
  mSegments.push_back({ mBuild, 0, mBuild->size(), 0 });
  mBuild.reset();
}



TextSource::TextSource(std::string text) {
  append(std::move(text));
}

void TextSource::append(std::string text) {
  if (text.empty()) return;
  auto data = std::make_shared<const std::string>(std::move(text));
  mBlocks.push_back({ data, 0, data->size() });
  mSize += data->size();
}

void TextSource::appendSlice(const TextSource& source, const size_t offset, const size_t length) {
  if (length == 0) return;
  size_t remaining = length;
  size_t position = offset;
  for (const Block& block: source.mBlocks) {
    if (remaining == 0) break;
    if (position >= block.length) {
      position -= block.length;
      continue;
    }
    const size_t take = std::min(remaining, block.length - position);
    const auto data = block.data;
    mBlocks.push_back({ data, block.begin + position, take });
    mSize += take;
    remaining -= take;
    position = 0;
  }
}

std::shared_ptr<const std::string> TextSource::blockOwner(const size_t index) const {
  if (index >= mBlocks.size()) return nullptr;
  return mBlocks[index].data;
}

std::string_view TextSource::block(const size_t index) const {
  if (index >= mBlocks.size()) return {};
  const Block& block = mBlocks[index];
  return std::string_view(*block.data).substr(block.begin, block.length);
}

std::string_view TextSource::view(const size_t offset, const size_t length) const {
  if (length == 0) return {};
  size_t position = offset;
  for (const Block& block: mBlocks) {
    if (position >= block.length) {
      position -= block.length;
      continue;
    }
    if (position + length <= block.length) {
      return std::string_view(*block.data).substr(block.begin + position, length);
    }
    return {};
  }
  return {};
}

size_t TextSource::commonPrefix(const std::string_view text) const {
  size_t matched = 0;
  for (const Block& block: mBlocks) {
    const std::string_view piece = std::string_view(*block.data).substr(block.begin, block.length);
    const size_t limit = std::min(piece.size(), text.size() - matched);
    size_t index = 0;
    while (index < limit && piece[index] == text[matched + index]) ++index;
    matched += index;
    if (index != piece.size() || matched == text.size()) break;
  }
  return matched;
}

size_t TextSource::commonSuffix(const std::string_view text) const {
  size_t matched = 0;
  for (auto iterator = mBlocks.rbegin(); iterator != mBlocks.rend(); ++iterator) {
    const Block& block = *iterator;
    const std::string_view piece = std::string_view(*block.data).substr(block.begin, block.length);
    const size_t limit = std::min(piece.size(), text.size() - matched);
    size_t index = 0;
    while (index < limit && piece[piece.size() - 1 - index] == text[text.size() - 1 - matched - index]) {
      ++index;
    }
    matched += index;
    if (index != piece.size() || matched == text.size()) break;
  }
  return matched;
}

bool TextSource::equals(const std::string_view text) const {
  if (text.size() != mSize) return false;
  size_t offset = 0;
  for (const Block& block: mBlocks) {
    const std::string_view piece = std::string_view(*block.data).substr(block.begin, block.length);
    if (text.substr(offset, piece.size()) != piece) return false;
    offset += piece.size();
  }
  return true;
}

std::string TextSource::toString() const {
  std::string result;
  result.reserve(mSize);
  for (const Block& block: mBlocks) {
    result.append(*block.data, block.begin, block.length);
  }
  return result;
}

namespace {

size_t sequenceLength(const std::string_view text, const size_t offset) {
  if (offset >= text.size()) return 0;
  const auto lead = static_cast<unsigned char>(text[offset]);
  if ((lead & 0x80u) == 0) return 1;
  if ((lead & 0xE0u) == 0xC0u) return 2;
  if ((lead & 0xF0u) == 0xE0u) return 3;
  if ((lead & 0xF8u) == 0xF0u) return 4;
  return 1;
}

uint32_t codePointAt(const std::string_view text, const size_t offset, const size_t length) {
  const auto lead = static_cast<unsigned char>(text[offset]);
  if (length == 1) return lead;
  if (length == 2) {
    return ((lead & 0x1Fu) << 6) | (static_cast<unsigned char>(text[offset + 1]) & 0x3Fu);
  }
  if (length == 3) {
    return ((lead & 0x0Fu) << 12) | ((static_cast<unsigned char>(text[offset + 1]) & 0x3Fu) << 6) |
           (static_cast<unsigned char>(text[offset + 2]) & 0x3Fu);
  }
  return ((lead & 0x07u) << 18) | ((static_cast<unsigned char>(text[offset + 1]) & 0x3Fu) << 12) |
         ((static_cast<unsigned char>(text[offset + 2]) & 0x3Fu) << 6) |
         (static_cast<unsigned char>(text[offset + 3]) & 0x3Fu);
}

size_t previousCodePointStart(const std::string_view text, const size_t end) {
  if (end == 0) return 0;
  size_t index = end - 1;
  while (index > 0 && (static_cast<unsigned char>(text[index]) & 0xC0u) == 0x80u) {
    --index;
  }
  return index;
}

void trimTrailingSpaces(std::string* line) {
  while (!line->empty() && line->back() == ' ') line->pop_back();
}

/** Drops a trailing U+00AD; a broken soft hyphen is rendered as a hyphen glyph instead. */
void trimTrailingSoftHyphens(std::string* line) {
  while (line->size() >= 2 && static_cast<unsigned char>((*line)[line->size() - 2]) == 0xC2u &&
         static_cast<unsigned char>((*line)[line->size() - 1]) == 0xADu) {
    line->resize(line->size() - 2);
  }
}

bool isMandatoryClass(const uint32_t codePoint) {
  const auto value = unicode::resolveLineBreakClass(codePoint);
  return value == unicode::LineBreakClass::BK || value == unicode::LineBreakClass::CR ||
         value == unicode::LineBreakClass::LF || value == unicode::LineBreakClass::NL;
}

/**
 * Emergency break guard: a line that has no legal opportunity must not split a
 * grapheme cluster. This covers the common extending marks, emoji modifiers and
 * the character after a ZWJ; a full UAX #29 segmenter can replace it later.
 */

/** Drops hard line-break characters so they are not rendered as a line's text. */
void trimTrailingSeparators(std::string* line) {
  while (!line->empty()) {
    const size_t start = previousCodePointStart(*line, line->size());
    const uint32_t codePoint = codePointAt(*line, start, line->size() - start);
    if (!isMandatoryClass(codePoint)) break;
    line->resize(start);
  }
}

bool requiresBidiSource(const TextSource& source) {
  for (size_t index = 0; index < source.blockCount(); ++index) {
    if (requiresBidi(source.block(index))) return true;
  }
  return false;
}

struct LineRunCacheKey {
  size_t textHash = 0;
  const graphics::Typeface* typeface = nullptr;
  uint32_t sizeBits = 0;
  std::string levels;
  std::shared_ptr<const std::string> owner;
  size_t begin = 0;
  size_t length = 0;

  bool operator==(const LineRunCacheKey& other) const {
    if (textHash != other.textHash || typeface != other.typeface ||
        sizeBits != other.sizeBits || levels != other.levels ||
        length != other.length) {
      return false;
    }
    if (owner == other.owner && begin == other.begin) return true;
    if (owner == nullptr || other.owner == nullptr) return false;
    return owner->compare(begin, length, *other.owner, other.begin, other.length) == 0;
  }
};

struct LineRunCacheHash {
  size_t operator()(const LineRunCacheKey& key) const noexcept {
    size_t hash = key.textHash;
    hash ^= std::hash<const void*>{}(key.typeface) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
    hash ^= std::hash<uint32_t>{}(key.sizeBits) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
    hash ^= std::hash<std::string>{}(key.levels) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
    return hash;
  }
};

class VisualRunBlockCache {
 public:
  static VisualRunBlockCache& instance() {
    static VisualRunBlockCache cache;
    return cache;
  }

  std::shared_ptr<const VisualRunBlock> find(const LineRunCacheKey& key) {
    std::lock_guard lock(mMutex);
    const auto found = mEntries.find(key);
    if (found == mEntries.end()) {
      ++mMisses;
      return nullptr;
    }
    if (auto block = found->second.lock()) {
      ++mHits;
      return block;
    }
    mEntries.erase(found);
    ++mMisses;
    return nullptr;
  }

  void store(const LineRunCacheKey& key, std::shared_ptr<const VisualRunBlock> block) {
    std::lock_guard lock(mMutex);
    ++mStores;
    if (mEntries.size() > 2048) {
      for (auto iterator = mEntries.begin(); iterator != mEntries.end();) {
        iterator = iterator->second.expired() ? mEntries.erase(iterator) : std::next(iterator);
      }
    }
    mEntries[key] = std::move(block);
  }

  BidiLineCacheStats stats() {
    std::lock_guard lock(mMutex);
    return { mHits.load(), mMisses.load(), mStores.load(), mEntries.size() };
  }

  void resetStats() {
    std::lock_guard lock(mMutex);
    mHits = 0;
    mMisses = 0;
    mStores = 0;
  }

 private:
  std::mutex mMutex;
  std::atomic<uint64_t> mHits{0};
  std::atomic<uint64_t> mMisses{0};
  std::atomic<uint64_t> mStores{0};
  std::unordered_map<LineRunCacheKey, std::weak_ptr<const VisualRunBlock>, LineRunCacheHash> mEntries;
};


void attachVisualRuns(const std::string& text, const graphics::Font* font, TextLayout& layout,
    const TextDirectionMode directionMode, const std::span<const BidiSpan> bidiSpans) {
  const bool forced = directionMode != TextDirectionMode::Auto;
  if (!forced && bidiSpans.empty() && !requiresBidi(text)) {
    layout.baseDirection = TextDirection::Ltr;
    return;
  }
  std::optional<TextDirection> requested;
  if (directionMode == TextDirectionMode::Ltr) requested = TextDirection::Ltr;
  if (directionMode == TextDirectionMode::Rtl) requested = TextDirection::Rtl;
  std::shared_ptr<const BidiParagraph> paragraph;
  if (!bidiSpans.empty()) {
    paragraph = std::make_shared<const BidiParagraph>(
      resolveBidiParagraphWithSpans(text, requested, bidiSpans));
  } else {
    paragraph = resolveBidiParagraphShared(text, requested, layout.source->blockOwner(0), 0);
  }
  if (paragraph == nullptr || paragraph->byteOffsets.size() != paragraph->levels.size()) return;
  layout.baseDirection = paragraph->baseLevel % 2 != 0 ? TextDirection::Rtl : TextDirection::Ltr;
  const bool baseRtl = paragraph->baseLevel % 2 != 0;
  const auto isL1Separator = [](const uint32_t codePoint) {
    return codePoint == 0x20u || codePoint == 0x09u || codePoint == 0x0Au ||
           codePoint == 0x0Du || codePoint == 0x2028u || codePoint == 0x2029u;
  };

  const std::shared_ptr<const graphics::Font> shapeFont =
    font != nullptr ? graphics::Font::create(font->typeface(), font->size()) : nullptr;
  for (TextLine& line: layout.lines) {
    if (line.text.empty()) continue;
    // A synthesized ellipsis line has no byte-for-byte mapping into the source
    // paragraph, so its visual runs cannot be derived from paragraph levels.
    if (line.ownedText != nullptr) {
      line.rtl = baseRtl;
      continue;
    }
    const size_t contentEnd = line.start + line.text.size();
    const auto first = std::lower_bound(paragraph->byteOffsets.begin(), paragraph->byteOffsets.end(), line.start);
    if (first == paragraph->byteOffsets.end()) continue;
    size_t index = static_cast<size_t>(first - paragraph->byteOffsets.begin());
    // UAX #9 L1: reset trailing whitespace/segment separators to the paragraph level.
    size_t resetStart = contentEnd;
    for (size_t position = index; position < paragraph->byteOffsets.size(); ++position) {
      const size_t offset = paragraph->byteOffsets[position];
      if (offset >= contentEnd) break;
      const size_t length = std::max<size_t>(1, sequenceLength(text, offset));
      if (isL1Separator(codePointAt(text, offset, length))) {
        if (resetStart == contentEnd) resetStart = offset;
      } else {
        resetStart = contentEnd;
      }
    }
    std::vector<VisualRunEntry> runs;
    const auto appendRun = [&](const size_t begin, const size_t end, const uint8_t level) {
      VisualRunEntry entry;
      entry.run = TextVisualRun{ begin, end, level };
      if (font != nullptr && end > begin) {
        const std::string logical(text.substr(begin, end - begin));
        const bool rightToLeft = level % 2 != 0;
        entry.lazy = LazyTextBlob::create(shapeFont, logical,
          rightToLeft ? graphics::Font::Direction::RightToLeft
                      : graphics::Font::Direction::LeftToRight,
          rightToLeft);
      }
      runs.push_back(std::move(entry));
    };
    bool needsReordering = false;
    size_t runStart = 0;
    uint8_t runLevel = 0;
    bool hasRun = false;
    for (; index < paragraph->byteOffsets.size() && paragraph->byteOffsets[index] < contentEnd; ++index) {
      uint8_t level = paragraph->levels[index];
      if (paragraph->byteOffsets[index] >= resetStart) level = paragraph->baseLevel;
      if (level == 0xFF) continue;
      if (level != 0) needsReordering = true;
      if (!hasRun) {
        runStart = paragraph->byteOffsets[index];
        runLevel = level;
        hasRun = true;
      } else if (level != runLevel) {
        appendRun(runStart, paragraph->byteOffsets[index], runLevel);
        runStart = paragraph->byteOffsets[index];
        runLevel = level;
      }
    }
    if (hasRun) appendRun(runStart, contentEnd, runLevel);
    if (needsReordering && !runs.empty()) {
      LineRunCacheKey cacheKey;
      if (font != nullptr) {
        cacheKey.textHash = std::hash<std::string_view>{}(line.text);
        cacheKey.typeface = font->typeface().get();
        const float fontSize = font->size();
        std::memcpy(&cacheKey.sizeBits, &fontSize, sizeof(cacheKey.sizeBits));
        cacheKey.levels.reserve(runs.size());
        for (const VisualRunEntry& entry: runs) {
          cacheKey.levels.push_back(static_cast<char>(entry.run.level));
        }
        cacheKey.owner = layout.source->blockOwner(0);
        if (cacheKey.owner != nullptr && line.text.data() >= cacheKey.owner->data() &&
            line.text.data() + line.text.size() <= cacheKey.owner->data() + cacheKey.owner->size()) {
          cacheKey.begin = static_cast<size_t>(line.text.data() - cacheKey.owner->data());
          cacheKey.length = line.text.size();
          if (const auto cached = VisualRunBlockCache::instance().find(cacheKey)) {
            line.visualRuns = VisualRunList(cached);
            continue;
          }
        } else {
          cacheKey.owner.reset();
        }
      }
      auto block = std::make_shared<VisualRunBlock>();
      block->runs = std::move(runs);
      const std::vector<size_t> order = visualRunOrder(VisualRunList(block));
      block->order.assign(order.begin(), order.end());
      if (font != nullptr && cacheKey.owner != nullptr) {
        VisualRunBlockCache::instance().store(cacheKey, block);
      }
      line.visualRuns = VisualRunList(std::move(block));
    }
  }
}

float measure(const graphics::Font& font, const std::string_view text) {
  return detail::TextShapeCache::instance().measure(font, text);
}

float effectiveLineHeight(const graphics::Font& font, const TextLayoutOptions& options) {
  if (options.lineHeight > 0.0f) return options.lineHeight;
  const graphics::FontMetrics& metrics = font.metrics();
  return metrics.ascent + metrics.descent + metrics.lineGap;
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

/**
 * CSS `text-align: justify` for space-separated text: the slack on every line
 * except the last line of a paragraph is spread after each U+0020. Bidi lines
 * and lines without spaces keep their natural width.
 */
void applyJustification(TextLayout& layout, const TextLayoutOptions& options,
    const std::string& text) {
  if (options.align != TextAlign::Justify || options.maxWidth <= 0.0f) return;
  const size_t count = layout.lines.size();
  for (size_t index = 0; index < count; ++index) {
    TextLine& line = layout.lines[index];
    line.justifySpacing = 0.0f;
    if (index + 1 >= count) continue;
    if (!line.visualRuns.empty()) continue;
    if (line.end < text.size() && (text[line.end] == '\n' || text[line.end] == '\r')) continue;
    const size_t spaces = static_cast<size_t>(
      std::count(line.text.begin(), line.text.end(), ' '));
    if (spaces == 0) continue;
    const float slack = options.maxWidth - line.width;
    if (slack > 0.0f) line.justifySpacing = slack / static_cast<float>(spaces);
  }
}

TextLayout layoutTextUncached(const std::string& input, const graphics::Font& font,
        const graphics::FontFallback* fallback, const TextLayoutOptions& options) {
  TextLayout layout;
  layout.source = std::make_shared<const TextSource>(input);
  const std::string& text = input;
  if (text.empty()) {
    layout.size = { 0.0f, 0.0f };
    return layout;
  }
  layout.lines.reserve(std::min<size_t>(8192, text.size() / 32 + 1));
  const graphics::FontMetrics& metrics = font.metrics();
  const float naturalLineHeight = metrics.ascent + metrics.descent + metrics.lineGap;
  const float lineHeight = options.lineHeight > 0.0f ? options.lineHeight : naturalLineHeight;
  layout.lineHeight = lineHeight;
  layout.align = options.align;
  layout.policy = std::make_shared<const LineBreakPolicy>(options.policy);
  const bool wrap = options.maxWidth > 0.0f;
  const auto measureText = [&](const std::string_view value) {
    if (value.find('\t') == std::string_view::npos) {
      return fallback != nullptr ? fallback->measure(value) : measure(font, value);
    }
    std::string expanded;
    expanded.reserve(value.size() + options.tabSize);
    for (const char character: value) {
      if (character == '\t' && options.tabSize > 0) {
        expanded.append(static_cast<size_t>(options.tabSize), ' ');
      } else {
        expanded.push_back(character);
      }
    }
    return fallback != nullptr ? fallback->measure(expanded) : measure(font, expanded);
  };
  // Line wrapping sums per-code-point advances. Cache the 128 ASCII values
  // directly and keep a small direct-mapped table for repeated non-ASCII
  // characters (CJK, Hebrew, Arabic, emoji components) so a 1 MiB line does not
  // shape the same character hundreds of thousands of times.
  struct CodepointWidth {
    uint32_t codePoint = 0;
    float width = 0.0f;
    bool known = false;
  };
  std::array<float, 128> asciiWidths{};
  std::array<bool, 128> asciiKnown{};
  std::array<CodepointWidth, 1024> widths{};
  const auto widthOf = [&](const uint32_t codePoint, const std::string_view piece) {
    if (codePoint < 128) {
      if (!asciiKnown[codePoint]) {
        asciiWidths[codePoint] = measureText(piece);
        asciiKnown[codePoint] = true;
      }
      return asciiWidths[codePoint];
    }
    CodepointWidth& entry = widths[(codePoint * 2654435761u) & 1023u];
    if (entry.known && entry.codePoint == codePoint) return entry.width;
    entry.codePoint = codePoint;
    entry.width = measureText(piece);
    entry.known = true;
    return entry.width;
  };

  // Shaped advance table: one shaping pass for the paragraph, so wrapping decisions use the same
  // advances that drawing will use. Fallback runs are concatenated by their own shaping pass. A tab
  // is expanded to spaces at draw time, so it keeps the per-code-point path.
  detail::ShapedAdvances shaped;
  if (wrap && text.find('\t') == std::string::npos) {
    graphics::Font::Direction direction = graphics::Font::Direction::Auto;
    if (options.direction == TextDirectionMode::Ltr) {
      direction = graphics::Font::Direction::LeftToRight;
    } else if (options.direction == TextDirectionMode::Rtl) {
      direction = graphics::Font::Direction::RightToLeft;
    }
    shaped = fallback != nullptr ? detail::ShapedAdvances::fromFallback(*fallback, text)
                                 : detail::ShapedAdvances::fromFont(font, text, direction);
  }

  const std::string hyphenText = "\xE2\x80\x90"; // U+2010 HYPHEN
  const float hyphenWidth = wrap ? measureText(hyphenText) : 0.0f;

  size_t lineStart = 0;
  std::string line;
  line.reserve(wrap
    ? std::min<size_t>(text.size(), static_cast<size_t>(options.maxWidth) * 2)
    : text.size());
  float lineWidth = 0.0f;
  size_t lastBreak = 0;
  bool lastBreakHyphen = false;
  size_t consecutiveHyphenated = 0;

  const auto flush = [&](const size_t nextStart, const bool hyphenated) {
    trimTrailingSeparators(&line);
    trimTrailingSpaces(&line);
    if (hyphenated) trimTrailingSoftHyphens(&line);
    TextLine entry;
    entry.start = lineStart;
    entry.end = lineStart + line.size();
    entry.text = layout.source->view(lineStart, line.size());
    entry.hyphenated = hyphenated && !entry.text.empty();
    entry.width = measureText(entry.text) + (entry.hyphenated ? hyphenWidth : 0.0f);
    entry.xOffset = alignmentOffset(options.align, options.maxWidth, entry.width);
    layout.lines.push_back(std::move(entry));
    consecutiveHyphenated = hyphenated ? consecutiveHyphenated + 1 : 0;
    line.clear();
    lineWidth = 0.0f;
    lastBreak = 0;
    lastBreakHyphen = false;
    lineStart = nextStart;
  };

  size_t offset = 0;
  unicode::GraphemeBreakState graphemeState;
  size_t graphemeProcessed = 0;
  const bool allowEmergency = options.policy.overflowWrap != OverflowWrap::Normal;
  const auto isGraphemeAt = [&](const size_t position) {
    while (graphemeProcessed < position && graphemeProcessed < text.size()) {
      const size_t length = std::max<size_t>(1, sequenceLength(text, graphemeProcessed));
      graphemeState.isBoundaryBefore(codePointAt(text, graphemeProcessed, length));
      graphemeProcessed += length;
    }
    if (graphemeProcessed != position) return true;
    const size_t length = std::max<size_t>(1, sequenceLength(text, position));
    const bool boundary = graphemeState.isBoundaryBefore(codePointAt(text, position, length));
    graphemeProcessed += length;
    return boundary;
  };
  const auto append = [&](const size_t position, const unicode::BreakAction action,
      const bool hyphenate) {
    const size_t length = std::max<size_t>(1, sequenceLength(text, position));
    const uint32_t cp = codePointAt(text, position, length);
    if (action == unicode::BreakAction::MandatoryBreak && position > lineStart) {
      flush(position, false);
    } else if (action == unicode::BreakAction::AllowBreak && !line.empty()) {
      // A soft hyphen immediately before the boundary also renders a hyphen.
      bool softHyphen = false;
      if (position > lineStart) {
        const size_t previousStart = previousCodePointStart(text, position);
        softHyphen = previousStart < position &&
                     codePointAt(text, previousStart, position - previousStart) == 0x00AD;
      }
      bool hyphenCandidate = hyphenate || softHyphen;
      if (hyphenCandidate && !softHyphen) {
        const size_t lineLimit = options.policy.hyphenateLimitLines;
        if (lineLimit != 0 && consecutiveHyphenated >= lineLimit) hyphenCandidate = false;
        const float zone = options.policy.hyphenateLimitZone;
        if (hyphenCandidate && zone > 0.0f && wrap) {
          const float slack = options.maxWidth - lineWidth - hyphenWidth;
          if (slack > zone) hyphenCandidate = false;
        }
      }
      if (hyphenCandidate || !hyphenate) {
        lastBreak = line.size();
        lastBreakHyphen = hyphenCandidate;
      }
    }

    const std::string_view piece = std::string_view(text).substr(position, length);
    const bool separator = isMandatoryClass(cp);
    const float pieceWidth = separator
                               ? 0.0f
                               : (shaped.valid() ? shaped.advance(position, position + length)
                                               : widthOf(cp, piece));
    const bool reserveHyphen = lastBreak > 0 && lastBreak == line.size() && lastBreakHyphen;
    if (wrap && !line.empty() &&
        lineWidth + pieceWidth + (reserveHyphen ? hyphenWidth : 0.0f) > options.maxWidth) {
      if (lastBreak > 0) {
        if (lastBreak == line.size()) {
          // The whole buffered line is the candidate, so the break is right here.
          flush(position, lastBreakHyphen);
        } else {
          std::string remainder = line.substr(lastBreak);
          size_t skipped = 0;
          while (skipped < remainder.size() && remainder[skipped] == ' ') ++skipped;
          const size_t fragmentEnd = lineStart + lastBreak;
          const bool hyphenated = lastBreakHyphen;
          line.resize(lastBreak);
          flush(fragmentEnd, hyphenated);
          lineStart += skipped;
          line = remainder.substr(skipped);
          lineWidth = measureText(line);
        }
      } else if (allowEmergency && isGraphemeAt(position)) {
        // No legal opportunity before the overflowing character: use the
        // emergency grapheme boundary; the UAX #29 state keeps Indic conjuncts,
        // emoji ZWJ sequences and RI flags together.
        flush(position, false);
      }
    }
    line.append(piece);
    lineWidth += pieceWidth;
    return position + length;
  };

  // Consume opportunities as they are produced instead of materializing them:
  // codepoints before the next opportunity are NoBreak, the opportunity itself
  // carries its action. Non-default policies materialize a tailored list because
  // they add/remove boundaries after the streaming pass.
  auto consume = [&](const size_t breakOffset, const unicode::BreakAction action,
      const bool hyphenate) {
    while (offset < breakOffset && offset < text.size()) {
      offset = append(offset, unicode::BreakAction::NoBreak, false);
    }
    if (offset == breakOffset && offset < text.size()) offset = append(offset, action, hyphenate);
  };
  if (options.policy.isDefault()) {
    unicode::forEachLineBreakOpportunity(text,
      [&](const size_t breakOffset, const unicode::BreakAction action) {
        consume(breakOffset, action, false);
      });
  } else {
    for (const TailoredLineBreak& opportunity: computeTailoredLineBreaks(text, options.policy)) {
      consume(opportunity.byteOffset,
        opportunity.mandatory ? unicode::BreakAction::MandatoryBreak
                              : unicode::BreakAction::AllowBreak,
        opportunity.hyphenate);
    }
  }
  while (offset < text.size()) offset = append(offset, unicode::BreakAction::NoBreak, false);
  if (!line.empty()) flush(text.size(), false);

  if (options.maxLines > 0 && static_cast<int>(layout.lines.size()) > options.maxLines) {
    layout.lines.resize(static_cast<size_t>(options.maxLines));
    layout.truncated = true;
    if (options.ellipsis && !layout.lines.empty()) {
      TextLine& last = layout.lines.back();
      const std::string ellipsis = "\xE2\x80\xA6";
      const std::vector<size_t> boundaries = unicode::computeGraphemeBreaks(last.text);
      bool applied = false;
      // boundaries[0] is 0 and the final one is the full text; only interior
      // grapheme boundaries are valid truncation points.
      for (size_t index = boundaries.size() - 1; index-- > 1;) {
        const size_t cut = boundaries[index];
        const std::string candidate = std::string(last.text.substr(0, cut)) + ellipsis;
        if (options.maxWidth > 0.0f && measureText(candidate) > options.maxWidth) continue;
        last.ownedText = std::make_shared<const std::string>(candidate);
        last.text = *last.ownedText;
        last.width = measureText(candidate);
        last.hyphenated = false;
        last.visualRuns = VisualRunList{};
        last.end = last.start + cut + ellipsis.size();
        applied = true;
        break;
      }
      if (!applied) {
        last.ownedText = std::make_shared<const std::string>(ellipsis);
        last.text = *last.ownedText;
        last.width = measureText(ellipsis);
        last.hyphenated = false;
        last.visualRuns = VisualRunList{};
        last.end = last.start + ellipsis.size();
      }
      last.xOffset = alignmentOffset(options.align, options.maxWidth, last.width);
    }
  }

  attachVisualRuns(text, fallback == nullptr ? &font : nullptr, layout, options.direction,
    options.bidiSpans);
  applyJustification(layout, options, text);
  if (fallback == nullptr) {
    for (TextLine& line: layout.lines) {
      if (line.text.empty() || !line.visualRuns.empty()) continue;
      line.shaped = detail::TextShapeCache::instance().shape(font, line.text);
    }
  }

  float width = 0.0f;
  width = layout.lines.maxWidth();
  layout.size = { width, lineHeight * static_cast<float>(layout.lines.size()) };
  layout.orientation = options.orientation;
  layout.directionMode = options.direction;
  if (options.orientation != TextOrientation::Horizontal) {
    // The horizontal breaker already produced column-length lines; transpose the
    // box so maxWidth is the column length and each line becomes a column.
    layout.size = { lineHeight * static_cast<float>(layout.lines.size()), width };
    for (TextLine& line: layout.lines) {
      line.xOffset = 0.0f;
    }
  }
  return layout;
}

} // namespace anonymous

std::optional<TextLayout> tryLayoutParagraphs(const std::string& text,
    const graphics::Font& font, const TextLayoutOptions& options) {
  if (options.maxLines != 0 || options.ellipsis) {
    return std::nullopt;
  }
  if (text.find_first_of("\r\n") == std::string::npos) return std::nullopt;

  TextLayout combined;
  auto source = std::make_shared<TextSource>(text);
  combined.source = source;
  const float naturalLineHeight = font.metrics().ascent + font.metrics().descent +
                                  font.metrics().lineGap;
  const float lineHeight = options.lineHeight > 0.0f ? options.lineHeight : naturalLineHeight;
  combined.lineHeight = lineHeight;
  combined.align = options.align;
  combined.policy = std::make_shared<const LineBreakPolicy>(options.policy);
  float width = 0.0f;
  size_t start = 0;
  while (start <= text.size()) {
    const size_t separator = text.find_first_of("\r\n", start);
    const size_t end = separator == std::string::npos ? text.size() : separator;
    const std::string paragraph = text.substr(start, end - start);
    const TextLayout part = layoutText(paragraph, font, options);
    if (part.lines.empty()) {
      TextLine empty;
      empty.start = start;
      empty.end = start;
      empty.xOffset = alignmentOffset(options.align, options.maxWidth, 0.0f);
      combined.lines.push_back(std::move(empty));
    } else {
      for (TextLine line: part.lines) {
        line.start += start;
        line.end += start;
        line.text = combined.source->view(line.start, line.end - line.start);
        line.visualRuns = VisualRunList(line.visualRuns.block(),
          line.visualRuns.offset() + static_cast<std::ptrdiff_t>(start));
        combined.lines.push_back(std::move(line));
      }
    }
    if (separator == std::string::npos) break;
    start = separator + 1;
    if (text[separator] == '\r' && start < text.size() && text[start] == '\n') ++start;
  }
  // A trailing separator does not create an extra empty paragraph line.
  if (!combined.lines.empty() && combined.lines.back().text.empty() &&
      combined.lines.back().start == text.size()) {
    combined.lines.pop_back();
  }
  width = combined.lines.maxWidth();
  combined.size = { width, lineHeight * static_cast<float>(combined.lines.size()) };
  return combined;
}

TextLayout layoutText(const std::string& text, const graphics::Font& font, const TextLayoutOptions& options) {
  const bool cacheable = options.maxLines == 0 && !options.ellipsis &&
                         options.bidiSpans.empty();
  if (cacheable) {
    if (const std::optional<TextLayout> cached =
                    detail::TextLayoutCache::instance().find(font, text, options)) {
      return *cached;
    }
    if (std::optional<TextLayout> segmented = tryLayoutParagraphs(text, font, options)) {
      detail::TextLayoutCache::instance().store(font, text, options, *segmented);
      return std::move(*segmented);
    }
    TextLayout layout = layoutTextUncached(text, font, nullptr, options);
    detail::TextLayoutCache::instance().store(font, text, options, layout);
    return layout;
  }
  return layoutTextUncached(text, font, nullptr, options);
}

TextLayout layoutText(const std::string& text, const graphics::Font& font, const graphics::FontFallback& fallback,
        const TextLayoutOptions& options) {
  return layoutTextUncached(text, font, &fallback, options);
}

TextLayout layoutText(const std::string& text, const graphics::Font& font, const float maxWidth) {
  return layoutText(text, font, TextLayoutOptions{ maxWidth, 0, false });
}

size_t paragraphStart(const std::string_view text, size_t offset) {
  while (offset > 0 && text[offset - 1] != '\n' && text[offset - 1] != '\r') --offset;
  return offset;
}

size_t paragraphEnd(const std::string_view text, size_t offset) {
  while (offset < text.size() && text[offset] != '\n' && text[offset] != '\r') ++offset;
  return offset;
}

std::optional<TextLayout> tryUpdateBidiParagraphs(const TextLayout& previous,
    const std::string_view newText, const graphics::Font& font, const TextLayoutOptions& options,
    const TextEditRange* edit) {
  if (previous.align != options.align || previous.orientation != options.orientation ||
      previous.directionMode != options.direction) {
    return std::nullopt;
  }
  const std::string oldText = previous.source->toString();
  size_t prefix = 0;
  size_t suffixLength = 0;
  if (edit != nullptr && edit->start <= oldText.size() && edit->start <= newText.size() &&
      edit->oldEnd <= oldText.size() && edit->newEnd <= newText.size() &&
      edit->oldEnd >= edit->start && edit->newEnd >= edit->start) {
    prefix = edit->start;
    suffixLength = std::min(oldText.size() - edit->oldEnd, newText.size() - edit->newEnd);
  } else {
    prefix = previous.source->commonPrefix(newText);
    suffixLength = std::min(previous.source->commonSuffix(newText),
      std::min(oldText.size() - prefix, newText.size() - prefix));
  }
  const size_t oldSuffixStart = oldText.size() - suffixLength;
  const size_t newSuffixStart = newText.size() - suffixLength;
  const size_t oldParagraphStart = paragraphStart(oldText, prefix);
  const size_t oldParagraphEnd = paragraphEnd(oldText, oldSuffixStart);
  const size_t newParagraphStart = paragraphStart(newText, prefix);
  const size_t newParagraphEnd = paragraphEnd(newText, newSuffixStart);
  if (oldParagraphStart == 0 && oldParagraphEnd == oldText.size() &&
      newParagraphStart == 0 && newParagraphEnd == newText.size()) {
    return std::nullopt;
  }

  size_t first = 0;
  while (first < previous.lines.size() && previous.lines[first].start < oldParagraphStart) ++first;
  size_t suffixIndex = 0;
  while (suffixIndex < previous.lines.size() &&
         previous.lines[suffixIndex].start < oldParagraphEnd) {
    ++suffixIndex;
  }

  auto sourceBuilder = std::make_shared<TextSource>();
  sourceBuilder->appendSlice(*previous.source, 0, newParagraphStart);
  if (newParagraphEnd > newParagraphStart) {
    sourceBuilder->append(std::string(newText.substr(newParagraphStart,
      newParagraphEnd - newParagraphStart)));
  }
  if (oldParagraphEnd < oldText.size()) {
    sourceBuilder->appendSlice(*previous.source, oldParagraphEnd,
      oldText.size() - oldParagraphEnd);
  }

  TextLayout combined;
  combined.source = sourceBuilder;
  combined.lines.appendShared(previous.lines, 0, first);
  if (first > 0) combined.borrowedSources.push_back(previous.source);

  if (newParagraphEnd > newParagraphStart) {
    const TextLayout paragraph = layoutText(
      std::string(newText.substr(newParagraphStart, newParagraphEnd - newParagraphStart)),
      font, options);
    for (TextLine line: paragraph.lines) {
      line.start += newParagraphStart;
      line.end += newParagraphStart;
      line.visualRuns = VisualRunList(line.visualRuns.block(),
        line.visualRuns.offset() + static_cast<std::ptrdiff_t>(newParagraphStart));
      line.text = combined.source->view(line.start, line.end - line.start);
      combined.lines.push_back(std::move(line));
    }
  }
  if (suffixIndex < previous.lines.size()) {
    const auto shift = static_cast<std::ptrdiff_t>(newSuffixStart) -
                       static_cast<std::ptrdiff_t>(oldSuffixStart);
    combined.lines.seal();
    combined.lines.appendSharedShifted(previous.lines, suffixIndex, previous.lines.size(), shift);
    combined.borrowedSources.push_back(previous.source);
  }
  const float lineHeight = effectiveLineHeight(font, options);
  combined.lineHeight = lineHeight;
  combined.align = options.align;
  combined.policy = std::make_shared<const LineBreakPolicy>(options.policy);
  combined.size = { combined.lines.maxWidth(),
    lineHeight * static_cast<float>(combined.lines.size()) };
  return combined;
}

BidiLineCacheStats bidiLineCacheStats() {
  return VisualRunBlockCache::instance().stats();
}

void resetBidiLineCacheStats() {
  VisualRunBlockCache::instance().resetStats();
}

TextLayout updateTextLayoutImpl(const TextLayout& previous, const std::string_view newText,
    const graphics::Font& font, const TextLayoutOptions& options, TextLayout* moveFrom,
    const TextEditRange* edit) {
  if (previous.source == nullptr || options.maxLines != 0 || options.ellipsis ||
      previous.align != options.align || previous.orientation != options.orientation ||
      previous.directionMode != options.direction) {
    return layoutText(std::string(newText), font, options);
  }
  const LineBreakPolicy previousPolicy = previous.policy != nullptr
    ? *previous.policy
    : LineBreakPolicy{};
  if (previousPolicy != options.policy) {
    return layoutText(std::string(newText), font, options);
  }
  if (requiresBidiSource(*previous.source) || requiresBidi(newText)) {
    if (std::optional<TextLayout> paragraph =
          tryUpdateBidiParagraphs(previous, newText, font, options, edit)) {
      return std::move(*paragraph);
    }
    return layoutText(std::string(newText), font, options);
  }

  const TextSource& oldText = *previous.source;
  size_t prefix = 0;
  size_t suffixLength = 0;
  if (edit != nullptr && edit->start <= oldText.size() && edit->start <= newText.size() &&
      edit->oldEnd <= oldText.size() && edit->newEnd <= newText.size() &&
      edit->oldEnd >= edit->start && edit->newEnd >= edit->start) {
    prefix = edit->start;
    suffixLength = std::min(oldText.size() - edit->oldEnd, newText.size() - edit->newEnd);
  } else {
    prefix = oldText.commonPrefix(newText);
    suffixLength = oldText.commonSuffix(newText);
  }
  if (prefix == oldText.size() && prefix == newText.size()) return previous;

  const size_t oldSuffixStart = oldText.size() - suffixLength;
  const size_t newSuffixStart = newText.size() - suffixLength;

  size_t first = 0;
  // Keep the line containing the edit in the reflowed range, including any
  // trimmed trailing whitespace between its text and the next line.
  while (first + 1 < previous.lines.size() && previous.lines[first + 1].start <= prefix) {
    ++first;
  }
  const size_t resume = std::min(first < previous.lines.size()
                                   ? previous.lines[first].start
                                   : (previous.lines.empty() ? 0 : previous.lines.back().start),
    newText.size());

  size_t suffixIndex = previous.lines.size();
  while (suffixIndex > 0 && previous.lines[suffixIndex - 1].start >= oldSuffixStart) {
    --suffixIndex;
  }
  size_t middleEnd = newText.size();
  if (suffixIndex < previous.lines.size() && suffixIndex > first) {
    const TextLine& firstSuffix = previous.lines[suffixIndex];
    const size_t suffixSkip = firstSuffix.start - oldSuffixStart;
    const size_t candidateStart = newSuffixStart + suffixSkip;
    const size_t candidateEnd =
      std::min(newText.size(), firstSuffix.end - oldSuffixStart + newSuffixStart);
    bool aligned = candidateStart == resume;
    if (!aligned && candidateStart > resume && candidateStart < newText.size()) {
      // Probe one line beyond the candidate so the preceding reflowed line cannot
      // silently absorb the suffix start. The candidate is reusable only if it is
      // still a complete line with the same text in that larger layout.
      size_t probeEnd = candidateEnd;
      if (suffixIndex + 1 < previous.lines.size()) {
        const size_t nextEnd = previous.lines[suffixIndex + 1].end - oldSuffixStart + newSuffixStart;
        probeEnd = std::min(newText.size(), std::max(probeEnd, nextEnd));
      }
      const TextLayout probe =
        layoutText(std::string(newText.substr(resume, probeEnd - resume)), font, options);
      for (const TextLine& line: probe.lines) {
        if (line.start + resume == candidateStart && line.end + resume == candidateEnd &&
            line.text == firstSuffix.text) {
          aligned = true;
          break;
        }
      }
    }
    if (aligned) middleEnd = candidateStart;
  }

  auto sourceBuilder = std::make_shared<TextSource>();
  sourceBuilder->appendSlice(oldText, 0, resume);
  if (middleEnd > resume) {
    sourceBuilder->append(std::string(newText.substr(resume, middleEnd - resume)));
  }
  if (middleEnd < newText.size()) {
    const size_t suffixOldStart = oldSuffixStart + (middleEnd - newSuffixStart);
    sourceBuilder->appendSlice(oldText, suffixOldStart, newText.size() - middleEnd);
  }

  TextLayout combined;
  combined.source = sourceBuilder;
  combined.lines.reserve(previous.lines.size() + 1);
  combined.lines.appendShared(previous.lines, 0, first);
  if (first > 0) combined.borrowedSources.push_back(previous.source);

  const auto appendRange = [&](const TextLayout& layout, const size_t base) {
    for (TextLine line: layout.lines) {
      line.start += base;
      line.end += base;
      line.visualRuns = VisualRunList(line.visualRuns.block(),
        line.visualRuns.offset() + static_cast<std::ptrdiff_t>(base));
      line.text = combined.source->view(line.start, line.end - line.start);
      combined.lines.push_back(std::move(line));
    }
  };

  if (resume < middleEnd) {
    const TextLayout middle =
      layoutText(std::string(newText.substr(resume, middleEnd - resume)), font, options);
    appendRange(middle, resume);
  }
  if (middleEnd < newText.size() && suffixIndex < previous.lines.size()) {
    const auto offset = static_cast<std::ptrdiff_t>(newSuffixStart) -
                        static_cast<std::ptrdiff_t>(oldSuffixStart);
    combined.lines.seal();
    combined.lines.appendSharedShifted(previous.lines, suffixIndex, previous.lines.size(), offset);
    combined.borrowedSources.push_back(previous.source);
  }

  const float lineHeight = effectiveLineHeight(font, options);
  combined.lineHeight = lineHeight;
  combined.align = options.align;
  combined.policy = std::make_shared<const LineBreakPolicy>(options.policy);
  float width = 0.0f;
  width = combined.lines.maxWidth();
  combined.size = { width, lineHeight * static_cast<float>(combined.lines.size()) };
  return combined;
}

TextLayout updateTextLayout(const TextLayout& previous, const std::string_view newText,
    const graphics::Font& font, const TextLayoutOptions& options) {
  return updateTextLayoutImpl(previous, newText, font, options, nullptr, nullptr);
}

TextLayout updateTextLayout(const TextLayout& previous, const std::string_view newText,
    const TextEditRange& edit, const graphics::Font& font, const TextLayoutOptions& options) {
  return updateTextLayoutImpl(previous, newText, font, options, nullptr, &edit);
}

TextLayout updateTextLayout(TextLayout&& previous, const std::string_view newText,
    const graphics::Font& font, const TextLayoutOptions& options) {
  return updateTextLayoutImpl(previous, newText, font, options, &previous, nullptr);
}

TextLayout updateTextLayout(TextLayout&& previous, const std::string_view newText,
    const TextEditRange& edit, const graphics::Font& font, const TextLayoutOptions& options) {
  return updateTextLayoutImpl(previous, newText, font, options, &previous, &edit);
}

std::vector<size_t> visualRunOrder(const TextLine& line) {
  return visualRunOrder(line.visualRuns);
}

std::vector<size_t> visualRunOrder(const std::vector<TextVisualRun>& runs) {
  auto block = std::make_shared<VisualRunBlock>();
  block->runs.reserve(runs.size());
  for (const TextVisualRun& run: runs) block->runs.push_back({ run, nullptr });
  return visualRunOrder(VisualRunList(std::move(block)));
}

std::vector<size_t> visualRunOrder(const VisualRunList& runs) {
  std::vector<size_t> order(runs.size());
  std::iota(order.begin(), order.end(), 0);
  if (runs.empty()) return order;

  uint8_t highest = 0;
  uint8_t lowestOdd = 0;
  bool hasOdd = false;
  for (const TextVisualRun& run: runs) {
    highest = std::max(highest, run.level);
    if (run.level % 2 != 0) {
      lowestOdd = hasOdd ? std::min(lowestOdd, run.level) : run.level;
      hasOdd = true;
    }
  }
  if (!hasOdd) return order;
  for (int level = highest; level >= static_cast<int>(lowestOdd); --level) {
    size_t index = 0;
    while (index < order.size()) {
      if (runs[order[index]].level < level) {
        ++index;
        continue;
      }
      size_t end = index;
      while (end < order.size() && runs[order[end]].level >= level) ++end;
      std::reverse(order.begin() + static_cast<std::ptrdiff_t>(index),
        order.begin() + static_cast<std::ptrdiff_t>(end));
      index = end;
    }
  }
  return order;
}

} // namespace coral::ui
