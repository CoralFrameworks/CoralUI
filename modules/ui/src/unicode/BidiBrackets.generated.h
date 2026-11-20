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
 * Generated from Unicode 18.0.0 BidiBrackets.txt.
 * Source: https://www.unicode.org/Public/18.0.0/ucd/BidiBrackets.txt
 * DO NOT EDIT BY HAND; see tools/unicode/generate_bidi_brackets.py.
 */

#ifndef CORAL_UI_UNICODE_BIDIBRACKETS_GENERATED_H
#define CORAL_UI_UNICODE_BIDIBRACKETS_GENERATED_H

#include <cstddef>
#include <cstdint>

namespace coral::ui::unicode {

struct BidiBracket {
  uint32_t codePoint;
  uint32_t pair;
  bool open;
};

inline constexpr BidiBracket kBidiBrackets[130] = {
  { 0x0028u, 0x0029u, true },
  { 0x0029u, 0x0028u, false },
  { 0x005Bu, 0x005Du, true },
  { 0x005Du, 0x005Bu, false },
  { 0x007Bu, 0x007Du, true },
  { 0x007Du, 0x007Bu, false },
  { 0x0F3Au, 0x0F3Bu, true },
  { 0x0F3Bu, 0x0F3Au, false },
  { 0x0F3Cu, 0x0F3Du, true },
  { 0x0F3Du, 0x0F3Cu, false },
  { 0x169Bu, 0x169Cu, true },
  { 0x169Cu, 0x169Bu, false },
  { 0x2045u, 0x2046u, true },
  { 0x2046u, 0x2045u, false },
  { 0x207Du, 0x207Eu, true },
  { 0x207Eu, 0x207Du, false },
  { 0x208Du, 0x208Eu, true },
  { 0x208Eu, 0x208Du, false },
  { 0x2308u, 0x2309u, true },
  { 0x2309u, 0x2308u, false },
  { 0x230Au, 0x230Bu, true },
  { 0x230Bu, 0x230Au, false },
  { 0x2329u, 0x232Au, true },
  { 0x232Au, 0x2329u, false },
  { 0x2768u, 0x2769u, true },
  { 0x2769u, 0x2768u, false },
  { 0x276Au, 0x276Bu, true },
  { 0x276Bu, 0x276Au, false },
  { 0x276Cu, 0x276Du, true },
  { 0x276Du, 0x276Cu, false },
  { 0x276Eu, 0x276Fu, true },
  { 0x276Fu, 0x276Eu, false },
  { 0x2770u, 0x2771u, true },
  { 0x2771u, 0x2770u, false },
  { 0x2772u, 0x2773u, true },
  { 0x2773u, 0x2772u, false },
  { 0x2774u, 0x2775u, true },
  { 0x2775u, 0x2774u, false },
  { 0x27C5u, 0x27C6u, true },
  { 0x27C6u, 0x27C5u, false },
  { 0x27E6u, 0x27E7u, true },
  { 0x27E7u, 0x27E6u, false },
  { 0x27E8u, 0x27E9u, true },
  { 0x27E9u, 0x27E8u, false },
  { 0x27EAu, 0x27EBu, true },
  { 0x27EBu, 0x27EAu, false },
  { 0x27ECu, 0x27EDu, true },
  { 0x27EDu, 0x27ECu, false },
  { 0x27EEu, 0x27EFu, true },
  { 0x27EFu, 0x27EEu, false },
  { 0x2983u, 0x2984u, true },
  { 0x2984u, 0x2983u, false },
  { 0x2985u, 0x2986u, true },
  { 0x2986u, 0x2985u, false },
  { 0x2987u, 0x2988u, true },
  { 0x2988u, 0x2987u, false },
  { 0x2989u, 0x298Au, true },
  { 0x298Au, 0x2989u, false },
  { 0x298Bu, 0x298Cu, true },
  { 0x298Cu, 0x298Bu, false },
  { 0x298Du, 0x2990u, true },
  { 0x298Eu, 0x298Fu, false },
  { 0x298Fu, 0x298Eu, true },
  { 0x2990u, 0x298Du, false },
  { 0x2991u, 0x2992u, true },
  { 0x2992u, 0x2991u, false },
  { 0x2993u, 0x2994u, true },
  { 0x2994u, 0x2993u, false },
  { 0x2995u, 0x2996u, true },
  { 0x2996u, 0x2995u, false },
  { 0x2997u, 0x2998u, true },
  { 0x2998u, 0x2997u, false },
  { 0x29D8u, 0x29D9u, true },
  { 0x29D9u, 0x29D8u, false },
  { 0x29DAu, 0x29DBu, true },
  { 0x29DBu, 0x29DAu, false },
  { 0x29FCu, 0x29FDu, true },
  { 0x29FDu, 0x29FCu, false },
  { 0x2E22u, 0x2E23u, true },
  { 0x2E23u, 0x2E22u, false },
  { 0x2E24u, 0x2E25u, true },
  { 0x2E25u, 0x2E24u, false },
  { 0x2E26u, 0x2E27u, true },
  { 0x2E27u, 0x2E26u, false },
  { 0x2E28u, 0x2E29u, true },
  { 0x2E29u, 0x2E28u, false },
  { 0x2E55u, 0x2E56u, true },
  { 0x2E56u, 0x2E55u, false },
  { 0x2E57u, 0x2E58u, true },
  { 0x2E58u, 0x2E57u, false },
  { 0x2E59u, 0x2E5Au, true },
  { 0x2E5Au, 0x2E59u, false },
  { 0x2E5Bu, 0x2E5Cu, true },
  { 0x2E5Cu, 0x2E5Bu, false },
  { 0x2E62u, 0x2E63u, true },
  { 0x2E63u, 0x2E62u, false },
  { 0x3008u, 0x3009u, true },
  { 0x3009u, 0x3008u, false },
  { 0x300Au, 0x300Bu, true },
  { 0x300Bu, 0x300Au, false },
  { 0x300Cu, 0x300Du, true },
  { 0x300Du, 0x300Cu, false },
  { 0x300Eu, 0x300Fu, true },
  { 0x300Fu, 0x300Eu, false },
  { 0x3010u, 0x3011u, true },
  { 0x3011u, 0x3010u, false },
  { 0x3014u, 0x3015u, true },
  { 0x3015u, 0x3014u, false },
  { 0x3016u, 0x3017u, true },
  { 0x3017u, 0x3016u, false },
  { 0x3018u, 0x3019u, true },
  { 0x3019u, 0x3018u, false },
  { 0x301Au, 0x301Bu, true },
  { 0x301Bu, 0x301Au, false },
  { 0xFE59u, 0xFE5Au, true },
  { 0xFE5Au, 0xFE59u, false },
  { 0xFE5Bu, 0xFE5Cu, true },
  { 0xFE5Cu, 0xFE5Bu, false },
  { 0xFE5Du, 0xFE5Eu, true },
  { 0xFE5Eu, 0xFE5Du, false },
  { 0xFF08u, 0xFF09u, true },
  { 0xFF09u, 0xFF08u, false },
  { 0xFF3Bu, 0xFF3Du, true },
  { 0xFF3Du, 0xFF3Bu, false },
  { 0xFF5Bu, 0xFF5Du, true },
  { 0xFF5Du, 0xFF5Bu, false },
  { 0xFF5Fu, 0xFF60u, true },
  { 0xFF60u, 0xFF5Fu, false },
  { 0xFF62u, 0xFF63u, true },
  { 0xFF63u, 0xFF62u, false },
};

inline constexpr size_t kBidiBracketCount =
  sizeof(kBidiBrackets) / sizeof(kBidiBrackets[0]);

/** @return the bracket record for @p codePoint, or null. */
[[nodiscard]] inline const BidiBracket* bidiBracket(const uint32_t codePoint) noexcept {
  size_t first = 0;
  size_t last = kBidiBracketCount;
  while (first < last) {
    const size_t middle = first + (last - first) / 2;
    if (kBidiBrackets[middle].codePoint < codePoint) {
      first = middle + 1;
    } else {
      last = middle;
    }
  }
  if (first >= kBidiBracketCount || kBidiBrackets[first].codePoint != codePoint) {
    return nullptr;
  }
  return &kBidiBrackets[first];
}

} // namespace coral::ui::unicode

#endif // CORAL_UI_UNICODE_BIDIBRACKETS_GENERATED_H
