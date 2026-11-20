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

#ifndef CORAL_UI_UNICODE_GRAPHEMEBREAK_H
#define CORAL_UI_UNICODE_GRAPHEMEBREAK_H

#include "unicode/GraphemeBreakProperties.generated.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace coral::ui::unicode {

/** Streaming UAX #29 state; call isBoundaryBefore() once per code point. */
class GraphemeBreakState {
 public:
  /** Consumes @p codePoint and returns whether a cluster boundary precedes it. */
  bool isBoundaryBefore(uint32_t codePoint) noexcept;

 private:
  GraphemeBreakClass previous_ = GraphemeBreakClass::Other;
  uint8_t extendedPictographicState_ = 0; // 0 none, 1 base, 2 zwj
  uint8_t inCBState_ = 0;                 // 0 none, 1 linker
  int regionalIndicators_ = 0;
  bool first_ = true;
};

/**
 * Computes UAX #29 extended grapheme-cluster boundaries for @p utf8.
 *
 * Returns ascending UTF-8 byte offsets, including 0 and `utf8.size()`.
 */
[[nodiscard]] std::vector<size_t> computeGraphemeBreaks(std::string_view utf8);

/** @return true when @p byteOffset is a grapheme-cluster boundary. */
[[nodiscard]] bool isGraphemeBoundary(std::string_view utf8, size_t byteOffset);

/**
 * @return the last grapheme-cluster boundary strictly before @p byteOffset
 * (0 when there is none). The text is rescanned from the start, so callers
 * should keep editor text small or cache the result.
 */
[[nodiscard]] size_t previousGraphemeBoundary(std::string_view utf8, size_t byteOffset);

/**
 * @return the first grapheme-cluster boundary strictly after @p byteOffset
 * (`utf8.size()` when there is none).
 */
[[nodiscard]] size_t nextGraphemeBoundary(std::string_view utf8, size_t byteOffset);

} // namespace coral::ui::unicode

#endif // CORAL_UI_UNICODE_GRAPHEMEBREAK_H
