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

#ifndef CORAL_UI_SYSTEMFONTS_H
#define CORAL_UI_SYSTEMFONTS_H

#include <ui/Types.h>

#include <memory>
#include <string>
#include <vector>

namespace graphics {
class Font;
} // namespace graphics

namespace coral::ui {

/**
 * @return curated system font files (CJK, emoji and broad-coverage faces),
 * ordered by usefulness. Discovery scans the platform font directories once.
 *
 * The list is intentionally curated rather than "every installed face": loading
 * hundreds of faces at startup costs more than it helps a lightweight UI.
 */
[[nodiscard]] std::vector<std::string> systemFontFiles();

/**
 * @return a cached fallback face set for @p primary: the primary plus up to 12
 * discovered system faces at the primary's pixel size. `nullptr` when discovery
 * found nothing (the caller should keep using the primary alone).
 *
 * The returned object is shared between callers with the same typeface and size;
 * `FontFallback::shape`/`measure` pick the first face that covers a code point.
 */
[[nodiscard]] std::shared_ptr<const FontFallback> systemFontFallback(
  std::shared_ptr<const Font> primary);

/**
 * Test seam: replaces the discovered file list and clears both the file and
 * fallback caches. An empty list restores real discovery on next use.
 */
void setSystemFontFilesForTesting(std::vector<std::string> files);

} // namespace coral::ui

#endif // CORAL_UI_SYSTEMFONTS_H
