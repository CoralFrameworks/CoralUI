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
 * Generated from Unicode 18.0.0 UAX #14 rules (tools/unicode/uax14_rules.json).
 * DO NOT EDIT BY HAND; see tools/unicode/generate_uax14_dfa.py.
 */

#ifndef CORAL_UI_UNICODE_LINEBREAKDFA_GENERATED_H
#define CORAL_UI_UNICODE_LINEBREAKDFA_GENERATED_H

#include <cstdint>

#include "unicode/LineBreakProperties.generated.h"

namespace coral::ui::unicode {

enum class BreakAction : uint8_t {
  NoBreak = 0,
  AllowBreak = 1,
  MandatoryBreak = 2,
};

inline constexpr uint16_t kLineBreakDfaStateCount = 154;
inline constexpr uint16_t kLineBreakDfaSymbolCount = 101;
inline constexpr uint16_t kLineBreakTokenCodeCount = 125;
inline constexpr uint16_t kLineBreakDfaInitialState = 1;
inline constexpr uint16_t kLineBreakDfaRuleCount = 79;
inline constexpr uint8_t kLineBreakDfaMaskWords = 2;
inline constexpr uint8_t kLineBreakSotTokenCode = 0;
inline constexpr uint8_t kLineBreakEotTokenCode = 255;

/** @return the DFA state after @p symbol from @p state. */
[[nodiscard]] uint16_t lineBreakDfaTransition(uint16_t state, uint8_t symbol) noexcept;

/** @return the canonical DFA symbol for a token code. */
[[nodiscard]] uint8_t lineBreakDfaTokenSymbol(uint8_t tokenCode) noexcept;

/**
 * @return the token code for a resolved class plus context flags, or 255 when the
 * combination is not present in Unicode data (callers should use kLineBreakEotTokenCode).
 */
[[nodiscard]] uint8_t lineBreakDfaTokenCode(LineBreakClass klass, uint8_t flags) noexcept;

/** @return the sorted candidate rule alternatives for a DFA state. */
[[nodiscard]] const uint16_t* lineBreakDfaCandidates(uint16_t state, uint8_t& count) noexcept;

/**
 * @return the highest-priority candidate whose right context is empty or one token,
 * for @p code0 (kLineBreakEotTokenCode is the end-of-text token), or 255.
 */
[[nodiscard]] uint8_t lineBreakDfaBestRule(uint16_t state, uint8_t code0) noexcept;

/** @return the candidates with a right context of two or three tokens. */
[[nodiscard]] const uint16_t* lineBreakDfaMultiCandidates(uint16_t state, uint8_t& count) noexcept;

[[nodiscard]] uint16_t lineBreakDfaRuleRank(uint16_t rule) noexcept;

[[nodiscard]] uint8_t lineBreakDfaRuleAction(uint16_t rule) noexcept;
[[nodiscard]] uint8_t lineBreakDfaRuleRightLength(uint16_t rule) noexcept;
[[nodiscard]] const uint64_t* lineBreakDfaRuleRightMask(uint16_t rule, uint8_t position) noexcept;
[[nodiscard]] const char* lineBreakDfaRuleId(uint16_t rule) noexcept;

} // namespace coral::ui::unicode

#endif // CORAL_UI_UNICODE_LINEBREAKDFA_GENERATED_H
