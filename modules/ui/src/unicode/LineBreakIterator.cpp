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

#include "LineBreakIterator.h"

#include <algorithm>
#include <array>
#include <string>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "LineBreakDfa.generated.inc"

namespace coral::ui::unicode {

namespace {

bool cannotAttachTo(const uint8_t klass) {
  const auto value = static_cast<LineBreakClass>(klass);
  return value == LineBreakClass::BK || value == LineBreakClass::CR || value == LineBreakClass::LF ||
         value == LineBreakClass::NL || value == LineBreakClass::SP || value == LineBreakClass::ZW;
}

uint8_t tokenCode(const LineBreakClass klass, const uint8_t flags) {
  const uint8_t code = lineBreakDfaTokenCode(klass, flags);
  if (code != kLineBreakEotTokenCode) return code;
  // Every resolved class plus flag combination that can occur in Unicode data is
  // in the generated universe, including the trailing-ZWJ variants. The fallback
  // only protects against a corrupted table; treating the token as AL keeps the
  // iterator well defined instead of producing an end-of-text sentinel.
  return lineBreakDfaTokenCode(LineBreakClass::AL, 0);
}

/** Token code for each ASCII byte, initialized once from the generated table. */
const uint8_t* asciiTokenCodes() {
  static const auto table = [] {
    std::array<uint8_t, 128> values{};
    for (size_t index = 0; index < values.size(); ++index) {
      const AsciiLineBreakProperty& property = kAsciiLineBreakProperties[index];
      values[index] = tokenCode(property.value, property.flags);
    }
    return values;
  }();
  return table.data();
}

bool rightContextMatches(const uint16_t rule, const uint8_t* rightCodes) {
  const uint8_t length = lineBreakDfaRuleRightLength(rule);
  for (uint8_t position = 0; position < length; ++position) {
    const uint64_t* mask = lineBreakDfaRuleRightMask(rule, position);
    if (mask == nullptr) return false;
    const uint8_t code = rightCodes[position];
    const uint32_t bit = code == kLineBreakEotTokenCode ? kLineBreakTokenCodeCount : code;
    if ((mask[bit >> 6] & (uint64_t{ 1 } << (bit & 63u))) == 0) return false;
  }
  return true;
}

uint8_t resolveBoundary(const uint16_t candidateState, const uint8_t* rightCodes,
    uint16_t* ruleOut = nullptr) {
  // The generated dispatch table already resolves every zero/one-token right
  // context (most rules); only the handful of long right contexts remain.
  const uint8_t best = lineBreakDfaBestRule(candidateState, rightCodes[0]);
  const uint16_t bestRank = best == 255 ? 0xFFFFu : lineBreakDfaRuleRank(best);
  uint8_t multiCount = 0;
  const uint16_t* multi = lineBreakDfaMultiCandidates(candidateState, multiCount);
  if (multi != nullptr) {
    for (uint8_t index = 0; index < multiCount; ++index) {
      const uint16_t rule = multi[index];
      if (lineBreakDfaRuleRank(rule) > bestRank) break;
      if (rightContextMatches(rule, rightCodes)) {
        if (ruleOut != nullptr) *ruleOut = rule;
        return lineBreakDfaRuleAction(rule);
      }
    }
  }
  if (best != 255) {
    if (ruleOut != nullptr) *ruleOut = best;
    return lineBreakDfaRuleAction(best);
  }
  if (ruleOut != nullptr) *ruleOut = 0xFFFFu;
  return static_cast<uint8_t>(BreakAction::AllowBreak);
}

struct EmittedToken {
  uint8_t code = 0;
  uint32_t codePointStart = 0;
  uint32_t codePointCount = 0;
  size_t byteStart = 0;
};

struct ActionSink {
  std::vector<uint8_t>& actions;

  void token(const uint32_t codePointStart, const uint32_t codePointCount, const size_t, const uint8_t action,
      const uint16_t) const {
    // actions is zero-initialized to NoBreak; only the token head needs a write.
    (void)codePointCount;
    actions[codePointStart] = action;
  }

  void finish(const size_t codePointCount) const {
    actions[codePointCount] = static_cast<uint8_t>(BreakAction::MandatoryBreak);
  }
};

struct ActionCallbackSink {
  LineBreakActionCallback callback = nullptr;
  void* context = nullptr;

  void token(const uint32_t codePointStart, const uint32_t codePointCount, const size_t,
      const uint8_t action, const uint16_t) const {
    callback(context, codePointStart, static_cast<BreakAction>(action));
    for (uint32_t offset = 1; offset < codePointCount; ++offset) {
      callback(context, codePointStart + offset, BreakAction::NoBreak);
    }
  }

  void finish(const size_t codePointCount) const {
    callback(context, codePointCount, BreakAction::MandatoryBreak);
  }
};

struct ActionBlockWriter {
  std::array<uint8_t, 4096> block{};
  size_t used = 0;
  size_t start = 0;
  LineBreakActionBlockCallback callback = nullptr;
  void* context = nullptr;

  void push(const uint8_t action) {
    block[used++] = action;
    if (used == block.size()) flush();
  }

  void flush() {
    if (used == 0) return;
    callback(context, start, block.data(), used);
    start += used;
    used = 0;
  }
};

struct ActionBlockSink {
  ActionBlockWriter& writer;

