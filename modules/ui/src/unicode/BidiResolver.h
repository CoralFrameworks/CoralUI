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

#ifndef CORAL_UI_UNICODE_BIDIRESOLVER_H
#define CORAL_UI_UNICODE_BIDIRESOLVER_H

#include "unicode/BidiProperties.generated.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coral::ui::unicode {

/** 0xFF marks a character removed by X9 (BidiTest/BidiCharacterTest print it as 'x'). */
inline constexpr uint8_t kBidiRemovedLevel = 0xFF;

struct BidiParagraphResult {
  uint8_t baseLevel = 0;
  /** One resolved level per input code point, or kBidiRemovedLevel after X9. */
  std::vector<uint8_t> levels;
  /** Visual order of the non-removed input indices (L2). */
  std::vector<size_t> visualOrder;
};

/**
 * Resolves one paragraph with the Unicode Bidirectional Algorithm.
 *
 * @param paragraphDirection -1 for P2/P3 (auto), 0 for forced LTR, 1 for forced RTL.
 *
 * The implementation covers P2/P3, X1-X10, W1-W7, N0-N2, I1-I2 and L1-L2. L3/L4
 * (mirroring and shaping) are a separate rendering concern. Paragraph splitting
 * at B (P1) is the caller's responsibility.
 */
[[nodiscard]] BidiParagraphResult resolveBidiParagraph(const std::vector<uint32_t>& codePoints, int paragraphDirection);

} // namespace coral::ui::unicode

#endif // CORAL_UI_UNICODE_BIDIRESOLVER_H
