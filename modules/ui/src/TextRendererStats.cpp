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

#include <ui/TextRendererStats.h>

#include "TextPainter.h"

#include <mutex>

namespace coral::ui {

namespace {

std::mutex& statsMutex() {
  static std::mutex mutex;
  return mutex;
}

TextRendererStats& statsStorage() {
  static TextRendererStats stats;
  return stats;
}

} // namespace

TextRendererStats textRendererStats() {
  TextRendererStats stats;
  {
    const std::lock_guard lock(statsMutex());
    stats = statsStorage();
  }
  // The cache owns its own lock and its own counters; query it after releasing the stats lock so the
  // two never nest (a run build takes the stats lock while the cache is already unlocked).
  const TextPainter::RunCacheSnapshot cache = TextPainter::runCacheSnapshot();
  stats.runCacheEntries = cache.entries;
  stats.runCacheBytes = cache.bytes;
  stats.runCacheEvictions = cache.evictions;
  stats.runCacheBudgetBytes = TextPainter::runCacheBudgetBytes();
  return stats;
}

void resetTextRendererStats() {
  const std::lock_guard lock(statsMutex());
  statsStorage() = TextRendererStats{};
}

void setTextRunCacheBudgetBytes(const size_t bytes) {
  TextPainter::setRunCacheBudgetBytes(bytes);
}

size_t textRunCacheBudgetBytes() {
  return TextPainter::runCacheBudgetBytes();
}

void clearTextRunCache() {
  TextPainter::clearRunCache();
}

namespace detail {

void recordRunBuild(const size_t bytes) {
  const std::lock_guard lock(statsMutex());
  TextRendererStats& stats = statsStorage();
  ++stats.runBuilds;
  stats.bitmapBytes += bytes;
}

void recordRunCacheHit() {
  const std::lock_guard lock(statsMutex());
  ++statsStorage().runCacheHits;
}

void recordRunDrawCall() {
  const std::lock_guard lock(statsMutex());
  ++statsStorage().runDrawCalls;
}

void recordAtlasGlyph() {
  const std::lock_guard lock(statsMutex());
  ++statsStorage().atlasGlyphs;
}

} // namespace detail

} // namespace coral::ui