  void token(const uint32_t codePointStart, const uint32_t codePointCount, const size_t,
      const uint8_t action, const uint16_t) const {
    (void)codePointStart;
    for (uint32_t offset = 0; offset < codePointCount; ++offset) {
      writer.push(offset == 0 ? action : static_cast<uint8_t>(BreakAction::NoBreak));
    }
  }

  void finish(const size_t) const {
    writer.push(static_cast<uint8_t>(BreakAction::MandatoryBreak));
  }
};

struct OpportunitySink {
  std::vector<LineBreakOpportunity>& opportunities;

  void token(const uint32_t, const uint32_t, const size_t byteStart, const uint8_t action,
      const uint16_t) const {
    if (action == static_cast<uint8_t>(BreakAction::NoBreak)) return;
    opportunities.push_back({ byteStart, static_cast<BreakAction>(action) });
  }

  void finish(const size_t) const {}
};

struct CallbackOpportunitySink {
  LineBreakOpportunityCallback callback = nullptr;
  void* context = nullptr;
  size_t count = 0;

  void token(const uint32_t, const uint32_t, const size_t byteStart, const uint8_t action,
      const uint16_t) {
    if (action == static_cast<uint8_t>(BreakAction::NoBreak)) return;
    if (callback != nullptr) callback(context, byteStart, static_cast<BreakAction>(action));
    ++count;
  }

  void finish(const size_t) const {}
};

struct RuleSink {
  std::vector<uint16_t>& rules;

  void token(const uint32_t codePointStart, const uint32_t codePointCount, const size_t,
      const uint8_t, const uint16_t rule) const {
    if (codePointStart >= rules.size()) return;
    rules[codePointStart] = rule;
    for (uint32_t offset = 1; offset < codePointCount; ++offset) {
      if (codePointStart + offset < rules.size()) rules[codePointStart + offset] = 0xFFFFu;
    }
  }

  void finish(const size_t) const {}
};

/**
 * UTF-8 engine with a resumable ASCII run path.
 *
 * The generic per-code-point loop pays for the UTF-8 branch and lambda call at
 * every ASCII byte. Mixed prose is mostly long ASCII runs between CJK/Arabic/
 * emoji spans, so this engine carries the full DFA/token state while scanning
 * an ASCII run with a tight byte loop.
 */
template<typename Sink>
class FastUtf8Engine {
 public:
  explicit FastUtf8Engine(Sink& sink) : mSink(sink) {
    mPending[0] = kLineBreakDfaInitialState;
  }

  void consume(const uint32_t codePoint, const size_t byteOffset) {
    LineBreakClass resolved;
    uint8_t contextFlags = 0;
    if (codePoint < 128) {
      const AsciiLineBreakProperty& property = kAsciiLineBreakProperties[codePoint];
      resolved = property.value;
      contextFlags = property.flags;
    } else {
      CachedProperty& cached = mCache[codePoint & 4095u];
      if (cached.codePoint != codePoint) {
        cached.codePoint = codePoint;
        cached.value = static_cast<uint8_t>(resolveLineBreakClass(codePoint));
        cached.flags = lineBreakFlags(codePoint);
      }
      resolved = static_cast<LineBreakClass>(cached.value);
      contextFlags = cached.flags;
    }
    const bool combining = resolved == LineBreakClass::CM || resolved == LineBreakClass::ZWJ;
    if (combining && mHasToken && !cannotAttachTo(mTokenBase)) {
      ++mTokenCount;
      if (resolved == LineBreakClass::ZWJ) {
        mTokenFlags |= kLineBreakFlagZwjTail;
      } else {
        mTokenFlags &= static_cast<uint8_t>(~kLineBreakFlagZwjTail);
      }
      mTokenCodeValue = tokenCode(static_cast<LineBreakClass>(mTokenBase), mTokenFlags);
      ++mCodePointIndex;
      return;
    }
    const bool coalescibleClass = resolved == LineBreakClass::AL ||
                                  resolved == LineBreakClass::NU ||
                                  resolved == LineBreakClass::SP;
    if (codePoint < 128 && mHasToken && !combining && coalescibleClass &&
        mTokenBase == static_cast<uint8_t>(resolved) && mTokenFlags == contextFlags) {
      ++mTokenCount;
      ++mCodePointIndex;
      return;
    }
    flushToken();
    if (combining) {
      mTokenBase = static_cast<uint8_t>(LineBreakClass::AL);
      mTokenFlags = resolved == LineBreakClass::ZWJ ? kLineBreakFlagZwjTail : 0;
    } else {
      mTokenBase = static_cast<uint8_t>(resolved);
      mTokenFlags = contextFlags;
    }
    mTokenCodeValue = (codePoint < 128 && !combining)
      ? asciiTokenCodes()[codePoint]
      : tokenCode(static_cast<LineBreakClass>(mTokenBase), mTokenFlags);
    mTokenStart = static_cast<uint32_t>(mCodePointIndex);
    mTokenCount = 1;
    mTokenByteStart = byteOffset;
    mHasToken = true;
    ++mCodePointIndex;
  }

