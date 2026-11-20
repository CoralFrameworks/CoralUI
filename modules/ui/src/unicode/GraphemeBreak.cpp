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

#include "unicode/GraphemeBreak.h"

#include "unicode/GraphemeBreakProperties.generated.h"

#include <algorithm>
#include <array>

namespace coral::ui::unicode {

namespace {

constexpr size_t kMaxByteLength = 4;

size_t decodeAt(const std::string_view utf8, const size_t offset, uint32_t& codePoint) {
  const auto lead = static_cast<unsigned char>(utf8[offset]);
  if (lead < 0x80u) {
    codePoint = lead;
    return 1;
  }
  const size_t available = utf8.size() - offset;
  const auto continuation = [](const unsigned char value) { return (value & 0xC0u) == 0x80u; };
  if (lead >= 0xC2u && lead <= 0xDFu && available >= 2) {
    const auto second = static_cast<unsigned char>(utf8[offset + 1]);
    if (continuation(second)) {
      codePoint = (static_cast<uint32_t>(lead & 0x1Fu) << 6) | static_cast<uint32_t>(second & 0x3Fu);
      return 2;
    }
  } else if (lead >= 0xE0u && lead <= 0xEFu && available >= 3) {
    const auto second = static_cast<unsigned char>(utf8[offset + 1]);
    const auto third = static_cast<unsigned char>(utf8[offset + 2]);
    if (continuation(second) && continuation(third)) {
      const uint32_t value = (static_cast<uint32_t>(lead & 0x0Fu) << 12) |
                             (static_cast<uint32_t>(second & 0x3Fu) << 6) |
                             static_cast<uint32_t>(third & 0x3Fu);
      if (value >= 0x800u && (value < 0xD800u || value > 0xDFFFu)) {
        codePoint = value;
        return 3;
      }
    }
  } else if (lead >= 0xF0u && lead <= 0xF4u && available >= 4) {
    const auto second = static_cast<unsigned char>(utf8[offset + 1]);
    const auto third = static_cast<unsigned char>(utf8[offset + 2]);
    const auto fourth = static_cast<unsigned char>(utf8[offset + 3]);
    if (continuation(second) && continuation(third) && continuation(fourth)) {
      const uint32_t value = (static_cast<uint32_t>(lead & 0x07u) << 18) |
                             (static_cast<uint32_t>(second & 0x3Fu) << 12) |
                             (static_cast<uint32_t>(third & 0x3Fu) << 6) |
                             static_cast<uint32_t>(fourth & 0x3Fu);
      if (value >= 0x10000u && value <= 0x10FFFFu) {
        codePoint = value;
        return 4;
      }
    }
  }
  codePoint = 0xFFFDu;
  return 1;
}

bool isControl(const GraphemeBreakClass value) {
  return value == GraphemeBreakClass::Control || value == GraphemeBreakClass::CR ||
         value == GraphemeBreakClass::LF;
}

bool isHangulSyllable(const GraphemeBreakClass value) {
  return value == GraphemeBreakClass::L || value == GraphemeBreakClass::V ||
         value == GraphemeBreakClass::T || value == GraphemeBreakClass::LV ||
         value == GraphemeBreakClass::LVT;
}

enum class ExtendedPictographicState : uint8_t { None, Base, Zwj };

enum class InCBState : uint8_t { None, Linker };

} // namespace

bool GraphemeBreakState::isBoundaryBefore(const uint32_t codePoint) noexcept {
  struct CachedProperties {
    uint32_t codePoint = 0xFFFFFFFFu;
    uint8_t value = 0;
    uint8_t inCB = 0;
    uint8_t extendedPictographic = 0;
  };
  thread_local std::array<CachedProperties, 2048> propertyCache{};
  CachedProperties& cached = propertyCache[(codePoint * 2654435761u) & 2047u];
  if (cached.codePoint != codePoint) {
    cached.codePoint = codePoint;
    cached.value = static_cast<uint8_t>(graphemeBreakClass(codePoint));
    cached.extendedPictographic = codePoint >= 128 && isExtendedPictographic(codePoint) ? 1 : 0;
    cached.inCB = codePoint < 128 ? static_cast<uint8_t>(GraphemeInCB::None)
                                  : static_cast<uint8_t>(graphemeInCB(codePoint));
  }
  const GraphemeBreakClass current = static_cast<GraphemeBreakClass>(cached.value);
  const bool extendedPictographic = cached.extendedPictographic != 0;
  const GraphemeInCB incb = static_cast<GraphemeInCB>(cached.inCB);
  const auto epState = static_cast<ExtendedPictographicState>(extendedPictographicState_);
  const auto incbState = static_cast<InCBState>(inCBState_);

  bool boundary = false;
  if (!first_) {
    // GB3 / GB4 / GB5.
    if (previous_ == GraphemeBreakClass::CR && current == GraphemeBreakClass::LF) {
      boundary = false;
    } else if (isControl(previous_) || isControl(current)) {
      boundary = true;
    // GB6 / GB7 / GB8.
    } else if (previous_ == GraphemeBreakClass::L &&
               (current == GraphemeBreakClass::L || current == GraphemeBreakClass::V ||
                 current == GraphemeBreakClass::LV || current == GraphemeBreakClass::LVT)) {
      boundary = false;
    } else if ((previous_ == GraphemeBreakClass::LV || previous_ == GraphemeBreakClass::V) &&
               (current == GraphemeBreakClass::V || current == GraphemeBreakClass::T)) {
      boundary = false;
    } else if ((previous_ == GraphemeBreakClass::LVT || previous_ == GraphemeBreakClass::T) &&
               current == GraphemeBreakClass::T) {
      boundary = false;
    // GB9 / GB9a / GB9b.
    } else if (current == GraphemeBreakClass::Extend || current == GraphemeBreakClass::ZWJ ||
               current == GraphemeBreakClass::SpacingMark ||
               previous_ == GraphemeBreakClass::Prepend) {
      boundary = false;
    // GB9c (Unicode 18): [InCB=Extend|Linker]* Linker [InCB=Extend|Linker]* x Consonant.
    } else if (incb == GraphemeInCB::Consonant && incbState == InCBState::Linker) {
      boundary = false;
    // GB11: ExtPict Extend* ZWJ x ExtPict.
    } else if (extendedPictographic && epState == ExtendedPictographicState::Zwj) {
      boundary = false;
    // GB12 / GB13.
    } else if (current == GraphemeBreakClass::Regional_Indicator &&
               previous_ == GraphemeBreakClass::Regional_Indicator &&
               regionalIndicators_ % 2 == 1) {
      boundary = false;
    } else {
      boundary = true;
    }
  }

  // Update the lookbehind state for the next boundary.
  if (extendedPictographic) {
    extendedPictographicState_ = static_cast<uint8_t>(ExtendedPictographicState::Base);
  } else if (current == GraphemeBreakClass::Extend &&
             epState == ExtendedPictographicState::Base) {
    extendedPictographicState_ = static_cast<uint8_t>(ExtendedPictographicState::Base);
  } else if (current == GraphemeBreakClass::ZWJ &&
             epState == ExtendedPictographicState::Base) {
    extendedPictographicState_ = static_cast<uint8_t>(ExtendedPictographicState::Zwj);
  } else {
    extendedPictographicState_ = static_cast<uint8_t>(ExtendedPictographicState::None);
  }

  switch (incb) {
    case GraphemeInCB::Linker:
      inCBState_ = static_cast<uint8_t>(InCBState::Linker);
      break;
    case GraphemeInCB::Extend:
      break;
    case GraphemeInCB::Consonant:
    case GraphemeInCB::None:
    case GraphemeInCB::Count:
      inCBState_ = static_cast<uint8_t>(InCBState::None);
      break;
  }

  if (current == GraphemeBreakClass::Regional_Indicator) {
    ++regionalIndicators_;
  } else {
    regionalIndicators_ = 0;
  }

  previous_ = current;
  first_ = false;
  return boundary;
}

std::vector<size_t> computeGraphemeBreaks(const std::string_view utf8) {
  std::vector<size_t> boundaries;
  boundaries.push_back(0);
  if (utf8.empty()) return boundaries;

  GraphemeBreakState state;
  size_t offset = 0;
  while (offset < utf8.size()) {
    uint32_t codePoint = 0;
    const size_t length = decodeAt(utf8, offset, codePoint);
    if (state.isBoundaryBefore(codePoint)) boundaries.push_back(offset);
    offset += length;
  }
  boundaries.push_back(utf8.size());
  return boundaries;
}

bool isGraphemeBoundary(const std::string_view utf8, const size_t byteOffset) {
  if (byteOffset > utf8.size()) return false;
  const std::vector<size_t> boundaries = computeGraphemeBreaks(utf8);
  return std::binary_search(boundaries.begin(), boundaries.end(), byteOffset);
}

size_t previousGraphemeBoundary(const std::string_view utf8, const size_t byteOffset) {
  if (byteOffset == 0 || utf8.empty()) return 0;
  const size_t target = std::min(byteOffset, utf8.size());
  GraphemeBreakState state;
  size_t previous = 0;
  size_t last = 0;
  size_t offset = 0;
  while (offset < target) {
    uint32_t codePoint = 0;
    const size_t length = decodeAt(utf8, offset, codePoint);
    if (state.isBoundaryBefore(codePoint)) {
      previous = last;
      last = offset;
    }
    offset += length;
  }
  return last == target ? previous : last;
}

size_t nextGraphemeBoundary(const std::string_view utf8, const size_t byteOffset) {
  if (byteOffset >= utf8.size()) return utf8.size();
  GraphemeBreakState state;
  size_t offset = 0;
  while (offset < utf8.size()) {
    uint32_t codePoint = 0;
    const size_t length = decodeAt(utf8, offset, codePoint);
    const bool boundary = state.isBoundaryBefore(codePoint);
    if (boundary && offset > byteOffset) return offset;
    offset += length;
  }
  return utf8.size();
}

} // namespace coral::ui::unicode
