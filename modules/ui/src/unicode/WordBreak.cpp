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

#include "unicode/WordBreak.h"

#include "unicode/GraphemeBreakProperties.generated.h"

#include <algorithm>

namespace coral::ui::unicode {

namespace {

struct Item {
  uint32_t codePoint = 0;
  size_t byteOffset = 0;
  WordBreakClass cls = WordBreakClass::Other;
  bool extendedPictographic = false;
};

size_t decodeAt(const std::string_view utf8, const size_t offset, uint32_t& codePoint) {
  const auto lead = static_cast<unsigned char>(utf8[offset]);
  if (lead < 0x80u) {
    codePoint = lead;
    return 1;
  }
  const size_t available = utf8.size() - offset;
  const auto continuation = [](const unsigned char value) { return (value & 0xC0u) == 0x80u; };
  if (lead >= 0xC2u && lead <= 0xDFu && available >= 2 &&
      continuation(static_cast<unsigned char>(utf8[offset + 1]))) {
    codePoint = (static_cast<uint32_t>(lead & 0x1Fu) << 6) |
                static_cast<uint32_t>(static_cast<unsigned char>(utf8[offset + 1]) & 0x3Fu);
    return 2;
  }
  if (lead >= 0xE0u && lead <= 0xEFu && available >= 3 &&
      continuation(static_cast<unsigned char>(utf8[offset + 1])) &&
      continuation(static_cast<unsigned char>(utf8[offset + 2]))) {
    const uint32_t value = (static_cast<uint32_t>(lead & 0x0Fu) << 12) |
                           (static_cast<uint32_t>(static_cast<unsigned char>(utf8[offset + 1]) & 0x3Fu) << 6) |
                           static_cast<uint32_t>(static_cast<unsigned char>(utf8[offset + 2]) & 0x3Fu);
    if (value >= 0x800u && (value < 0xD800u || value > 0xDFFFu)) {
      codePoint = value;
      return 3;
    }
  }
  if (lead >= 0xF0u && lead <= 0xF4u && available >= 4 &&
      continuation(static_cast<unsigned char>(utf8[offset + 1])) &&
      continuation(static_cast<unsigned char>(utf8[offset + 2])) &&
      continuation(static_cast<unsigned char>(utf8[offset + 3]))) {
    const uint32_t value = (static_cast<uint32_t>(lead & 0x07u) << 18) |
                           (static_cast<uint32_t>(static_cast<unsigned char>(utf8[offset + 1]) & 0x3Fu) << 12) |
                           (static_cast<uint32_t>(static_cast<unsigned char>(utf8[offset + 2]) & 0x3Fu) << 6) |
                           static_cast<uint32_t>(static_cast<unsigned char>(utf8[offset + 3]) & 0x3Fu);
    if (value >= 0x10000u && value <= 0x10FFFFu) {
      codePoint = value;
      return 4;
    }
  }
  codePoint = 0xFFFDu;
  return 1;
}

bool isIgnored(const WordBreakClass value) {
  return value == WordBreakClass::Extend || value == WordBreakClass::Format ||
         value == WordBreakClass::ZWJ;
}

bool isNewline(const WordBreakClass value) {
  return value == WordBreakClass::Newline || value == WordBreakClass::CR ||
         value == WordBreakClass::LF;
}

bool isAhLetter(const WordBreakClass value) {
  return value == WordBreakClass::ALetter || value == WordBreakClass::Hebrew_Letter;
}

bool isMidLetter(const WordBreakClass value) {
  return value == WordBreakClass::MidLetter || value == WordBreakClass::MidNumLet ||
         value == WordBreakClass::Single_Quote;
}

bool isMidNumber(const WordBreakClass value) {
  return value == WordBreakClass::MidNum || value == WordBreakClass::MidNumLet ||
         value == WordBreakClass::Single_Quote;
}

/** Index of the previous non-ignored item at or before @p index. */
int previousBase(const std::vector<Item>& items, const size_t index) {
  for (size_t position = index + 1; position-- > 0;) {
    if (!isIgnored(items[position].cls)) return static_cast<int>(position);
  }
  return -1;
}

int nextBase(const std::vector<Item>& items, const size_t index) {
  for (size_t position = index; position < items.size(); ++position) {
    if (!isIgnored(items[position].cls)) return static_cast<int>(position);
  }
  return -1;
}

bool isBoundary(const std::vector<Item>& items, const size_t index) {
  if (index == 0 || index >= items.size()) return true;
  const Item& left = items[index - 1];
  const Item& right = items[index];
  // WB3
  if (left.cls == WordBreakClass::CR && right.cls == WordBreakClass::LF) return false;
  // WB3a/WB3b
  if (isNewline(left.cls) || isNewline(right.cls)) return true;
  // WB3c
  if (left.cls == WordBreakClass::ZWJ && right.extendedPictographic) return false;
  // WB3d
  if (left.cls == WordBreakClass::WSegSpace && right.cls == WordBreakClass::WSegSpace) {
    return false;
  }
  // WB4: X (Extend | Format | ZWJ)* is kept attached.
  if (isIgnored(right.cls)) return false;

  const int leftIndex = previousBase(items, index - 1);
  if (leftIndex < 0) return true;
  const int rightIndex = static_cast<int>(index);
  const WordBreakClass leftClass = items[static_cast<size_t>(leftIndex)].cls;
  const WordBreakClass rightClass = right.cls;
  const int beforeLeft = leftIndex > 0 ? previousBase(items, static_cast<size_t>(leftIndex - 1)) : -1;
  const int afterRight = nextBase(items, index + 1);
  const auto classAt = [&](const int position) {
    return position >= 0 ? items[static_cast<size_t>(position)].cls : WordBreakClass::Other;
  };
  const WordBreakClass beforeClass = classAt(beforeLeft);
  const WordBreakClass afterClass = classAt(afterRight);
  (void) rightIndex;

  // WB5
  if (isAhLetter(leftClass) && isAhLetter(rightClass)) return false;
  // WB6 / WB7
  if (isAhLetter(leftClass) && isMidLetter(rightClass) && isAhLetter(afterClass)) return false;
  if (isMidLetter(leftClass) && isAhLetter(beforeClass) && isAhLetter(rightClass)) return false;
  // WB7a / WB7b / WB7c
  if (leftClass == WordBreakClass::Hebrew_Letter && rightClass == WordBreakClass::Single_Quote) {
    return false;
  }
  if (leftClass == WordBreakClass::Hebrew_Letter && rightClass == WordBreakClass::Double_Quote &&
      afterClass == WordBreakClass::Hebrew_Letter) {
    return false;
  }
  if (leftClass == WordBreakClass::Double_Quote && beforeClass == WordBreakClass::Hebrew_Letter &&
      rightClass == WordBreakClass::Hebrew_Letter) {
    return false;
  }
  // WB8 / WB9 / WB10
  if (leftClass == WordBreakClass::Numeric && rightClass == WordBreakClass::Numeric) return false;
  if (isAhLetter(leftClass) && rightClass == WordBreakClass::Numeric) return false;
  if (leftClass == WordBreakClass::Numeric && isAhLetter(rightClass)) return false;
  // WB11 / WB12
  if (isMidNumber(leftClass) && beforeClass == WordBreakClass::Numeric &&
      rightClass == WordBreakClass::Numeric) {
    return false;
  }
  if (leftClass == WordBreakClass::Numeric && isMidNumber(rightClass) &&
      afterClass == WordBreakClass::Numeric) {
    return false;
  }
  // WB13
  if (leftClass == WordBreakClass::Katakana && rightClass == WordBreakClass::Katakana) return false;
  // WB13a / WB13b
  if ((isAhLetter(leftClass) || leftClass == WordBreakClass::Numeric ||
       leftClass == WordBreakClass::Katakana || leftClass == WordBreakClass::ExtendNumLet) &&
      rightClass == WordBreakClass::ExtendNumLet) {
    return false;
  }
  if (leftClass == WordBreakClass::ExtendNumLet &&
      (isAhLetter(rightClass) || rightClass == WordBreakClass::Numeric ||
       rightClass == WordBreakClass::Katakana)) {
    return false;
  }
  // WB15 / WB16: for a boundary between two regional indicators, keep the pair
  // together when the run ending at `leftIndex` has odd length.
  if (leftClass == WordBreakClass::Regional_Indicator &&
      rightClass == WordBreakClass::Regional_Indicator) {
    size_t count = 0;
    int position = leftIndex;
    while (position >= 0) {
      if (items[static_cast<size_t>(position)].cls != WordBreakClass::Regional_Indicator) break;
      ++count;
      position = position > 0
        ? previousBase(items, static_cast<size_t>(position - 1))
        : -1;
    }
    if (count % 2 == 1) return false;
  }
  // WB999
  return true;
}

} // namespace

std::vector<size_t> computeWordBreaks(const std::string_view utf8) {
  std::vector<Item> items;
  items.reserve(utf8.size());
  size_t offset = 0;
  while (offset < utf8.size()) {
    uint32_t codePoint = 0;
    const size_t length = decodeAt(utf8, offset, codePoint);
    Item item;
    item.codePoint = codePoint;
    item.byteOffset = offset;
    item.cls = wordBreakClass(codePoint);
    item.extendedPictographic = isExtendedPictographic(codePoint);
    items.push_back(item);
    offset += length;
  }
  std::vector<size_t> boundaries;
  boundaries.reserve(items.size() + 1);
  boundaries.push_back(0);
  for (size_t index = 1; index < items.size(); ++index) {
    if (isBoundary(items, index)) boundaries.push_back(items[index].byteOffset);
  }
  boundaries.push_back(utf8.size());
  return boundaries;
}

bool isWordBoundary(const std::string_view utf8, const size_t byteOffset) {
  if (byteOffset > utf8.size()) return false;
  if (byteOffset == 0 || byteOffset == utf8.size()) return true;
  const std::vector<size_t> boundaries = computeWordBreaks(utf8);
  return std::binary_search(boundaries.begin(), boundaries.end(), byteOffset);
}

std::pair<size_t, size_t> wordRangeAt(const std::string_view utf8, const size_t byteOffset) {
  const std::vector<size_t> boundaries = computeWordBreaks(utf8);
  if (boundaries.empty()) return { 0, 0 };
  const size_t offset = std::min(byteOffset, utf8.size());
  const auto next = std::upper_bound(boundaries.begin(), boundaries.end(), offset);
  const size_t end = next == boundaries.end() ? utf8.size() : *next;
  const size_t start = next == boundaries.begin() ? 0 : *(next - 1);
  return { start, end };
}

} // namespace coral::ui::unicode