  void consumeAsciiRun(const std::string_view text, const size_t start, const size_t end) {
    const uint8_t* asciiCodes = asciiTokenCodes();
    for (size_t index = start; index < end; ++index) {
      const uint8_t codePoint = static_cast<uint8_t>(text[index]);
      const AsciiLineBreakProperty& property = kAsciiLineBreakProperties[codePoint];
      const uint8_t resolved = static_cast<uint8_t>(property.value);
      const bool coalescible = property.value == LineBreakClass::AL ||
                               property.value == LineBreakClass::NU ||
                               property.value == LineBreakClass::SP;
      if (mHasToken && coalescible && mTokenBase == resolved && mTokenFlags == property.flags) {
        ++mTokenCount;
        ++mCodePointIndex;
        continue;
      }
      flushToken();
      mTokenBase = resolved;
      mTokenFlags = property.flags;
      mTokenCodeValue = asciiCodes[codePoint];
      mTokenStart = static_cast<uint32_t>(mCodePointIndex);
      mTokenCount = 1;
      mTokenByteStart = index;
      mHasToken = true;
      ++mCodePointIndex;
    }
  }

  void finish() {
    flushToken();
    while (mPendingCount > 1) {
      uint8_t rightCodes[3] = {
        kLineBreakEotTokenCode,
        kLineBreakEotTokenCode,
        kLineBreakEotTokenCode,
      };
      for (size_t index = 0; index < 3; ++index) {
        if (mFirstBoundary + index < mEmitted) {
          rightCodes[index] = mRing[(mFirstBoundary + index) % 3].code;
        }
      }
      uint16_t rule = 0xFFFFu;
      const uint8_t action = mFirstBoundary == 0
        ? static_cast<uint8_t>(BreakAction::NoBreak)
        : resolveBoundary(mPending[0], rightCodes, &rule);
      writeAction(mFirstBoundary, action, rule);
      mPending[0] = mPending[1];
      mPending[1] = mPending[2];
      --mPendingCount;
      ++mFirstBoundary;
    }
    mSink.finish(mCodePointIndex);
  }

 private:
  struct CachedProperty {
    uint32_t codePoint = ~0u;
    uint8_t value = 0;
    uint8_t flags = 0;
  };

  void writeAction(const size_t boundary, const uint8_t action, const uint16_t rule) {
    const EmittedToken& token = mRing[boundary % 3];
    mSink.token(token.codePointStart, token.codePointCount, token.byteStart, action, rule);
  }

  void emit(const uint8_t code, const uint32_t start, const uint32_t count, const size_t byteStart) {
    mRing[mEmitted % 3] = EmittedToken{ code, start, count, byteStart };
    if (mPendingCount == 3) {
      const uint8_t rightCodes[3] = {
        mRing[(mEmitted - 2) % 3].code,
        mRing[(mEmitted - 1) % 3].code,
        code,
      };
      uint16_t rule = 0xFFFFu;
      const uint8_t action = mFirstBoundary == 0
        ? static_cast<uint8_t>(BreakAction::NoBreak)
        : resolveBoundary(mPending[0], rightCodes, &rule);
      writeAction(mFirstBoundary, action, rule);
      mPending[0] = mPending[1];
      mPending[1] = mPending[2];
      --mPendingCount;
      ++mFirstBoundary;
    }
    mState = lineBreakDfaTransition(mState, lineBreakDfaTokenSymbol(code));
    mPending[mPendingCount++] = mState;
    ++mEmitted;
  }

  void flushToken() {
    if (!mHasToken) return;
    emit(mTokenCodeValue, mTokenStart, mTokenCount, mTokenByteStart);
    mHasToken = false;
  }

