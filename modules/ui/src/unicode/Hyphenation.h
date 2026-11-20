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

#ifndef CORAL_UI_UNICODE_HYPHENATION_H
#define CORAL_UI_UNICODE_HYPHENATION_H

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace coral::ui::unicode {

/**
 * CSS `hyphenate-limit-chars` overrides. Zero means "use the language default";
 * non-zero values raise the effective minimum.
 */
struct HyphenationLimits {
  uint8_t minWordLength = 0;
  uint8_t minBefore = 0;
  uint8_t minAfter = 0;
};

/**
 * @return true when a built-in Liang pattern table exists for @p language
 * (English "en", Italian "it", Spanish "es"...; regional suffixes are accepted).
 */
[[nodiscard]] bool hasHyphenation(std::string_view language) noexcept;

/**
 * @return true when @p codePoint can participate in a hyphenatable word:
 * ASCII letters and Latin-1 letters that fold to an ASCII base (é, ñ, ü, ...).
 */
[[nodiscard]] bool isHyphenationLetter(uint32_t codePoint) noexcept;

/**
 * Hyphenates one ASCII word with the language's Liang patterns.
 *
 * Appends break positions to @p positions: byte offsets in @p word in
 * `1..word.size()-1`, each with at least the language's left/right hyphen minima
 * on both sides. The function is a no-op for words shorter than the minima,
 * words containing non `[A-Za-z]` characters, or unknown languages.
 *
 * @return true when a table was consulted (even if it produced no position).
 */
bool hyphenateWord(std::string_view word, std::string_view language,
  std::vector<size_t>& positions, const HyphenationLimits& limits = {});

} // namespace coral::ui::unicode

#endif // CORAL_UI_UNICODE_HYPHENATION_H
