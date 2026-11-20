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

#include "RichTextLayoutCache.h"

#include <utility>

namespace coral::ui::detail {

RichTextLayoutCache& RichTextLayoutCache::instance() {
  static RichTextLayoutCache cache;
  return cache;
}

RichTextLayoutCache::Key RichTextLayoutCache::makeKey(const std::vector<RichTextRun>& runs,
    const RichTextLayoutOptions& options, const bool ownText) const {
  Key key;
  key.wrapBits = floatBits(options.maxWidth > 0.0f ? options.maxWidth : 0.0f);
  key.lineHeightBits = floatBits(options.lineHeight > 0.0f ? options.lineHeight : 0.0f);
  key.tabSize = options.tabSize;
  key.align = static_cast<uint8_t>(options.align);
  key.orientation = static_cast<uint8_t>(options.orientation);
  key.direction = static_cast<uint8_t>(options.direction);
  key.maxLines = options.maxLines;
  key.ellipsis = options.ellipsis;
  key.policy = makePolicyKey(options.policy);
  key.runs.reserve(runs.size());
  for (const RichTextRun& run: runs) {
    RunKey entry;
    if (run.font != nullptr) {
      entry.typeface = run.font->typeface().get();
      entry.sizeBits = floatBits(run.font->size());
    }
    entry.text = run.text;
    if (ownText) {
      entry.textOwner = std::make_shared<const std::string>(run.text);
      entry.text = *entry.textOwner;
    }
    entry.fallback = run.fallback.get();
    entry.redBits = floatBits(run.color.red);
    entry.greenBits = floatBits(run.color.green);
    entry.blueBits = floatBits(run.color.blue);
    entry.alphaBits = floatBits(run.color.alpha);
    entry.underline = run.underline;
    key.runs.push_back(std::move(entry));
  }
  return key;
}

std::optional<RichTextLayout> RichTextLayoutCache::find(const std::vector<RichTextRun>& runs,
    const RichTextLayoutOptions& options) {
  Key key = makeKey(runs, options, false);
  std::lock_guard<std::mutex> lock(mMutex);
  const auto found = mEntries.find(key);
  if (found == mEntries.end()) {
    return std::nullopt;
  }
  mLru.splice(mLru.begin(), mLru, found->second.lru);
  return found->second.layout;
}

void RichTextLayoutCache::store(const std::vector<RichTextRun>& runs,
    const RichTextLayoutOptions& options, RichTextLayout layout) {
  Key key = makeKey(runs, options, true);
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
  std::vector<std::shared_ptr<const graphics::Typeface>> typefaces;
  std::vector<std::shared_ptr<const graphics::FontFallback>> fallbacks;
  typefaces.reserve(runs.size());
  fallbacks.reserve(runs.size());
  for (const RichTextRun& run: runs) {
    typefaces.push_back(run.font != nullptr ? run.font->typeface() : nullptr);
    fallbacks.push_back(run.fallback);
  }
  mLru.push_front(key);
  mEntries.emplace(std::move(key),
    Entry{ std::move(typefaces), std::move(fallbacks), options.policy.dictionary,
      std::move(layout), mLru.begin() });
}

} // namespace coral::ui::detail