  Sink& mSink;
  uint16_t mPending[3] = { kLineBreakDfaInitialState, 0, 0 };
  size_t mPendingCount = 1;
  size_t mFirstBoundary = 0;
  uint16_t mState = kLineBreakDfaInitialState;
  EmittedToken mRing[3];
  size_t mEmitted = 0;
  size_t mCodePointIndex = 0;
  bool mHasToken = false;
  uint8_t mTokenBase = 0;
  uint8_t mTokenFlags = 0;
  uint8_t mTokenCodeValue = 0;
  uint32_t mTokenStart = 0;
  uint32_t mTokenCount = 0;
  size_t mTokenByteStart = 0;
  CachedProperty mCache[4096];
};

/**
 * Normalizes LB9/LB10 and runs the DFA in one pass. Token boundaries are resolved
 * two tokens late because LB25 has three-token right contexts; the ring keeps
 * exactly the three tokens needed for those decisions.
 *
 * `next` returns one code point and its byte offset; the sink receives the
 * boundary actions. This lets the action and opportunity paths share the DFA
 * while the opportunity path decodes UTF-8 without materializing code point or
 * boundary arrays.
 */
template<typename NextCodePoint, typename Sink>
void runEngine(NextCodePoint&& next, Sink&& sink) {
  uint16_t pending[3] = { kLineBreakDfaInitialState, 0, 0 };
  size_t pendingCount = 1;
  size_t firstBoundary = 0;
  uint16_t state = kLineBreakDfaInitialState;
  EmittedToken ring[3];
  size_t emitted = 0;
  size_t codePointIndex = 0;

  const auto writeAction = [&](const size_t boundary, const uint8_t action,
      const uint16_t rule) {
    const EmittedToken& token = ring[boundary % 3];
    sink.token(token.codePointStart, token.codePointCount, token.byteStart, action, rule);
  };

  const auto emit = [&](const uint8_t code, const uint32_t start, const uint32_t count, const size_t byteStart) {
    ring[emitted % 3] = EmittedToken{ code, start, count, byteStart };
    if (pendingCount == 3) {
      const uint8_t rightCodes[3] = {
        ring[(emitted - 2) % 3].code,
        ring[(emitted - 1) % 3].code,
        code,
      };
      uint16_t rule = 0xFFFFu;
      const uint8_t action =
              firstBoundary == 0 ? static_cast<uint8_t>(BreakAction::NoBreak) : resolveBoundary(pending[0], rightCodes, &rule);
      writeAction(firstBoundary, action, rule);
      pending[0] = pending[1];
      pending[1] = pending[2];
      --pendingCount;
      ++firstBoundary;
    }
    state = lineBreakDfaTransition(state, lineBreakDfaTokenSymbol(code));
    pending[pendingCount++] = state;
    ++emitted;
  };

  const uint8_t* asciiCodes = asciiTokenCodes();
  bool hasToken = false;
  uint8_t tokenBase = 0;
  uint8_t tokenFlags = 0;
  uint8_t tokenCodeValue = 0;
  uint32_t tokenStart = 0;
  uint32_t tokenCount = 0;
  size_t tokenByteStart = 0;
  const auto flushToken = [&]() {
    if (!hasToken) return;
    emit(tokenCodeValue, tokenStart, tokenCount, tokenByteStart);
    hasToken = false;
  };

  uint32_t codePoint = 0;
  size_t byteOffset = 0;
  struct CachedProperty {
    uint32_t codePoint = ~0u;
    uint8_t value = 0;
    uint8_t flags = 0;
  };
  CachedProperty propertyCache[4096];
  while (next(codePoint, byteOffset)) {
    LineBreakClass resolved;
    uint8_t contextFlags = 0;
    if (codePoint < 128) {
      const AsciiLineBreakProperty& property = kAsciiLineBreakProperties[codePoint];
      resolved = property.value;
      contextFlags = property.flags;
    } else {
      CachedProperty& cached = propertyCache[codePoint & 4095u];
      if (cached.codePoint != codePoint) {
        cached.codePoint = codePoint;
        cached.value = static_cast<uint8_t>(resolveLineBreakClass(codePoint));
        cached.flags = lineBreakFlags(codePoint);
      }
      resolved = static_cast<LineBreakClass>(cached.value);
      contextFlags = cached.flags;
    }
    const bool combining = resolved == LineBreakClass::CM || resolved == LineBreakClass::ZWJ;
    if (combining && hasToken && !cannotAttachTo(tokenBase)) {
      ++tokenCount;
      // LB8a cares about a ZWJ at the end of the cluster; a following CM means
      // the cluster no longer ends in ZWJ.
      if (resolved == LineBreakClass::ZWJ) {
        tokenFlags |= kLineBreakFlagZwjTail;
      } else {
        tokenFlags &= static_cast<uint8_t>(~kLineBreakFlagZwjTail);
      }
      tokenCodeValue = tokenCode(static_cast<LineBreakClass>(tokenBase), tokenFlags);
      ++codePointIndex;
      continue;
    }

    // Consecutive ASCII characters of the same class have no internal break
    // opportunity for these classes and leave the DFA in the same class state,
    // so fold the run into the current token. PO/PR (prefix/postfix) are
    // deliberately excluded: adjacent `%`/`$` do break. ActionSink still writes
    // NoBreak for the internal codepoint boundaries; layout only sees the
    // (absent) opportunities.
    const bool coalescibleClass = resolved == LineBreakClass::AL ||
                                  resolved == LineBreakClass::NU ||
                                  resolved == LineBreakClass::SP;
    if (codePoint < 128 && hasToken && !combining && coalescibleClass &&
        tokenBase == static_cast<uint8_t>(resolved) && tokenFlags == contextFlags) {
      ++tokenCount;
      ++codePointIndex;
      continue;
    }

    flushToken();
    if (combining) {
      // LB10: a CM/ZWJ with no valid base is treated as U+0041 (AL, Lu, Na, no EP).
      tokenBase = static_cast<uint8_t>(LineBreakClass::AL);
      tokenFlags = resolved == LineBreakClass::ZWJ ? kLineBreakFlagZwjTail : 0;
    } else {
      tokenBase = static_cast<uint8_t>(resolved);
      tokenFlags = contextFlags;
    }
    tokenCodeValue = (codePoint < 128 && !combining) ? asciiCodes[codePoint]
                                                     : tokenCode(static_cast<LineBreakClass>(tokenBase), tokenFlags);
    tokenStart = static_cast<uint32_t>(codePointIndex);
    tokenCount = 1;
    tokenByteStart = byteOffset;
    hasToken = true;
    ++codePointIndex;
  }
  flushToken();

  while (pendingCount > 1) {
    uint8_t rightCodes[3] = {
      kLineBreakEotTokenCode,
      kLineBreakEotTokenCode,
      kLineBreakEotTokenCode,
    };
    for (size_t offset = 0; offset < 3; ++offset) {
      if (firstBoundary + offset < emitted) {
        rightCodes[offset] = ring[(firstBoundary + offset) % 3].code;
      }
    }
    uint16_t rule = 0xFFFFu;
    const uint8_t action =
            firstBoundary == 0 ? static_cast<uint8_t>(BreakAction::NoBreak) : resolveBoundary(pending[0], rightCodes, &rule);
    writeAction(firstBoundary, action, rule);
    pending[0] = pending[1];
    pending[1] = pending[2];
    --pendingCount;
    ++firstBoundary;
  }
  sink.finish(codePointIndex);
}

/**
 * ASCII-only variant of runEngine. Every code point is one byte and the classes
 * never contain CM/ZWJ, so the decode, LB9/LB10 and tokenCodepoint bookkeeping
 * collapses into a byte loop. Used by both the opportunity and boundary streams
 * when the input is pure ASCII (the common layout case).
 */
template<typename Sink, typename CharAt>
void runAsciiEngineCore(const size_t size, CharAt charAt, Sink&& sink) {
  uint16_t pending[3] = { kLineBreakDfaInitialState, 0, 0 };
  size_t pendingCount = 1;
  size_t firstBoundary = 0;
  uint16_t state = kLineBreakDfaInitialState;
  EmittedToken ring[3];
  size_t emitted = 0;

  const auto writeAction = [&](const size_t boundary, const uint8_t action,
      const uint16_t rule) {
    const EmittedToken& token = ring[boundary % 3];
    sink.token(token.codePointStart, token.codePointCount, token.byteStart, action, rule);
  };
  const auto emit = [&](const uint8_t code, const uint32_t start, const uint32_t count,
      const size_t byteStart) {
    ring[emitted % 3] = EmittedToken{ code, start, count, byteStart };
    if (pendingCount == 3) {
      const uint8_t rightCodes[3] = {
        ring[(emitted - 2) % 3].code,
        ring[(emitted - 1) % 3].code,
        code,
      };
      uint16_t rule = 0xFFFFu;
      const uint8_t action = firstBoundary == 0
        ? static_cast<uint8_t>(BreakAction::NoBreak)
        : resolveBoundary(pending[0], rightCodes, &rule);
      writeAction(firstBoundary, action, rule);
      pending[0] = pending[1];
      pending[1] = pending[2];
      --pendingCount;
      ++firstBoundary;
    }
    state = lineBreakDfaTransition(state, lineBreakDfaTokenSymbol(code));
    pending[pendingCount++] = state;
    ++emitted;
  };

  const uint8_t* asciiCodes = asciiTokenCodes();
  bool hasToken = false;
  uint8_t tokenBase = 0;
  uint8_t tokenFlags = 0;
  uint8_t tokenCodeValue = 0;
  uint32_t tokenStart = 0;
  uint32_t tokenCount = 0;
  size_t tokenByteStart = 0;
  const auto flushToken = [&]() {
    if (!hasToken) return;
    emit(tokenCodeValue, tokenStart, tokenCount, tokenByteStart);
    hasToken = false;
  };

  for (size_t byteIndex = 0; byteIndex < size; ++byteIndex) {
    const uint8_t codePoint = static_cast<uint8_t>(charAt(byteIndex));
    const AsciiLineBreakProperty& property = kAsciiLineBreakProperties[codePoint];
    const uint8_t resolved = static_cast<uint8_t>(property.value);
    const bool coalescible = property.value == LineBreakClass::AL ||
                             property.value == LineBreakClass::NU ||
                             property.value == LineBreakClass::SP;
    if (hasToken && coalescible && tokenBase == resolved && tokenFlags == property.flags) {
      ++tokenCount;
      continue;
    }
    flushToken();
    tokenBase = resolved;
    tokenFlags = property.flags;
    tokenCodeValue = asciiCodes[codePoint];
    tokenStart = static_cast<uint32_t>(byteIndex);
    tokenCount = 1;
    tokenByteStart = byteIndex;
    hasToken = true;
  }
  flushToken();

  while (pendingCount > 1) {
    uint8_t rightCodes[3] = {
      kLineBreakEotTokenCode,
      kLineBreakEotTokenCode,
      kLineBreakEotTokenCode,
    };
    for (size_t index = 0; index < 3; ++index) {
      if (firstBoundary + index < emitted) {
        rightCodes[index] = ring[(firstBoundary + index) % 3].code;
      }
    }
    uint16_t rule = 0xFFFFu;
    const uint8_t action = firstBoundary == 0
      ? static_cast<uint8_t>(BreakAction::NoBreak)
      : resolveBoundary(pending[0], rightCodes, &rule);
    writeAction(firstBoundary, action, rule);
    pending[0] = pending[1];
    pending[1] = pending[2];
    --pendingCount;
    ++firstBoundary;
  }
  sink.finish(size);
}

template<typename Sink>
void runAsciiEngine(const std::string_view text, Sink&& sink) {
  runAsciiEngineCore(text.size(), [&](const size_t index) { return text[index]; },
    std::forward<Sink>(sink));
}

bool isAsciiOnly(const std::string_view text) {
  return std::all_of(text.begin(), text.end(),
    [](const char value) { return static_cast<unsigned char>(value) < 0x80u; });
}

void engineActionsInto(const std::vector<uint32_t>& codePoints, std::vector<uint8_t>& actions) {
  const size_t count = codePoints.size();
  actions.assign(count + 1, 0);
  bool ascii = true;
  bool alphabetic = true;
  for (const uint32_t codePoint: codePoints) {
    if (codePoint >= 0x80u || codePoint == 0u) {
      ascii = false;
      alphabetic = false;
      break;
    }
    if (!((codePoint >= 'A' && codePoint <= 'Z') ||
          (codePoint >= 'a' && codePoint <= 'z'))) {
      alphabetic = false;
    }
  }
  if (alphabetic) {
    // A single alphabetic run has no internal break opportunity: only the eot
    // boundary is mandatory. actions is already zero (NoBreak).
    actions.back() = static_cast<uint8_t>(BreakAction::MandatoryBreak);
    return;
  }
  bool prose = true;
  for (size_t position = 0; position < codePoints.size();) {
    const uint32_t codePoint = codePoints[position];
    if (codePoint == ' ') {
      while (position < codePoints.size() && codePoints[position] == ' ') ++position;
      if (position < codePoints.size()) {
        const uint32_t next = codePoints[position];
        const bool nextLetter = (next >= 'A' && next <= 'Z') || (next >= 'a' && next <= 'z');
        const bool nextQuote = next == '\'' || next == '"';
        if (nextLetter || nextQuote) {
          actions[position] = static_cast<uint8_t>(BreakAction::AllowBreak);
        }
      }
      continue;
    }
    const bool letter = (codePoint >= 'A' && codePoint <= 'Z') ||
                        (codePoint >= 'a' && codePoint <= 'z');
    const bool punctuation = codePoint == '.' || codePoint == ',' || codePoint == ';' ||
                             codePoint == ':' || codePoint == '!' || codePoint == '?' ||
                             codePoint == '\'' || codePoint == '"';
    if (!letter && !punctuation) {
      prose = false;
      break;
    }
    ++position;
  }
  if (prose) {
    // Single-line prose: UAX #14 only breaks after a run of spaces before a
    // letter or quote (LB18); punctuation and letters stay attached. This
    // avoids the per-token DFA ring for the most common paragraph shape.
    actions.back() = static_cast<uint8_t>(BreakAction::MandatoryBreak);
    return;
  }
  if (ascii) {
    runAsciiEngineCore(codePoints.size(),
      [&](const size_t index) { return static_cast<char>(codePoints[index]); },
      ActionSink{ actions });
    return;
  }
  size_t index = 0;
  const auto next = [&](uint32_t& codePoint, size_t& byteOffset) {
    if (index >= codePoints.size()) return false;
    codePoint = codePoints[index];
    byteOffset = index;
    ++index;
    return true;
  };
  runEngine(next, ActionSink{ actions });
}

bool isContinuation(const unsigned char value) { return (value & 0xC0u) == 0x80u; }

/** Decodes one scalar value; invalid input produces one U+FFFD and consumes one byte. */
inline size_t decodeCodePoint(const std::string_view text, const size_t offset, uint32_t& codePoint) {
  const auto lead = static_cast<unsigned char>(text[offset]);
  const size_t available = text.size() - offset;
  if (lead < 0x80u) {
    codePoint = lead;
    return 1;
  }
  if (lead >= 0xC2u && lead <= 0xDFu && available >= 2) {
    const auto second = static_cast<unsigned char>(text[offset + 1]);
    if (isContinuation(second)) {
      codePoint = (static_cast<uint32_t>(lead & 0x1Fu) << 6) | static_cast<uint32_t>(second & 0x3Fu);
      return 2;
    }
  } else if (lead >= 0xE0u && lead <= 0xEFu && available >= 3) {
    const auto second = static_cast<unsigned char>(text[offset + 1]);
    const auto third = static_cast<unsigned char>(text[offset + 2]);
    if (isContinuation(second) && isContinuation(third)) {
      const uint32_t value = (static_cast<uint32_t>(lead & 0x0Fu) << 12) |
                             (static_cast<uint32_t>(second & 0x3Fu) << 6) | static_cast<uint32_t>(third & 0x3Fu);
      if (value >= 0x800u && (value < 0xD800u || value > 0xDFFFu)) {
        codePoint = value;
        return 3;
      }
    }
  } else if (lead >= 0xF0u && lead <= 0xF4u && available >= 4) {
    const auto second = static_cast<unsigned char>(text[offset + 1]);
    const auto third = static_cast<unsigned char>(text[offset + 2]);
    const auto fourth = static_cast<unsigned char>(text[offset + 3]);
    if (isContinuation(second) && isContinuation(third) && isContinuation(fourth)) {
      const uint32_t value = (static_cast<uint32_t>(lead & 0x07u) << 18) |
                             (static_cast<uint32_t>(second & 0x3Fu) << 12) |
                             (static_cast<uint32_t>(third & 0x3Fu) << 6) | static_cast<uint32_t>(fourth & 0x3Fu);
      if (value >= 0x10000u && value <= 0x10FFFFu) {
        codePoint = value;
        return 4;
      }
    }
  }
  codePoint = 0xFFFDu;
  return 1;
}

} // namespace

std::vector<uint8_t> computeLineBreakActions(const std::vector<uint32_t>& codePoints) {
  std::vector<uint8_t> actions;
  engineActionsInto(codePoints, actions);
  return actions;
}

std::vector<uint16_t> computeLineBreakRules(const std::vector<uint32_t>& codePoints) {
  std::vector<uint16_t> rules(codePoints.size() + 1, 0xFFFFu);
  size_t index = 0;
  const auto next = [&](uint32_t& codePoint, size_t& byteOffset) {
    if (index >= codePoints.size()) return false;
    codePoint = codePoints[index];
    byteOffset = index;
    ++index;
    return true;
  };
  runEngine(next, RuleSink{ rules });
  return rules;
}

const char* lineBreakRuleName(const uint16_t rule) noexcept {
  return lineBreakDfaRuleId(rule);
}

std::span<const uint8_t> LineBreakActionBuffer::compute(
    const std::vector<uint32_t>& codePoints) {
  engineActionsInto(codePoints, mActions);
  return mActions;
}

size_t forEachLineBreakAction(const std::vector<uint32_t>& codePoints,
    const LineBreakActionCallback callback, void* context) {
  if (callback == nullptr) return 0;
  const size_t count = codePoints.size();
  bool alphabetic = true;
  bool ascii = true;
  for (const uint32_t codePoint: codePoints) {
    if (codePoint >= 0x80u || codePoint == 0u) {
      ascii = false;
      alphabetic = false;
      break;
    }
    if (!((codePoint >= 'A' && codePoint <= 'Z') ||
          (codePoint >= 'a' && codePoint <= 'z'))) {
      alphabetic = false;
    }
  }
  if (alphabetic) {
    for (size_t position = 0; position < count; ++position) {
      callback(context, position, BreakAction::NoBreak);
    }
    callback(context, count, BreakAction::MandatoryBreak);
    return count + 1;
  }
  bool prose = true;
  for (const uint32_t codePoint: codePoints) {
    const bool letter = (codePoint >= 'A' && codePoint <= 'Z') ||
                        (codePoint >= 'a' && codePoint <= 'z');
    const bool punctuation = codePoint == '.' || codePoint == ',' || codePoint == ';' ||
                             codePoint == ':' || codePoint == '!' || codePoint == '?' ||
                             codePoint == '\'' || codePoint == '"' || codePoint == ' ';
    if (!letter && !punctuation) {
      prose = false;
      break;
    }
  }
  if (prose) {
    for (size_t position = 0; position <= count; ++position) {
      BreakAction action = BreakAction::NoBreak;
      if (position == count) {
        action = BreakAction::MandatoryBreak;
      } else if (position > 0 && codePoints[position - 1] == ' ' &&
                 codePoints[position] != ' ') {
        const uint32_t codePoint = codePoints[position];
        const bool letter = (codePoint >= 'A' && codePoint <= 'Z') ||
                            (codePoint >= 'a' && codePoint <= 'z');
        if (letter || codePoint == '\'' || codePoint == '"') {
          action = BreakAction::AllowBreak;
        }
      }
      callback(context, position, action);
    }
    return count + 1;
  }
  ActionCallbackSink sink{ callback, context };
  if (ascii) {
    runAsciiEngineCore(codePoints.size(),
      [&](const size_t index) { return static_cast<char>(codePoints[index]); }, sink);
    return count + 1;
  }
  size_t index = 0;
  const auto next = [&](uint32_t& codePoint, size_t& byteOffset) {
    if (index >= codePoints.size()) return false;
    codePoint = codePoints[index];
    byteOffset = index;
    ++index;
    return true;
  };
  runEngine(next, sink);
  return count + 1;
}

size_t forEachLineBreakActionBlock(const std::vector<uint32_t>& codePoints,
    const LineBreakActionBlockCallback callback, void* context) {
  if (callback == nullptr) return 0;
  const size_t count = codePoints.size();
  bool alphabetic = true;
  bool ascii = true;
  for (const uint32_t codePoint: codePoints) {
    if (codePoint >= 0x80u || codePoint == 0u) {
      ascii = false;
      alphabetic = false;
      break;
    }
    if (!((codePoint >= 'A' && codePoint <= 'Z') ||
          (codePoint >= 'a' && codePoint <= 'z'))) {
      alphabetic = false;
    }
  }
  ActionBlockWriter writer;
  writer.callback = callback;
  writer.context = context;
  if (alphabetic) {
    for (size_t position = 0; position < count; ++position) {
      writer.push(static_cast<uint8_t>(BreakAction::NoBreak));
    }
    writer.push(static_cast<uint8_t>(BreakAction::MandatoryBreak));
    writer.flush();
    return count + 1;
  }
  bool prose = true;
  for (const uint32_t codePoint: codePoints) {
    const bool letter = (codePoint >= 'A' && codePoint <= 'Z') ||
                        (codePoint >= 'a' && codePoint <= 'z');
    const bool punctuation = codePoint == '.' || codePoint == ',' || codePoint == ';' ||
                             codePoint == ':' || codePoint == '!' || codePoint == '?' ||
                             codePoint == '\'' || codePoint == '"' || codePoint == ' ';
    if (!letter && !punctuation) {
      prose = false;
      break;
    }
  }
  if (prose) {
    for (size_t position = 0; position <= count; ++position) {
      BreakAction action = BreakAction::NoBreak;
      if (position == count) {
        action = BreakAction::MandatoryBreak;
      } else if (position > 0 && codePoints[position - 1] == ' ' &&
                 codePoints[position] != ' ') {
        const uint32_t codePoint = codePoints[position];
        const bool letter = (codePoint >= 'A' && codePoint <= 'Z') ||
                            (codePoint >= 'a' && codePoint <= 'z');
        if (letter || codePoint == '\'' || codePoint == '"') {
          action = BreakAction::AllowBreak;
        }
      }
      writer.push(static_cast<uint8_t>(action));
    }
    writer.flush();
    return count + 1;
  }
  ActionBlockSink sink{ writer };
  if (ascii) {
    runAsciiEngineCore(codePoints.size(),
      [&](const size_t index) { return static_cast<char>(codePoints[index]); }, sink);
    writer.flush();
    return count + 1;
  }
  size_t index = 0;
  const auto next = [&](uint32_t& codePoint, size_t& byteOffset) {
    if (index >= codePoints.size()) return false;
    codePoint = codePoints[index];
    byteOffset = index;
    ++index;
    return true;
  };
  runEngine(next, sink);
  writer.flush();
  return count + 1;
}

namespace {

struct BoundaryCallbackSink {
  std::string_view text;
  LineBreakBoundaryCallback callback = nullptr;
  void* context = nullptr;
  size_t count = 0;

