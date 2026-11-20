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

#ifndef CORAL_UI_POLICYCACHEKEY_H
#define CORAL_UI_POLICYCACHEKEY_H

#include <ui/LineBreakPolicy.h>

#include <cstdint>
#include <string>
#include <utility>

namespace coral::ui::detail {

/** The cache-relevant part of a LineBreakPolicy: flags, language and dictionary identity. */
struct PolicyKey {
  uint32_t bits = 0;
  std::string language;
  const BreakDictionary* dictionary = nullptr;
  uint8_t hyphenationMinWordLength = 0;
  uint8_t hyphenationMinBefore = 0;
  uint8_t hyphenationMinAfter = 0;
  float hyphenateLimitZone = 0.0f;
  uint8_t hyphenateLimitLines = 0;

  bool operator==(const PolicyKey& other) const {
    return bits == other.bits && dictionary == other.dictionary && language == other.language &&
      hyphenationMinWordLength == other.hyphenationMinWordLength &&
      hyphenationMinBefore == other.hyphenationMinBefore &&
      hyphenationMinAfter == other.hyphenationMinAfter &&
      hyphenateLimitZone == other.hyphenateLimitZone &&
      hyphenateLimitLines == other.hyphenateLimitLines;
  }
};

inline uint32_t encodePolicyBits(const LineBreakPolicy& policy) {
  return static_cast<uint32_t>(policy.strictness) |
         (static_cast<uint32_t>(policy.wordBreak) << 2) |
         (static_cast<uint32_t>(policy.overflowWrap) << 4) |
         (policy.hyphenation ? (1u << 6) : 0u);
}

inline PolicyKey makePolicyKey(const LineBreakPolicy& policy) {
  PolicyKey key;
  key.bits = encodePolicyBits(policy);
  key.language = policy.language;
  key.dictionary = policy.dictionary.get();
  key.hyphenationMinWordLength = policy.hyphenationMinWordLength;
  key.hyphenationMinBefore = policy.hyphenationMinBefore;
  key.hyphenationMinAfter = policy.hyphenationMinAfter;
  key.hyphenateLimitZone = policy.hyphenateLimitZone;
  key.hyphenateLimitLines = policy.hyphenateLimitLines;
  return key;
}

inline size_t hashPolicyKey(const PolicyKey& key) noexcept {
  size_t hash = std::hash<uint32_t>{}(key.bits);
  hash ^= std::hash<std::string>{}(key.language) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
  hash ^= std::hash<const void*>{}(key.dictionary) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
  hash ^= std::hash<uint8_t>{}(key.hyphenationMinWordLength) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
  hash ^= std::hash<uint8_t>{}(key.hyphenationMinBefore) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
  hash ^= std::hash<uint8_t>{}(key.hyphenationMinAfter) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
  hash ^= std::hash<float>{}(key.hyphenateLimitZone) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
  hash ^= std::hash<uint8_t>{}(key.hyphenateLimitLines) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
  return hash;
}

} // namespace coral::ui::detail

#endif // CORAL_UI_POLICYCACHEKEY_H
