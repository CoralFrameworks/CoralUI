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

#include "unicode/Hyphenation.h"

#include "unicode/HyphenationPatterns.generated.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace coral::ui::unicode {

namespace {

constexpr uint16_t kNoWeight = 0xFFFF;
/** Longest word we hyphenate; longer runs are not natural-language words. */
constexpr size_t kMaxWordLength = 64;

bool isAsciiLetter(const char value) {
  return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

char asciiLower(const char value) {
  return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

/**
 * Folds common Latin-1 letters to their ASCII base (é→e, ñ→n, ü→u, ...) so the
 * ASCII Liang patterns can hyphenate accented words. Ligatures (ß, æ, œ, þ) are
 * rejected because they would need a 1:many offset mapping.
 */
bool foldLatinLetter(const uint32_t codePoint, char& folded) {
  if (codePoint < 0x80u) {
    if (!isAsciiLetter(static_cast<char>(codePoint))) return false;
    folded = asciiLower(static_cast<char>(codePoint));
    return true;
  }
  // Greek monoton/polytonic letters are folded to the same ASCII label set as
  // the generated table (final/lunate sigma share s; tonos is dropped).
  uint32_t greek = codePoint;
  if (greek >= 0x0391u && greek <= 0x03A9u) greek += 32u;
  switch (greek) {
    case 0x03ACu: greek = 0x03B1u; break;
    case 0x03ADu: greek = 0x03B5u; break;
    case 0x03AEu: greek = 0x03B7u; break;
    case 0x03AFu: greek = 0x03B9u; break;
    case 0x03CCu: greek = 0x03BFu; break;
    case 0x03CDu: greek = 0x03C5u; break;
    case 0x03CEu: greek = 0x03C9u; break;
    case 0x03CAu: case 0x0390u: greek = 0x03B9u; break;
    case 0x03CBu: case 0x03B0u: greek = 0x03C5u; break;
    case 0x03C2u: case 0x03F2u: greek = 0x03C3u; break;
    case 0x1F71u: greek = 0x03B1u; break;
    case 0x1F73u: greek = 0x03B5u; break;
    case 0x1F75u: greek = 0x03B7u; break;
    case 0x1F77u: greek = 0x03B9u; break;
    case 0x1F79u: greek = 0x03BFu; break;
    case 0x1F7Bu: greek = 0x03C5u; break;
    case 0x1F7Du: greek = 0x03C9u; break;
    default: break;
  }
  if (greek >= 0x03B1u && greek <= 0x03C9u) {
    static constexpr char kGreekSymbols[] = "abgdezhqiklmnxoprsstufcyw";
    folded = kGreekSymbols[greek - 0x03B1u];
    return true;
  }
  // Cyrillic is transliterated one-to-one to ASCII labels so the ASCII DAFSA
  // and one-letter/one-byte offset mapping still apply.
  uint32_t cyrillic = codePoint;
  if (cyrillic == 0x0401u) cyrillic = 0x0451u;
  else if (cyrillic >= 0x0410u && cyrillic <= 0x042Fu) cyrillic += 32u;
  if (cyrillic == 0x0451u || (cyrillic >= 0x0430u && cyrillic <= 0x044Fu)) {
    static constexpr char kSymbols[] = "abvgde;jziyklmnoprstufhcwx,/*+=?A";
    size_t index = cyrillic == 0x0451u ? 6u : cyrillic - 0x0430u;
    if (index >= 6u) ++index;
    folded = kSymbols[index];
    return true;
  }
  // Ligatures are approximated by one base letter so the byte-offset mapping
  // stays 1:1 (ß→s, æ→a, œ→o, þ→t).
  switch (codePoint) {
    case 0x00DFu: folded = 's'; return true;
    case 0x00C6u: folded = 'a'; return true;
    case 0x00E6u: folded = 'a'; return true;
    case 0x00DEu: folded = 't'; return true;
    case 0x00FEu: folded = 't'; return true;
    case 0x0152u: folded = 'o'; return true;
    case 0x0153u: folded = 'o'; return true;
    default: break;
  }
  if (codePoint < 0x00C0u || codePoint > 0x00FFu) return false;
  static constexpr char kMap[64] = {
    'A', 'A', 'A', 'A', 'A', 'A', 0, 'C', 'E', 'E', 'E', 'E', 'I', 'I', 'I', 'I',
    'D', 'N', 'O', 'O', 'O', 'O', 'O', 0, 'O', 'U', 'U', 'U', 'U', 'Y', 0, 0,
    'a', 'a', 'a', 'a', 'a', 'a', 0, 'c', 'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i',
    'd', 'n', 'o', 'o', 'o', 'o', 'o', 0, 'o', 'u', 'u', 'u', 'u', 'y', 0, 'y',
  };
  const char base = kMap[codePoint - 0x00C0u];
  if (base == 0) return false;
  folded = asciiLower(base);
  return true;
}

size_t decodeUtf8(const std::string_view text, const size_t offset, uint32_t& codePoint) {
  const auto lead = static_cast<unsigned char>(text[offset]);
  if (lead < 0x80u) {
    codePoint = lead;
    return 1;
  }
  const size_t available = text.size() - offset;
  const auto continuation = [](const unsigned char value) { return (value & 0xC0u) == 0x80u; };
  if (lead >= 0xC2u && lead <= 0xDFu && available >= 2 &&
      continuation(static_cast<unsigned char>(text[offset + 1]))) {
    codePoint = (static_cast<uint32_t>(lead & 0x1Fu) << 6) |
                static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 1]) & 0x3Fu);
    return 2;
  }
  if (lead >= 0xE0u && lead <= 0xEFu && available >= 3 &&
      continuation(static_cast<unsigned char>(text[offset + 1])) &&
      continuation(static_cast<unsigned char>(text[offset + 2]))) {
    codePoint = (static_cast<uint32_t>(lead & 0x0Fu) << 12) |
                (static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 1]) & 0x3Fu) << 6) |
                static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 2]) & 0x3Fu);
    return 3;
  }
  if (lead >= 0xF0u && lead <= 0xF4u && available >= 4 &&
      continuation(static_cast<unsigned char>(text[offset + 1])) &&
      continuation(static_cast<unsigned char>(text[offset + 2])) &&
      continuation(static_cast<unsigned char>(text[offset + 3]))) {
    codePoint = (static_cast<uint32_t>(lead & 0x07u) << 18) |
                (static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 1]) & 0x3Fu) << 12) |
                (static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 2]) & 0x3Fu) << 6) |
                static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 3]) & 0x3Fu);
    return 4;
  }
  codePoint = 0xFFFDu;
  return 1;
}