  void token(const uint32_t, const uint32_t codePointCount, const size_t byteStart,
      const uint8_t action, const uint16_t) {
    callback(context, byteStart, static_cast<BreakAction>(action));
    ++count;
    size_t cursor = byteStart;
    for (uint32_t index = 1; index < codePointCount; ++index) {
      uint32_t codePoint = 0;
      cursor += decodeCodePoint(text, cursor, codePoint);
      callback(context, cursor, BreakAction::NoBreak);
      ++count;
    }
  }

  void finish(const size_t) {
    callback(context, text.size(), BreakAction::MandatoryBreak);
    ++count;
  }
};

struct BoundaryVectorSink {
  std::string_view text;
  std::vector<LineBreakBoundary>& boundaries;

  void token(const uint32_t, const uint32_t codePointCount, const size_t byteStart,
      const uint8_t action, const uint16_t) {
    boundaries.push_back({ static_cast<uint32_t>(byteStart),
      static_cast<BreakAction>(action) });
    size_t cursor = byteStart;
    for (uint32_t index = 1; index < codePointCount; ++index) {
      uint32_t codePoint = 0;
      cursor += decodeCodePoint(text, cursor, codePoint);
      boundaries.push_back({ static_cast<uint32_t>(cursor), BreakAction::NoBreak });
    }
  }

