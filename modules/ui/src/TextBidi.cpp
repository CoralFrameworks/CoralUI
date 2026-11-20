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

#include "unicode/BidiMirroring.generated.h"
#include "unicode/BidiProperties.generated.h"
#include "unicode/BidiResolver.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

namespace coral::ui {

namespace {

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

unicode::BidiClass bidiClassOf(const uint32_t codePoint) {
  return codePoint < 128 ? unicode::kAsciiBidiClasses[codePoint] : unicode::bidiClass(codePoint);
}

bool isRtlClass(const unicode::BidiClass value) {
  return value == unicode::BidiClass::R || value == unicode::BidiClass::AL;
}

bool isStrongClass(const unicode::BidiClass value) { return value == unicode::BidiClass::L || isRtlClass(value); }

} // namespace

TextDirection baseDirection(const std::string_view text) {
  size_t offset = 0;
  while (offset < text.size()) {
    const auto lead = static_cast<unsigned char>(text[offset]);
    const size_t length = lead < 0x80u ? 1u : lead < 0xE0u ? 2u : lead < 0xF0u ? 3u : 4u;
    if (offset + length > text.size()) break;
    const uint32_t cp = codePointAt(text, offset, length);
    const unicode::BidiClass value = bidiClassOf(cp);
    if (isStrongClass(value)) {
      return isRtlClass(value) ? TextDirection::Rtl : TextDirection::Ltr;
    }
    offset += length;
  }
  return TextDirection::Ltr;
}

bool requiresBidi(const std::string_view text) {
  size_t offset = 0;
  while (offset < text.size()) {
    const auto lead = static_cast<unsigned char>(text[offset]);
    const size_t length = lead < 0x80u ? 1u : lead < 0xE0u ? 2u : lead < 0xF0u ? 3u : 4u;
    if (offset + length > text.size()) break;
    const uint32_t cp = codePointAt(text, offset, length);
    const unicode::BidiClass value = bidiClassOf(cp);
    switch (value) {
      case unicode::BidiClass::R:
      case unicode::BidiClass::AL:
      case unicode::BidiClass::LRE:
      case unicode::BidiClass::LRO:
      case unicode::BidiClass::RLE:
      case unicode::BidiClass::RLO:
      case unicode::BidiClass::PDF:
      case unicode::BidiClass::LRI:
      case unicode::BidiClass::RLI:
      case unicode::BidiClass::FSI:
      case unicode::BidiClass::PDI:
        return true;
      default:
        break;
    }
    offset += length;
  }
  return false;
}

std::vector<BidiRun> bidiRuns(const std::string_view text) {
  std::vector<BidiRun> runs;
  size_t offset = 0;
  TextDirection current = TextDirection::Ltr;
  bool hasCurrent = false;
  while (offset < text.size()) {
    const auto lead = static_cast<unsigned char>(text[offset]);
    const size_t length = lead < 0x80u ? 1u : lead < 0xE0u ? 2u : lead < 0xF0u ? 3u : 4u;
    if (offset + length > text.size()) break;
    const uint32_t cp = codePointAt(text, offset, length);
    const TextDirection direction = isRtlClass(bidiClassOf(cp)) ? TextDirection::Rtl : TextDirection::Ltr;
    if (!hasCurrent || direction != current) {
      if (hasCurrent) runs.back().end = offset;
      runs.push_back({ offset, offset + length, direction });
      current = direction;
      hasCurrent = true;
    } else {
      runs.back().end = offset + length;
    }
    offset += length;
  }
  return runs;
}

std::vector<size_t> visualRunOrder(const std::vector<BidiRun>& runs, const TextDirection base) {
  std::vector<size_t> order(runs.size());
  for (size_t index = 0; index < runs.size(); ++index) order[index] = index;
  if (base == TextDirection::Rtl) std::reverse(order.begin(), order.end());
  return order;
}

namespace {

/**
 * Process-wide LRU of resolved paragraphs. Editors rebuild a layout on almost
 * every keystroke, but most paragraphs are untouched; caching each paragraph by
 * its text and base-direction request turns the UBA pass into a map lookup.
 */
class BidiParagraphCache {
 public:
  static BidiParagraphCache& instance() {
    static BidiParagraphCache cache;
    return cache;
  }

  struct Key {
    size_t hash = 0;
    int direction = -1;
    std::shared_ptr<const std::string> owner;
    size_t begin = 0;
    size_t length = 0;

    bool operator==(const Key& other) const {
      if (hash != other.hash || direction != other.direction || length != other.length ||
          owner == nullptr || other.owner == nullptr) {
        return false;
      }
      if (owner == other.owner && begin == other.begin) return true;
      return owner->compare(begin, length, *other.owner, other.begin, other.length) == 0;
    }
  };