bool languageForTag(const std::string_view language, HyphenationLanguage& result) {
  if (language.size() < 2) return false;
  const char first = static_cast<char>(asciiLower(language[0]));
  const char second = static_cast<char>(asciiLower(language[1]));
  if (language.size() > 2 && language[2] != '-' && language[2] != '_') return false;
  if (first == 'e' && second == 'n') {
    result = HyphenationLanguage::English;
    return true;
  }
  if (first == 'i' && second == 't') {
    result = HyphenationLanguage::Italian;
    return true;
  }
  if (first == 'e' && second == 's') {
    result = HyphenationLanguage::Spanish;
    return true;
  }
  if (first == 'd' && second == 'e') {
    result = HyphenationLanguage::German;
    return true;
  }
  if (first == 'f' && second == 'r') {
    result = HyphenationLanguage::French;
    return true;
  }
  if (first == 'p' && second == 't') {
    result = HyphenationLanguage::Portuguese;
    return true;
  }
  if (first == 'r' && second == 'u') {
    result = HyphenationLanguage::Russian;
    return true;
  }
  if (first == 'e' && second == 'l') {
    result = HyphenationLanguage::Greek;
    return true;
  }
  if (first == 'n' && second == 'l') {
    result = HyphenationLanguage::Dutch;
    return true;
  }
  if (first == 's' && second == 'v') {
    result = HyphenationLanguage::Swedish;
    return true;
  }
  if (first == 'd' && second == 'a') {
    result = HyphenationLanguage::Danish;
    return true;
  }
  if (first == 'f' && second == 'i') {
    result = HyphenationLanguage::Finnish;
    return true;
  }
  if (first == 'c' && second == 'a') {
    result = HyphenationLanguage::Catalan;
    return true;
  }
  return false;
}

uint16_t findEdge(const HyphenationTable& table, const uint16_t node, const uint8_t label) {
  const uint16_t start = table.nodeEdgeStart[node];
  const uint8_t count = table.nodeEdgeCount[node];
  for (uint8_t index = 0; index < count; ++index) {
    const uint16_t edge = static_cast<uint16_t>(start + index);
    const uint8_t value = table.edgeLabel[edge];
    if (value == label) return table.edgeChild[edge];
    if (value > label) break;
  }
  return kNoWeight;
}