  void finish(const size_t) {
    boundaries.push_back({ static_cast<uint32_t>(text.size()), BreakAction::MandatoryBreak });
  }
};

} // namespace

template<typename Sink>
void runUtf8Engine(const std::string_view utf8, Sink&& sink) {
  FastUtf8Engine<std::remove_reference_t<Sink>> engine(sink);
  size_t offset = 0;
  while (offset < utf8.size()) {
    const auto lead = static_cast<unsigned char>(utf8[offset]);
    if (lead < 0x80u) {
      const size_t start = offset;
      do {
        ++offset;
      } while (offset < utf8.size() && static_cast<unsigned char>(utf8[offset]) < 0x80u);
      engine.consumeAsciiRun(utf8, start, offset);
      continue;
    }
    uint32_t codePoint = 0;
    const size_t length = decodeCodePoint(utf8, offset, codePoint);
    engine.consume(codePoint, offset);
    offset += length;
  }
  engine.finish();
}

size_t forEachLineBreakBoundary(const std::string_view utf8,
    const LineBreakBoundaryCallback callback, void* const context) {
  BoundaryCallbackSink sink{ utf8, callback, context, 0 };
  if (isAsciiOnly(utf8)) {
    runAsciiEngine(utf8, sink);
    return sink.count;
  }
  runUtf8Engine(utf8, sink);
  return sink.count;
}

std::vector<LineBreakBoundary> computeLineBreaks(const std::string_view utf8) {
  // byteOffset is 32-bit in the compact result; larger inputs must stream.
  if (utf8.size() > UINT32_MAX) return {};
  std::vector<LineBreakBoundary> boundaries;
  boundaries.reserve(utf8.size() + 1);
  if (isAsciiOnly(utf8)) {
    BoundaryVectorSink sink{ utf8, boundaries };
    runAsciiEngine(utf8, sink);
    return boundaries;
  }
  BoundaryVectorSink sink{ utf8, boundaries };
  runUtf8Engine(utf8, sink);
  return boundaries;
}

LineBreakTable computeLineBreakTable(const std::string_view utf8) {
  if (utf8.size() > UINT32_MAX) return {};
  LineBreakTable table;
  table.offsets.reserve(utf8.size() + 1);
  table.actions.reserve(utf8.size() + 1);
  forEachLineBreakBoundary(utf8, [&table](const size_t offset, const BreakAction action) {
    table.offsets.push_back(static_cast<uint32_t>(offset));
    table.actions.push_back(static_cast<uint8_t>(action));
  });
  return table;
}

std::vector<LineBreakOpportunity> computeLineBreakOpportunities(const std::string_view utf8) {
  std::vector<LineBreakOpportunity> opportunities;
  opportunities.reserve(utf8.size() / 5 + 1);
  if (isAsciiOnly(utf8)) {
    runAsciiEngine(utf8, OpportunitySink{ opportunities });
    return opportunities;
  }
  runUtf8Engine(utf8, OpportunitySink{ opportunities });
  return opportunities;
}

size_t forEachLineBreakOpportunity(const std::string_view utf8,
    const LineBreakOpportunityCallback callback, void* const context) {
  CallbackOpportunitySink sink{ callback, context, 0 };
  if (isAsciiOnly(utf8)) {
    runAsciiEngine(utf8, sink);
    return sink.count;
  }
  runUtf8Engine(utf8, sink);
  return sink.count;
}

} // namespace coral::ui::unicode