  struct Hash {
    size_t operator()(const Key& key) const noexcept {
      size_t hash = key.hash;
      hash ^= std::hash<int>{}(key.direction) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      return hash;
    }
  };

  std::shared_ptr<const BidiParagraph> findShared(const std::string_view text,
      const int direction, std::shared_ptr<const std::string> owner, const size_t offset) {
    if (owner == nullptr || offset + text.size() > owner->size()) return nullptr;
    const Key key{ std::hash<std::string_view>{}(text), direction, std::move(owner), offset,
      text.size() };
    std::lock_guard<std::mutex> lock(mMutex);
    const auto found = mEntries.find(key);
    if (found == mEntries.end()) return nullptr;
    mOrder.splice(mOrder.begin(), mOrder, found->second.order);
    return found->second.paragraph;
  }

  std::optional<BidiParagraph> find(const std::string_view text, const int direction) {
    const auto owner = std::make_shared<const std::string>(text);
    const auto paragraph = findShared(text, direction, owner, 0);
    if (paragraph == nullptr) return std::nullopt;
    return *paragraph;
  }

  void store(const std::string_view text, const int direction,
      std::shared_ptr<const BidiParagraph> paragraph,
      std::shared_ptr<const std::string> owner = nullptr, size_t offset = 0) {
    if (paragraph == nullptr) return;
    if (owner == nullptr) {
      owner = std::make_shared<const std::string>(text);
      offset = 0;
    }
    if (offset + text.size() > owner->size()) return;
    Key key{ std::hash<std::string_view>{}(text), direction, std::move(owner), offset, text.size() };
    std::lock_guard<std::mutex> lock(mMutex);
    if (const auto found = mEntries.find(key); found != mEntries.end()) {
      mBytes -= found->second.bytes;
      mOrder.erase(found->second.order);
      mEntries.erase(found);
    }
    mBytes += key.length;
    mOrder.push_front(key);
    Entry& entry = mEntries[key];
    entry.paragraph = std::move(paragraph);
    entry.order = mOrder.begin();
    entry.bytes = key.length;
    while (mEntries.size() > kMaxEntries || (mBytes > kMaxBytes && mEntries.size() > 1)) {
      const Key& victim = mOrder.back();
      const auto found = mEntries.find(victim);
      if (found == mEntries.end()) break;
      mBytes -= found->second.bytes;
      mEntries.erase(found);
      mOrder.pop_back();
    }
  }

  void store(const std::string_view text, const int direction, BidiParagraph paragraph) {
    store(text, direction, std::make_shared<const BidiParagraph>(std::move(paragraph)));
  }

 private:
  struct Entry {
    std::shared_ptr<const BidiParagraph> paragraph;
    std::list<Key>::iterator order;
    size_t bytes = 0;
  };

  static constexpr size_t kMaxEntries = 128;
  static constexpr size_t kMaxBytes = 4u * 1024u * 1024u;

