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

#ifndef CORAL_UI_TEXTBIDI_H
#define CORAL_UI_TEXTBIDI_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace coral::ui {

enum class TextDirection : uint8_t {
  Ltr,
  Rtl,
};

struct BidiRun {
  size_t start = 0;
  size_t end = 0;
  TextDirection direction = TextDirection::Ltr;
};

/** Resolved levels and logical-to-visual order for one paragraph. */
struct BidiParagraph {
  uint8_t baseLevel = 0;
  /** Byte offset of each code point in the input UTF-8 string. */
  std::vector<size_t> byteOffsets;
  /** One level per code point; 0xFF marks a character removed by UAX #9 X9. */
  std::vector<uint8_t> levels;
  /** Visual order as indices into the input code point sequence. */
  std::vector<size_t> visualOrder;
};

/** CSS `unicode-bidi` modes applied to a byte range. */
enum class UnicodeBidi : uint8_t {
  Normal,
  Embed,
  Isolate,
  BidiOverride,
  IsolateOverride,
  Plaintext,
};

struct BidiSpan {
  size_t start = 0;
  size_t end = 0;
  UnicodeBidi mode = UnicodeBidi::Normal;
  TextDirection direction = TextDirection::Ltr;
};

/**
 * Applies CSS unicode-bidi spans on top of resolved levels. Spans use UTF-8
 * byte ranges, may nest, and are applied outer-to-inner. `Embed`/`Isolate` push
 * a level of the requested parity; `BidiOverride` forces the parity at the
 * current level; `IsolateOverride` does both.
 */
void applyBidiSpans(BidiParagraph& paragraph, std::span<const BidiSpan> spans);

/**
 * Resolves a paragraph with CSS unicode-bidi spans applied as explicit UBA
 * controls (LRE/RLE/LRO/RLO/LRI/RLI/PDF/PDI). `plaintext` spans choose their
 * own base direction from their first strong character and are isolated.
 * This gives the strict isolate boundary behaviour that post-level overrides
 * cannot express.
 */
[[nodiscard]] BidiParagraph resolveBidiParagraphWithSpans(std::string_view utf8,
    std::optional<TextDirection> direction, std::span<const BidiSpan> spans);

/** The paragraph direction selected from the first strong character. */
[[nodiscard]] TextDirection baseDirection(std::string_view text);

/**
 * @return true when @p text contains a right-to-left or explicit formatting
 * character, i.e. when laying it out without the UBA would be wrong.
 */
[[nodiscard]] bool requiresBidi(std::string_view text);

/**
 * Resolves one paragraph with the full Unicode Bidirectional Algorithm
 * (P2/P3, X1-X10, W1-W7, N0-N2, I1-I2, L1-L2).
 *
 * @param direction nullopt selects the paragraph direction with P2/P3.
 */
[[nodiscard]] BidiParagraph resolveBidiParagraph(std::string_view utf8,
        std::optional<TextDirection> direction = std::nullopt);

/** Shared cache entry for a resolved paragraph; avoids copying levels on every layout. */
[[nodiscard]] std::shared_ptr<const BidiParagraph> resolveBidiParagraphShared(
        std::string_view utf8, std::optional<TextDirection> direction = std::nullopt);

/** Same as above, retaining @p owner / @p offset as the cache key instead of copying text. */
[[nodiscard]] std::shared_ptr<const BidiParagraph> resolveBidiParagraphShared(
        std::string_view utf8, std::optional<TextDirection> direction,
        std::shared_ptr<const std::string> owner, size_t offset);

/** @return @p utf8 with each character replaced by its Bidi_Mirroring_Glyph, if any (L4). */
[[nodiscard]] std::string mirrorBidiText(std::string_view utf8);

/** Cached variant used on draw/hit paths; the returned string is shared and immutable. */
[[nodiscard]] std::shared_ptr<const std::string> mirrorBidiTextShared(std::string_view utf8);

/**
 * Splits into strong-direction runs. This is the base-level segmentation the editor needs for Arabic/Hebrew
 * runs; it is not the full Unicode Bidirectional Algorithm (no explicit embeddings or bracket resolution yet).
 */
[[nodiscard]] std::vector<BidiRun> bidiRuns(std::string_view text);

/** Returns run indices in visual order: logical for LTR paragraphs, reversed for RTL paragraphs. */
[[nodiscard]] std::vector<size_t> visualRunOrder(const std::vector<BidiRun>& runs, TextDirection base);

} // namespace coral::ui

#endif // CORAL_UI_TEXTBIDI_H
