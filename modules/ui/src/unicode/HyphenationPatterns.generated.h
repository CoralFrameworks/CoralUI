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

/*
 * Generated from hyph-utf8 pattern files (18.0.0).
 * DO NOT EDIT BY HAND; see tools/unicode/generate_hyphenation.py.
 */

#ifndef CORAL_UI_UNICODE_HYPHENATIONPATTERNS_GENERATED_H
#define CORAL_UI_UNICODE_HYPHENATIONPATTERNS_GENERATED_H

#include <cstddef>
#include <cstdint>

namespace coral::ui::unicode {

/** One TeX \\hyphenation{...} exception; positions are letter indices. */
struct HyphenationException {
  const char* word;
  const uint8_t* positions;
  uint8_t count;
};

/** Minimized Liang pattern DAFSA with nibble-packed weights. */
struct HyphenationTable {
  const uint16_t* nodeEdgeStart;
  const uint8_t* nodeEdgeCount;
  const uint16_t* nodeWeight;   // 0xFFFF when the node is not a pattern terminal
  const uint8_t* edgeLabel;
  const uint16_t* edgeChild;
  const uint16_t* weightOffset; // nibble index into weightBits
  const uint8_t* weightLength;  // number of weights, pattern letters + 1
  const uint8_t* weightBits;
  uint16_t nodeCount;
  uint16_t edgeCount;
  uint16_t weightCount;
  uint16_t root;
  uint8_t leftMin;
  uint8_t rightMin;
  const HyphenationException* exceptions;
  uint16_t exceptionCount;
};

enum class HyphenationLanguage : uint8_t {
  English,
  Italian,
  Spanish,
  German,
  French,
  Portuguese,
  Russian,
  Greek,
  Dutch,
  Swedish,
  Danish,
  Finnish,
  Catalan,
  Count,
};

[[nodiscard]] const HyphenationTable& hyphenationTable(HyphenationLanguage language) noexcept;
[[nodiscard]] const char* hyphenationLanguageTag(HyphenationLanguage language) noexcept;
[[nodiscard]] size_t hyphenationLanguageCount() noexcept;

inline constexpr uint16_t kHyphenationNodeCount = 35765;

} // namespace coral::ui::unicode

#endif // CORAL_UI_UNICODE_HYPHENATIONPATTERNS_GENERATED_H
