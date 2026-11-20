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

#ifndef CORAL_UI_UNICODE_LINEBREAKITERATOR_H
#define CORAL_UI_UNICODE_LINEBREAKITERATOR_H

#include "unicode/LineBreakDfa.generated.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace coral::ui::unicode {

/**
 * One boundary in logical order. `byteOffset` is a UTF-8 byte offset for
 * computeLineBreaks(). It is 32-bit to keep the 1M-entry result vector at 8 MiB
 * instead of 16 MiB; inputs above 4 GiB must use the streaming API.
 */
struct LineBreakBoundary {
  uint32_t byteOffset = 0;
  BreakAction action = BreakAction::NoBreak;
};
static_assert(sizeof(LineBreakBoundary) == 8, "LineBreakBoundary must stay 8 bytes");

/** A boundary that can actually start a new line (AllowBreak or MandatoryBreak). */
struct LineBreakOpportunity {
  size_t byteOffset = 0;
  BreakAction action = BreakAction::AllowBreak;
};

/**
 * Computes every boundary of @p utf8, including the sot boundary at offset 0 and
 * the mandatory eot boundary at `utf8.size()`.
 *
 * The implementation normalizes LB9/LB10 (CM/ZWJ attachment), then runs the
 * generated UAX #14 DFA. Right contexts of at most three tokens are resolved with
 * a small lookahead queue, so the result is logical order and independent of
 * font, width or Bidi.
 */
[[nodiscard]] std::vector<LineBreakBoundary> computeLineBreaks(std::string_view utf8);

/**
 * Compact all-boundary table: parallel uint32 offsets and uint8 actions.
 *
 * One million boundaries take ~5 MiB instead of 8 MiB for
 * `vector<LineBreakBoundary>`, and the struct-of-arrays layout scans faster.
 * Inputs above 4 GiB must use the streaming API.
 */
struct LineBreakTable {
  std::vector<uint32_t> offsets;
  std::vector<uint8_t> actions;

  [[nodiscard]] size_t size() const noexcept { return offsets.size(); }
  [[nodiscard]] bool empty() const noexcept { return offsets.empty(); }
};

[[nodiscard]] LineBreakTable computeLineBreakTable(std::string_view utf8);

/**
 * Computes only the usable line-break opportunities. This is the layout-facing
 * variant: it drops the NoBreak boundaries that line construction never uses.
 */
[[nodiscard]] std::vector<LineBreakOpportunity> computeLineBreakOpportunities(std::string_view utf8);

/** C callback used by the zero-allocation opportunity stream. */
using LineBreakOpportunityCallback = void (*)(void* context, size_t byteOffset, BreakAction action);

/** C callback used by the zero-allocation all-boundary stream. */
using LineBreakBoundaryCallback = void (*)(void* context, size_t byteOffset, BreakAction action);

/**
 * Streams every boundary (including NoBreak), starting with the sot boundary at
 * offset 0 and ending with the mandatory eot boundary at `utf8.size()`.
 * The return value is the boundary count; no result vector is built.
 */
size_t forEachLineBreakBoundary(std::string_view utf8, LineBreakBoundaryCallback callback,
  void* context);

/** Type-safe callback overload; @p sink is called as sink(byteOffset, action). */
template<typename Sink>
size_t forEachLineBreakBoundary(const std::string_view utf8, Sink&& sink) {
  return forEachLineBreakBoundary(utf8,
    [](void* context, const size_t byteOffset, const BreakAction action) {
      (*static_cast<std::remove_reference_t<Sink>*>(context))(byteOffset, action);
    }, &sink);
}

/**
 * Streams usable break opportunities without building a result vector. Layout
 * can consume the callback directly; the return value is the opportunity count.
 */
size_t forEachLineBreakOpportunity(std::string_view utf8, LineBreakOpportunityCallback callback,
  void* context);

/** Type-safe callback overload; @p sink is called as sink(byteOffset, action). */
template<typename Sink>
size_t forEachLineBreakOpportunity(const std::string_view utf8, Sink&& sink) {
  return forEachLineBreakOpportunity(utf8,
    [](void* context, const size_t byteOffset, const BreakAction action) {
      (*static_cast<std::remove_reference_t<Sink>*>(context))(byteOffset, action);
    }, &sink);
}

/**
 * Conformance/testing view over already-decoded code points.
 *
 * Returns one action per code point boundary (size N+1). BreakAction values are
 * preserved: NoBreak=0, AllowBreak=1, MandatoryBreak=2.
 */
[[nodiscard]] std::vector<uint8_t> computeLineBreakActions(const std::vector<uint32_t>& codePoints);

/** One UAX #14 rule index per boundary (0xFFFF when no rule was resolved). */
[[nodiscard]] std::vector<uint16_t> computeLineBreakRules(const std::vector<uint32_t>& codePoints);

/** @return the external rule id (for example "LB21a"), or "unknown". */
[[nodiscard]] const char* lineBreakRuleName(uint16_t rule) noexcept;

/**
 * Reusable action buffer for callers that recompute actions repeatedly.
 *
 * `compute()` reuses the allocation between calls (the action bytes are still
 * rewritten), so a steady-state caller performs no per-call vector allocation.
 * The returned span stays valid until the next `compute()`/`clear()`.
 */
class LineBreakActionBuffer {
 public:
  [[nodiscard]] std::span<const uint8_t> compute(const std::vector<uint32_t>& codePoints);
  [[nodiscard]] std::span<const uint8_t> actions() const noexcept { return mActions; }
  [[nodiscard]] size_t size() const noexcept { return mActions.size(); }
  void clear() noexcept { mActions.clear(); }

 private:
  std::vector<uint8_t> mActions;
};

/** Streaming action callback: called once per code point boundary, in order. */
using LineBreakActionCallback = void (*)(void* context, size_t boundary, BreakAction action);

/**
 * Streams one action per code point boundary without allocating a 1 MiB action
 * vector. Useful for conformance/debug consumers; layout should prefer
 * forEachLineBreakOpportunity(), which emits only actual opportunities.
 *
 * @return the number of boundaries emitted (codePoints.size() + 1).
 */
size_t forEachLineBreakAction(const std::vector<uint32_t>& codePoints,
  LineBreakActionCallback callback, void* context);

template<typename Sink>
size_t forEachLineBreakAction(const std::vector<uint32_t>& codePoints, Sink&& sink) {
  return forEachLineBreakAction(codePoints,
    [](void* context, const size_t boundary, const BreakAction action) {
      (*static_cast<std::remove_reference_t<Sink>*>(context))(boundary, action);
    }, &sink);
}

/** Block callback: @p actions covers boundaries [boundaryStart, +count). */
using LineBreakActionBlockCallback = void (*)(void* context, size_t boundaryStart,
  const uint8_t* actions, size_t count);

/**
 * Streams actions in 4 KiB blocks. Memory use is one 4 KiB stack block, and the
 * callback runs once per block instead of once per boundary. The pointer passed
 * to the callback is only valid during that callback.
 *
 * @return the number of boundaries emitted (codePoints.size() + 1).
 */
size_t forEachLineBreakActionBlock(const std::vector<uint32_t>& codePoints,
  LineBreakActionBlockCallback callback, void* context);

} // namespace coral::ui::unicode

#endif // CORAL_UI_UNICODE_LINEBREAKITERATOR_H
