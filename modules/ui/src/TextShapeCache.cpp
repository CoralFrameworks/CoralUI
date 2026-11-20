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

#include "TextShapeCache.h"

#include <cstring>

namespace coral::ui::detail {

namespace {

uint32_t floatBits(const float value) {
  uint32_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value));
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

} // namespace anonymous

TextShapeCache& TextShapeCache::instance() {
  static TextShapeCache cache;
  return cache;
}

TextShapeCache::Entry& TextShapeCache::findOrCreate(const graphics::Font& font, const std::string_view text,
        const graphics::Font::Direction direction) {
  Key key;
  key.typeface = font.typeface().get();
  key.sizeBits = floatBits(font.size());
  key.direction = direction;
  key.text.assign(text);
  if (const auto found = mEntries.find(key); found != mEntries.end()) {
    mLru.splice(mLru.begin(), mLru, found->second.lru);
    return found->second;
  }
  if (mEntries.size() >= kMaxEntries) {
    const Key& victim = mLru.back();
    mEntries.erase(victim);
    mLru.pop_back();
  }
  mLru.push_front(key);
  auto [found, inserted] = mEntries.try_emplace(std::move(key));
  (void) inserted;
  Entry& entry = found->second;
  entry.typeface = font.typeface();
  entry.blob = std::make_shared<const graphics::TextBlob>(font.shape(text, direction));
  const float unitsPerEm = static_cast<float>(entry.typeface->unitsPerEm());
  entry.width = unitsPerEm > 0.0f ? entry.blob->advance().x * font.size() / unitsPerEm : 0.0f;
  entry.lru = mLru.begin();
  return entry;
}

float TextShapeCache::measure(const graphics::Font& font, const std::string_view text,
        const graphics::Font::Direction direction) {
  if (text.empty()) return 0.0f;
  std::lock_guard<std::mutex> lock(mMutex);
  return findOrCreate(font, text, direction).width;
}

std::shared_ptr<const graphics::TextBlob> TextShapeCache::shape(const graphics::Font& font, const std::string_view text,
        const graphics::Font::Direction direction) {
  if (text.empty()) {
    return std::make_shared<const graphics::TextBlob>();
  }
  std::lock_guard<std::mutex> lock(mMutex);
  return findOrCreate(font, text, direction).blob;
}

} // namespace coral::ui::detail
