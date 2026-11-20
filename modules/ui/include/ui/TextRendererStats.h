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

#ifndef CORAL_UI_TEXTRENDERERSTATS_H
#define CORAL_UI_TEXTRENDERERSTATS_H

#include <cstddef>

namespace coral::ui {

/** Counters for the shared text-run bitmap atlas; useful for regression and benchmarks. */
struct TextRendererStats {
  size_t runBuilds = 0;
  size_t runCacheHits = 0;
  size_t runDrawCalls = 0;
  size_t atlasGlyphs = 0;
  size_t bitmapBytes = 0;
  /** Resident entries in the shared run cache, and the bytes they hold, at the time of the query. */
  size_t runCacheEntries = 0;
  size_t runCacheBytes = 0;
  /** Runs evicted by the cache's LRU policy since the process started. */
  size_t runCacheEvictions = 0;
  /** The byte budget the cache evicts against. */
  size_t runCacheBudgetBytes = 0;
};

[[nodiscard]] TextRendererStats textRendererStats();
void resetTextRendererStats();

/**
 * Sets the shared run cache's byte budget; 0 disables caching (every draw rebuilds its line).
 *
 * A run is a whole-line bitmap, so the budget is bytes rather than entries: a couple of thousand short
 * labels and a handful of long lines are very different amounts of memory.
 */
void setTextRunCacheBudgetBytes(size_t bytes);
[[nodiscard]] size_t textRunCacheBudgetBytes();
/** Drops every cached run; the next draw rebuilds it. Used by tests that need a cold cache. */
void clearTextRunCache();

namespace detail {
void recordRunBuild(size_t bytes);
void recordRunCacheHit();
void recordRunDrawCall();
void recordAtlasGlyph();
}

} // namespace coral::ui

#endif // CORAL_UI_TEXTRENDERERSTATS_H
