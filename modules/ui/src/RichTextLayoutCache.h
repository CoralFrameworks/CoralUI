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

#ifndef CORAL_UI_RICHTEXTLAYOUTCACHE_H
#define CORAL_UI_RICHTEXTLAYOUTCACHE_H

#include <ui/RichText.h>

#include "PolicyCacheKey.h"

#include <graphics/FontFallback.h>

#include <cstdint>
#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace coral::ui::detail {

/**
 * Process-wide LRU cache of rich-text layouts.
 *
 * The key includes every run's typeface/size/text plus the style fields that end up on the laid-out
 * segments (colour and underline) and the wrap width. Two layers showing the same styled paragraph at
 * the same width reuse its line breaks and segment widths; the cache keeps typefaces alive and evicts
 * the least-recently-used layout when full.
 */
class RichTextLayoutCache {
 public:
  static RichTextLayoutCache& instance();

  [[nodiscard]] std::optional<RichTextLayout> find(const std::vector<RichTextRun>& runs,
    const RichTextLayoutOptions& options);
  void store(const std::vector<RichTextRun>& runs, const RichTextLayoutOptions& options,
    RichTextLayout layout);

 private:
  RichTextLayoutCache() = default;

  struct RunKey {
    const graphics::Typeface* typeface = nullptr;
    uint32_t sizeBits = 0;
    const graphics::FontFallback* fallback = nullptr;
    std::shared_ptr<const std::string> textOwner;
    std::string_view text;
    uint32_t redBits = 0;
    uint32_t greenBits = 0;
    uint32_t blueBits = 0;
    uint32_t alphaBits = 0;
    bool underline = false;

    bool operator==(const RunKey& other) const noexcept {
      return typeface == other.typeface && sizeBits == other.sizeBits &&
        fallback == other.fallback && text == other.text &&
        redBits == other.redBits && greenBits == other.greenBits && blueBits == other.blueBits &&
        alphaBits == other.alphaBits && underline == other.underline;
    }
  };

  struct Key {
    std::vector<RunKey> runs;
    uint32_t wrapBits = 0;
    uint32_t lineHeightBits = 0;
    uint32_t tabSize = 0;
    uint8_t align = 0;
    uint8_t orientation = 0;
    uint8_t direction = 0;
    int maxLines = 0;
    bool ellipsis = false;
    PolicyKey policy;

    bool operator==(const Key& other) const noexcept {
      return wrapBits == other.wrapBits && lineHeightBits == other.lineHeightBits &&
        tabSize == other.tabSize &&
        align == other.align && orientation == other.orientation &&
        direction == other.direction &&
        maxLines == other.maxLines && ellipsis == other.ellipsis &&
        runs == other.runs && policy == other.policy;
    }
  };

  struct Hash {
    size_t operator()(const Key& key) const noexcept {
      size_t hash = std::hash<uint32_t>{}(key.wrapBits);
      hash ^= std::hash<uint32_t>{}(key.lineHeightBits) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<uint32_t>{}(key.tabSize) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<uint8_t>{}(key.align) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<uint8_t>{}(key.orientation) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<uint8_t>{}(key.direction) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<int>{}(key.maxLines) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<bool>{}(key.ellipsis) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= hashPolicyKey(key.policy) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      for (const RunKey& run: key.runs) {
        hash ^= std::hash<const void*>{}(run.typeface) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(run.sizeBits) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
        hash ^= std::hash<const void*>{}(run.fallback) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
        hash ^= std::hash<std::string_view>{}(run.text) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(run.redBits) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(run.greenBits) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(run.blueBits) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(run.alphaBits) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
        hash ^= std::hash<bool>{}(run.underline) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      }
      return hash;
    }
  };

  using LruList = std::list<Key>;
  using LruIterator = LruList::iterator;

  struct Entry {
    std::vector<std::shared_ptr<const graphics::Typeface>> typefaces;
    std::vector<std::shared_ptr<const graphics::FontFallback>> fallbacks;
    std::shared_ptr<const BreakDictionary> dictionary;
    RichTextLayout layout;
    LruIterator lru;
  };

  static uint32_t floatBits(float value) noexcept {
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
  }

  Key makeKey(const std::vector<RichTextRun>& runs,
    const RichTextLayoutOptions& options, bool ownText) const;

  std::mutex mMutex;
  std::unordered_map<Key, Entry, Hash> mEntries;
  LruList mLru; // front = most recently used
  static constexpr size_t kMaxEntries = 512;
};

} // namespace coral::ui::detail

#endif // CORAL_UI_RICHTEXTLAYOUTCACHE_H
