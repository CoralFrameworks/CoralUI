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

#ifndef CORAL_UI_TEXTSHAPECACHE_H
#define CORAL_UI_TEXTSHAPECACHE_H

#include <graphics/Font.h>
#include <graphics/TextBlob.h>

#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace coral::ui::detail {

/**
 * Process-wide cache of shaped runs and their measured widths.
 *
 * Wrapping asks for many small substrings (one per codepoint in the simple path, one per run segment
 * in rich text). Shaping each of them is the expensive part of layout, and the same words recur across
 * lines, layers and windows. A key of typeface + size + direction + text lets all of those share one
 * `TextBlob`; the blob is immutable, so sharing it is safe. The cache is bounded and evicts the
 * least-recently-used run when full, so hot words survive while one-off strings churn.
 */
class TextShapeCache {
  public:
  static TextShapeCache& instance();

  /** @return the layout width of @p text at @p font's size. */
  [[nodiscard]] float measure(const graphics::Font& font, std::string_view text,
          graphics::Font::Direction direction = graphics::Font::Direction::Auto);

  /** @return a shared, immutable shaped run for drawing; never null. */
  [[nodiscard]] std::shared_ptr<const graphics::TextBlob> shape(const graphics::Font& font, std::string_view text,
          graphics::Font::Direction direction = graphics::Font::Direction::Auto);

  private:
  TextShapeCache() = default;

  struct Key {
    const graphics::Typeface* typeface = nullptr;
    uint32_t sizeBits = 0;
    graphics::Font::Direction direction = graphics::Font::Direction::Auto;
    std::string text;

    bool operator==(const Key& other) const noexcept {
      return typeface == other.typeface && sizeBits == other.sizeBits && direction == other.direction &&
             text == other.text;
    }
  };

  struct Hash {
    size_t operator()(const Key& key) const noexcept {
      size_t hash = std::hash<const void*>{}(key.typeface);
      hash ^= std::hash<uint32_t>{}(key.sizeBits) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(key.direction)) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      hash ^= std::hash<std::string>{}(key.text) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
      return hash;
    }
  };

  using LruList = std::list<Key>;
  using LruIterator = LruList::iterator;

  struct Entry {
    std::shared_ptr<const graphics::Typeface> typeface;
    std::shared_ptr<const graphics::TextBlob> blob;
    float width = 0.0f;
    LruIterator lru;
  };

  Entry& findOrCreate(const graphics::Font& font, std::string_view text, graphics::Font::Direction direction);

  std::mutex mMutex;
  std::unordered_map<Key, Entry, Hash> mEntries;
  LruList mLru; // front = most recently used
  static constexpr size_t kMaxEntries = 4096;
};

} // namespace coral::ui::detail

#endif // CORAL_UI_TEXTSHAPECACHE_H
