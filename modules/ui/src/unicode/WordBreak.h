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

#ifndef CORAL_UI_UNICODE_WORDBREAK_H
#define CORAL_UI_UNICODE_WORDBREAK_H

#include "unicode/WordBreakProperties.generated.h"

#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace coral::ui::unicode {

/**
 * Computes UAX #29 word boundaries for @p utf8.
 *
 * Returns ascending UTF-8 byte offsets including 0 and `utf8.size()`. WB1-WB999
 * are implemented, including Hebrew quotes, numeric middles, Katakana,
 * ExtendNumLet, regional-indicator pairs and ZWJ × Extended_Pictographic.
 */
[[nodiscard]] std::vector<size_t> computeWordBreaks(std::string_view utf8);

/** @return true when @p byteOffset is a word boundary. */
[[nodiscard]] bool isWordBoundary(std::string_view utf8, size_t byteOffset);

/** @return the word segment [start, end) containing @p byteOffset. */
[[nodiscard]] std::pair<size_t, size_t> wordRangeAt(std::string_view utf8, size_t byteOffset);

} // namespace coral::ui::unicode

#endif // CORAL_UI_UNICODE_WORDBREAK_H
