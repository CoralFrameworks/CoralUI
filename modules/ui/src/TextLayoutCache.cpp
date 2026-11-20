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

#include "TextLayoutCache.h"

#include <utility>

namespace coral::ui::detail {

TextLayoutCache& TextLayoutCache::instance() {
  static TextLayoutCache cache;
  return cache;
}

std::optional<TextLayout> TextLayoutCache::find(const graphics::Font& font,
    const std::string_view text, const TextLayoutOptions& options) {
  Key key;
  key.typeface = font.typeface().get();
  key.sizeBits = floatBits(font.size());
  key.wrapBits = floatBits(options.maxWidth > 0.0f ? options.maxWidth : 0.0f);
  key.lineHeightBits = floatBits(options.lineHeight > 0.0f ? options.lineHeight : 0.0f);
  key.tabSize = options.tabSize;
  key.align = static_cast<uint8_t>(options.align);
  key.orientation = static_cast<uint8_t>(options.orientation);
  key.direction = static_cast<uint8_t>(options.direction);
  key.policy = makePolicyKey(options.policy);
  key.text.assign(text);
  std::lock_guard<std::mutex> lock(mMutex);
  const auto found = mEntries.find(key);
  if (found == mEntries.end()) {
    return std::nullopt;
  }
  // Promote the entry to most-recently-used. `splice` moves the list node without allocating.
  mLru.splice(mLru.begin(), mLru, found->second.lru);
  return found->second.layout;
}

void TextLayoutCache::store(const graphics::Font& font, const std::string_view text,
    const TextLayoutOptions& options, TextLayout layout) {
  Key key;
  key.typeface = font.typeface().get();
  key.sizeBits = floatBits(font.size());
  key.wrapBits = floatBits(options.maxWidth > 0.0f ? options.maxWidth : 0.0f);
  key.lineHeightBits = floatBits(options.lineHeight > 0.0f ? options.lineHeight : 0.0f);
  key.tabSize = options.tabSize;
  key.align = static_cast<uint8_t>(options.align);
  key.orientation = static_cast<uint8_t>(options.orientation);
  key.direction = static_cast<uint8_t>(options.direction);
  key.policy = makePolicyKey(options.policy);
  key.text.assign(text);
  std::lock_guard<std::mutex> lock(mMutex);
  if (const auto found = mEntries.find(key); found != mEntries.end()) {
    found->second.layout = std::move(layout);
    mLru.splice(mLru.begin(), mLru, found->second.lru);
    return;
  }
  if (mEntries.size() >= kMaxEntries) {
    const Key& victim = mLru.back();
    mEntries.erase(victim);
    mLru.pop_back();
  }
  mLru.push_front(key);
  mEntries.emplace(std::move(key),
    Entry{ font.typeface(), options.policy.dictionary, std::move(layout), mLru.begin() });
}

} // namespace coral::ui::detail