  std::mutex mMutex;
  std::list<Key> mOrder;
  std::unordered_map<Key, Entry, Hash> mEntries;
  size_t mBytes = 0;
};

BidiParagraph resolveSingleParagraphValue(std::string_view utf8,
    std::optional<TextDirection> direction);

std::shared_ptr<const BidiParagraph> resolveSingleParagraphShared(const std::string_view utf8,
    const std::optional<TextDirection> direction, std::shared_ptr<const std::string> owner,
    const size_t offset) {
  const int paragraphDirection = !direction.has_value()
    ? -1
    : (*direction == TextDirection::Rtl ? 1 : 0);
  if (auto cached = BidiParagraphCache::instance().findShared(utf8, paragraphDirection,
        owner, offset)) {
    return cached;
  }
  auto paragraph = std::make_shared<BidiParagraph>(
    resolveSingleParagraphValue(utf8, direction));
  BidiParagraphCache::instance().store(utf8, paragraphDirection, paragraph,
    std::move(owner), offset);
  return paragraph;
}


BidiParagraph resolveSingleParagraphValue(const std::string_view utf8,
    const std::optional<TextDirection> direction) {
  const int paragraphDirection = !direction.has_value()
    ? -1
    : (*direction == TextDirection::Rtl ? 1 : 0);

  std::vector<uint32_t> codePoints;
  codePoints.reserve(utf8.size());
  std::vector<size_t> byteOffsets;
  byteOffsets.reserve(utf8.size());
  size_t offset = 0;
  while (offset < utf8.size()) {
    const auto lead = static_cast<unsigned char>(utf8[offset]);
    const size_t length = lead < 0x80u ? 1u : lead < 0xE0u ? 2u : lead < 0xF0u ? 3u : 4u;
    if (offset + length > utf8.size()) break;
    byteOffsets.push_back(offset);
    codePoints.push_back(codePointAt(utf8, offset, length));
    offset += length;
  }

  const unicode::BidiParagraphResult resolved = unicode::resolveBidiParagraph(codePoints,
    paragraphDirection);
  BidiParagraph result;
  result.baseLevel = resolved.baseLevel;
  result.byteOffsets = std::move(byteOffsets);
  result.levels = resolved.levels;
  result.visualOrder = resolved.visualOrder;
  return result;
}

BidiParagraph resolveSingleParagraph(const std::string_view utf8,
    const std::optional<TextDirection> direction) {
  const int paragraphDirection = !direction.has_value()
    ? -1
    : (*direction == TextDirection::Rtl ? 1 : 0);
  if (const auto cached = BidiParagraphCache::instance().findShared(utf8, paragraphDirection,
        std::make_shared<const std::string>(utf8), 0)) {
    return *cached;
  }
  BidiParagraph result = resolveSingleParagraphValue(utf8, direction);
  BidiParagraphCache::instance().store(utf8, paragraphDirection, result);
  return result;
}

bool hasParagraphSeparator(const std::string_view utf8) {
  size_t offset = 0;
  while (offset < utf8.size()) {
    const auto lead = static_cast<unsigned char>(utf8[offset]);
    const size_t length = lead < 0x80u ? 1u : lead < 0xE0u ? 2u : lead < 0xF0u ? 3u : 4u;
    if (offset + length > utf8.size()) break;
    if (unicode::bidiClass(codePointAt(utf8, offset, length)) == unicode::BidiClass::B) return true;
    offset += length;
  }
  return false;
}

} // namespace

BidiParagraph resolveBidiParagraph(const std::string_view utf8, const std::optional<TextDirection> direction) {
  if (!hasParagraphSeparator(utf8)) return resolveSingleParagraph(utf8, direction);

  BidiParagraph combined;
  size_t segmentStart = 0;
  size_t codePointOffset = 0;
  bool first = true;
  for (size_t offset = 0; offset < utf8.size();) {
    const auto lead = static_cast<unsigned char>(utf8[offset]);
    const size_t length = lead < 0x80u ? 1u : lead < 0xE0u ? 2u : lead < 0xF0u ? 3u : 4u;
    if (offset + length > utf8.size()) break;
    const bool separator = unicode::bidiClass(codePointAt(utf8, offset, length)) == unicode::BidiClass::B;
    offset += length;
    if (!separator) continue;
    BidiParagraph paragraph = resolveSingleParagraph(
      utf8.substr(segmentStart, offset - segmentStart), direction);
    if (first) {
      combined.baseLevel = paragraph.baseLevel;
      first = false;
    }
    for (const size_t byte: paragraph.byteOffsets) combined.byteOffsets.push_back(
      segmentStart + byte);
    for (const uint8_t level: paragraph.levels) combined.levels.push_back(level);
    for (const size_t index: paragraph.visualOrder) combined.visualOrder.push_back(
      codePointOffset + index);
    codePointOffset += paragraph.levels.size();
    segmentStart = offset;
  }
  if (segmentStart < utf8.size()) {
    BidiParagraph paragraph = resolveSingleParagraph(utf8.substr(segmentStart), direction);
    if (first) {
      combined.baseLevel = paragraph.baseLevel;
      first = false;
    }
    for (const size_t byte: paragraph.byteOffsets) combined.byteOffsets.push_back(
      segmentStart + byte);
    for (const uint8_t level: paragraph.levels) combined.levels.push_back(level);
    for (const size_t index: paragraph.visualOrder) combined.visualOrder.push_back(
      codePointOffset + index);
  }
  return combined;
}

std::shared_ptr<const BidiParagraph> resolveBidiParagraphShared(
    const std::string_view utf8, const std::optional<TextDirection> direction) {
  return resolveBidiParagraphShared(utf8, direction,
    std::make_shared<const std::string>(utf8), 0);
}

std::shared_ptr<const BidiParagraph> resolveBidiParagraphShared(
    const std::string_view utf8, const std::optional<TextDirection> direction,
    std::shared_ptr<const std::string> owner, const size_t offset) {
  if (!hasParagraphSeparator(utf8)) {
    return resolveSingleParagraphShared(utf8, direction, std::move(owner), offset);
  }
  return std::make_shared<const BidiParagraph>(resolveBidiParagraph(utf8, direction));
}

void applyBidiSpans(BidiParagraph& paragraph, const std::span<const BidiSpan> spans) {
  if (spans.empty() || paragraph.levels.empty()) return;
  std::vector<BidiSpan> ordered(spans.begin(), spans.end());
  std::sort(ordered.begin(), ordered.end(), [](const BidiSpan& left, const BidiSpan& right) {
    if (left.start != right.start) return left.start < right.start;
    return left.end > right.end;
  });
  const auto pushLevel = [](uint8_t level, const TextDirection direction) {
    if (direction == TextDirection::Rtl) {
      return static_cast<uint8_t>(level % 2 == 0 ? level + 1 : level + 2);
    }
    return static_cast<uint8_t>(level % 2 == 0 ? level + 2 : level + 1);
  };
  for (size_t index = 0; index < paragraph.byteOffsets.size(); ++index) {
    const size_t offset = paragraph.byteOffsets[index];
    if (paragraph.levels[index] == 0xFF) continue;
    uint8_t level = paragraph.baseLevel;
    for (const BidiSpan& span: ordered) {
      if (offset < span.start || offset >= span.end) continue;
      switch (span.mode) {
        case UnicodeBidi::Normal:
        case UnicodeBidi::Plaintext:
          break;
        case UnicodeBidi::Embed:
        case UnicodeBidi::Isolate:
          level = pushLevel(level, span.direction);
          break;
        case UnicodeBidi::BidiOverride:
          if (level % 2 != (span.direction == TextDirection::Rtl ? 1 : 0)) {
            level = pushLevel(level, span.direction);
          }
          break;
        case UnicodeBidi::IsolateOverride:
          level = pushLevel(level, span.direction);
          break;
      }
    }
    paragraph.levels[index] = level;
  }
}

BidiParagraph resolveBidiParagraphWithSpans(const std::string_view utf8,
    const std::optional<TextDirection> direction, const std::span<const BidiSpan> spans) {
  if (spans.empty()) return resolveBidiParagraph(utf8, direction);

  std::vector<uint32_t> codePoints;
  std::vector<size_t> byteOffsets;
  size_t offset = 0;
  while (offset < utf8.size()) {
    const auto lead = static_cast<unsigned char>(utf8[offset]);
    const size_t length = lead < 0x80u ? 1u : lead < 0xE0u ? 2u : lead < 0xF0u ? 3u : 4u;
    if (offset + length > utf8.size()) break;
    byteOffsets.push_back(offset);
    codePoints.push_back(codePointAt(utf8, offset, length));
    offset += length;
  }

  struct Event {
    size_t position = 0;
    bool open = false;
    size_t start = 0;
    size_t end = 0;
    std::vector<uint32_t> controls;
  };
  std::vector<Event> events;
  struct OrderedSpan { BidiSpan span; size_t index; };
  std::vector<OrderedSpan> ordered;
  ordered.reserve(spans.size());
  for (size_t index = 0; index < spans.size(); ++index) {
    if (spans[index].mode == UnicodeBidi::Normal) continue;
    if (spans[index].start < spans[index].end && spans[index].end <= utf8.size()) {
      ordered.push_back({ spans[index], index });
    }
  }
  std::sort(ordered.begin(), ordered.end(), [](const OrderedSpan& left, const OrderedSpan& right) {
    if (left.span.start != right.span.start) return left.span.start < right.span.start;
    return left.span.end > right.span.end;
  });

  const auto controlsFor = [&](const BidiSpan& span) {
    std::vector<uint32_t> open;
    std::vector<uint32_t> close;
    TextDirection spanDirection = span.direction;
    if (span.mode == UnicodeBidi::Plaintext) {
      spanDirection = baseDirection(utf8.substr(span.start, span.end - span.start));
    }
    const bool rtl = spanDirection == TextDirection::Rtl;
    switch (span.mode) {
      case UnicodeBidi::Normal:
      case UnicodeBidi::Plaintext:
        open.push_back(rtl ? 0x2067u : 0x2066u);
        close.push_back(0x2069u);
        break;
      case UnicodeBidi::Embed:
        open.push_back(rtl ? 0x202Bu : 0x202Au);
        close.push_back(0x202Cu);
        break;
      case UnicodeBidi::Isolate:
        open.push_back(rtl ? 0x2067u : 0x2066u);
        close.push_back(0x2069u);
        break;
      case UnicodeBidi::BidiOverride:
        open.push_back(rtl ? 0x202Eu : 0x202Du);
        close.push_back(0x202Cu);
        break;
      case UnicodeBidi::IsolateOverride:
        open.push_back(rtl ? 0x2067u : 0x2066u);
        open.push_back(rtl ? 0x202Eu : 0x202Du);
        close.push_back(0x202Cu);
        close.push_back(0x2069u);
        break;
    }
    return std::pair{ std::move(open), std::move(close) };
  };

  for (const OrderedSpan& entry: ordered) {
    auto [open, close] = controlsFor(entry.span);
    Event openEvent{ entry.span.start, true, entry.span.start, entry.span.end, std::move(open) };
    Event closeEvent{ entry.span.end, false, entry.span.start, entry.span.end, std::move(close) };
    events.push_back(std::move(openEvent));
    events.push_back(std::move(closeEvent));
  }
  std::sort(events.begin(), events.end(), [](const Event& left, const Event& right) {
    if (left.position != right.position) return left.position < right.position;
    if (left.open != right.open) return !left.open;
    if (left.open) return left.end > right.end;
    return left.start > right.start;
  });

  std::vector<uint32_t> expanded;
  expanded.reserve(codePoints.size() + events.size() * 3);
  std::vector<int> origins;
  origins.reserve(expanded.capacity());
  size_t eventIndex = 0;
  const auto appendControls = [&](const size_t position) {
    while (eventIndex < events.size() && events[eventIndex].position == position) {
      for (const uint32_t control: events[eventIndex].controls) {
        expanded.push_back(control);
        origins.push_back(-1);
      }
      ++eventIndex;
    }
  };
  for (size_t index = 0; index < codePoints.size(); ++index) {
    appendControls(byteOffsets[index]);
    expanded.push_back(codePoints[index]);
    origins.push_back(static_cast<int>(index));
  }
  appendControls(utf8.size());

  const int paragraphDirection = !direction.has_value()
    ? -1
    : (*direction == TextDirection::Rtl ? 1 : 0);
  const unicode::BidiParagraphResult resolved = unicode::resolveBidiParagraph(expanded,
    paragraphDirection);
  BidiParagraph result;
  result.baseLevel = resolved.baseLevel;
  result.byteOffsets = std::move(byteOffsets);
  result.levels.assign(codePoints.size(), 0xFF);
  for (size_t index = 0; index < origins.size() && index < resolved.levels.size(); ++index) {
    if (origins[index] >= 0) {
      result.levels[static_cast<size_t>(origins[index])] = resolved.levels[index];
    }
  }
  return result;
}

std::string mirrorBidiText(const std::string_view utf8) {
  std::string result;
  result.reserve(utf8.size());
  size_t offset = 0;
  while (offset < utf8.size()) {
    const auto lead = static_cast<unsigned char>(utf8[offset]);
    const size_t length = lead < 0x80u ? 1u : lead < 0xE0u ? 2u : lead < 0xF0u ? 3u : 4u;
    if (offset + length > utf8.size()) break;
    uint32_t codePoint = codePointAt(utf8, offset, length);
    const uint32_t mirror = unicode::bidiMirror(codePoint);
    if (mirror != 0) codePoint = mirror;
    if (codePoint < 0x80u) {
      result.push_back(static_cast<char>(codePoint));
    } else if (codePoint < 0x800u) {
      result.push_back(static_cast<char>(0xC0u | (codePoint >> 6)));
      result.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    } else if (codePoint < 0x10000u) {
      result.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
      result.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
      result.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    } else {
      result.push_back(static_cast<char>(0xF0u | (codePoint >> 18)));
      result.push_back(static_cast<char>(0x80u | ((codePoint >> 12) & 0x3Fu)));
      result.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
      result.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    }
    offset += length;
  }
  return result;
}

std::shared_ptr<const std::string> mirrorBidiTextShared(const std::string_view utf8) {
  struct Cache {
    std::mutex mutex;
    std::unordered_map<std::string, std::shared_ptr<const std::string>> entries;
    std::deque<std::string> lru;
  };
  static Cache cache;
  const std::string key(utf8);
  {
    std::lock_guard<std::mutex> lock(cache.mutex);
    const auto found = cache.entries.find(key);
    if (found != cache.entries.end()) return found->second;
  }
  auto mirrored = std::make_shared<const std::string>(mirrorBidiText(utf8));
  {
    std::lock_guard<std::mutex> lock(cache.mutex);
    if (cache.entries.size() >= 512) {
      if (!cache.lru.empty()) {
        cache.entries.erase(cache.lru.front());
        cache.lru.pop_front();
      }
    }
    cache.entries.emplace(key, mirrored);
    cache.lru.push_back(key);
  }
  return mirrored;
}

} // namespace coral::ui