uint8_t weightAt(const HyphenationTable& table, const uint16_t weightId, const size_t index) {
  const size_t nibble = static_cast<size_t>(table.weightOffset[weightId]) + index;
  const uint8_t byte = table.weightBits[nibble >> 1];
  return (nibble & 1u) != 0 ? static_cast<uint8_t>(byte >> 4) : static_cast<uint8_t>(byte & 0x0F);
}

void applyPattern(const HyphenationTable& table, const uint16_t weightId, const size_t start,
    uint8_t* weights, const size_t weightCount) {
  const uint8_t length = table.weightLength[weightId];
  for (uint8_t index = 0; index < length; ++index) {
    const size_t position = start + index;
    if (position >= weightCount) break;
    const uint8_t value = weightAt(table, weightId, index);
    if (value > weights[position]) weights[position] = value;
  }
}

bool hyphenateFolded(const HyphenationTable& table, const std::string_view word,
    std::vector<size_t>& positions, const HyphenationLimits& limits) {
  const size_t length = word.size();
  if (length > kMaxWordLength) return false;
  const size_t left = std::max<size_t>(table.leftMin, limits.minBefore);
  const size_t right = std::max<size_t>(table.rightMin, limits.minAfter);
  if (length < std::max<size_t>(limits.minWordLength, left + right)) return true;
  if (table.exceptionCount > 0) {
    size_t low = 0;
    size_t high = table.exceptionCount;
    while (low < high) {
      const size_t middle = low + (high - low) / 2;
      const int order = word.compare(table.exceptions[middle].word);
      if (order == 0) {
        const HyphenationException& exception = table.exceptions[middle];
        for (uint8_t index = 0; index < exception.count; ++index) {
          const size_t position = exception.positions[index];
          if (position >= left && length - position >= right) positions.push_back(position);
        }
        return true;
      }
      if (order < 0) {
        high = middle;
      } else {
        low = middle + 1;
      }
    }
  }
  std::array<char, kMaxWordLength + 2> padded{};
  padded[0] = '.';
  for (size_t index = 0; index < length; ++index) padded[index + 1] = word[index];
  padded[length + 1] = '.';
  const size_t paddedLength = length + 2;

  std::array<uint8_t, kMaxWordLength + 3> weights{};
  for (size_t start = 0; start < paddedLength; ++start) {
    uint16_t node = table.root;
    for (size_t index = start; index < paddedLength; ++index) {
      node = findEdge(table, node, static_cast<uint8_t>(padded[index]));
      if (node == kNoWeight) break;
      const uint16_t weightId = table.nodeWeight[node];
      if (weightId != kNoWeight) {
        applyPattern(table, weightId, start, weights.data(), weights.size());
      }
    }
  }
  for (size_t position = 1; position < paddedLength - 1; ++position) {
    const size_t offset = position - 1;
    if ((weights[position] & 1u) == 0) continue;
    if (offset < left) continue;
    if (length - offset < right) continue;
    positions.push_back(offset);
  }
  return true;
}

} // namespace anonymous

bool hasHyphenation(const std::string_view language) noexcept {
  HyphenationLanguage resolved = HyphenationLanguage::English;
  return languageForTag(language, resolved);
}

bool isHyphenationLetter(const uint32_t codePoint) noexcept {
  char folded = 0;
  return foldLatinLetter(codePoint, folded);
}

bool hyphenateWord(const std::string_view word, const std::string_view language,
    std::vector<size_t>& positions, const HyphenationLimits& limits) {
  positions.clear();
  HyphenationLanguage resolved = HyphenationLanguage::English;
  if (!languageForTag(language, resolved)) return false;
  if (word.size() > kMaxWordLength) return false;

  std::array<char, kMaxWordLength + 1> folded{};
  std::array<size_t, kMaxWordLength + 1> byteOffsets{};
  size_t letters = 0;
  size_t offset = 0;
  while (offset < word.size()) {
    uint32_t codePoint = 0;
    const size_t length = decodeUtf8(word, offset, codePoint);
    char base = 0;
    if (!foldLatinLetter(codePoint, base)) return false;
    if (letters >= kMaxWordLength) return false;
    folded[letters] = base;
    byteOffsets[letters] = offset;
    ++letters;
    offset += length;
  }

  std::vector<size_t> letterBreaks;
  if (!hyphenateFolded(hyphenationTable(resolved),
        std::string_view(folded.data(), letters), letterBreaks, limits)) {
    return false;
  }
  positions.reserve(letterBreaks.size());
  for (const size_t letter: letterBreaks) {
    if (letter < letters) positions.push_back(byteOffsets[letter]);
  }
  return true;
}

} // namespace coral::ui::unicode
