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

#ifndef CORAL_UI_TEXTLayoutCache_H
#define CORAL_UI_TEXTLayoutCache_H

#include <ui/TextLayout.h>

#include "PolicyCacheKey.h"

#include <graphics/Font.h>

#include <cstdint>
#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace coral::ui::detail {

/**
 * Process-wide LRU cache of wrapped layouts.
 *
 * `TextLayout` is pure over (font, text, wrap width), so a label repeated in two layers or two windows
 * does not need to be measured and wrapped twice. The cached value holds its typeface so the key's
 * pointer stays valid. At capacity the least recently used layout is evicted rather than clearing the
 * whole cache, so a long-lived window keeps its hot labels while a one-off string churns through.
 */
class TextLayoutCache {
 public:
  static TextLayoutCache& instance();

  [[nodiscard]] std::optional<TextLayout> find(const graphics::Font& font, std::string_view text,
    const TextLayoutOptions& options);

  void store(const graphics::Font& font, std::string_view text, const TextLayoutOptions& options,
    TextLayout layout);

 private:
  TextLayoutCache() = default;

  struct Key {
    const graphics::Typeface* typeface = nullptr;
    uint32_t sizeBits = 0;
    uint32_t wrapBits = 0;
    uint32_t lineHeightBits = 0;
    uint32_t tabSize = 0;
    uint8_t align = 0;
    uint8_t orientation = 0;
    uint8_t direction = 0;
    PolicyKey policy;
    std::string text;

    bool operator==(const Key& other) const noexcept {
      return typeface == other.typeface && sizeBits == other.sizeBits &&
        wrapBits == other.wrapBits && lineHeightBits == other.lineHeightBits &&
        tabSize == other.tabSize &&
        align == other.align && orientation == other.orientation &&
        direction == other.direction && policy == other.policy && text == other.text;
    }
  };

  struct Hash {
    size_t operator()(const Key& key) const noexcept {
      size_t hash = std::hash<const void*>{}(key.typeface);
      hash ^= std::hash<uint32_t>{}(key.sizeBits) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<uint32_t>{}(key.wrapBits) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<uint32_t>{}(key.lineHeightBits) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<uint32_t>{}(key.tabSize) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<uint8_t>{}(key.align) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<uint8_t>{}(key.orientation) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<uint8_t>{}(key.direction) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= hashPolicyKey(key.policy) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<std::string>{}(key.text) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      return hash;
    }
  };

  using LruList = std::list<Key>;
  using LruIterator = LruList::iterator;

  struct Entry {
    std::shared_ptr<const graphics::Typeface> typeface;
    std::shared_ptr<const BreakDictionary> dictionary;
    TextLayout layout;
    LruIterator lru;
  };

  static uint32_t floatBits(float value) noexcept {
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
  }

  std::mutex mMutex;
  std::unordered_map<Key, Entry, Hash> mEntries;
  LruList mLru; // front = most recently used
  static constexpr size_t kMaxEntries = 1024;
};

} // namespace coral::ui::detail

#endif // CORAL_UI_TEXTLayoutCache_H
