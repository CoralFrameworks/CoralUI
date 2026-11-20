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

#ifndef CORAL_UI_LINEBREAKPOLICY_H
#define CORAL_UI_LINEBREAKPOLICY_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace coral::ui {

enum class LineBreakStrictness : uint8_t { Strict, Normal, Loose };
enum class WordBreak : uint8_t { Normal, BreakAll, KeepAll };
enum class OverflowWrap : uint8_t { Normal, BreakWord, Anywhere };

/** Optional dictionary hook for scripts without spaces (Thai, Khmer, Lao, ...). */
class BreakDictionary {
 public:
  virtual ~BreakDictionary() = default;
  virtual std::vector<size_t> opportunities(std::string_view text, std::string_view language) const = 0;
};

/** ICU-derived Thai word dictionary using maximal matching. */
class ThaiBreakDictionary final : public BreakDictionary {
 public:
  std::vector<size_t> opportunities(std::string_view text, std::string_view language) const override;
};

[[nodiscard]] std::shared_ptr<const BreakDictionary> makeThaiBreakDictionary();

/** ICU-derived Khmer word dictionary using maximal matching. */
class KhmerBreakDictionary final : public BreakDictionary {
 public:
  std::vector<size_t> opportunities(std::string_view text, std::string_view language) const override;
};

[[nodiscard]] std::shared_ptr<const BreakDictionary> makeKhmerBreakDictionary();

/** ICU-derived Lao word dictionary using maximal matching. */
class LaoBreakDictionary final : public BreakDictionary {
 public:
  std::vector<size_t> opportunities(std::string_view text, std::string_view language) const override;
};

[[nodiscard]] std::shared_ptr<const BreakDictionary> makeLaoBreakDictionary();

/** ICU-derived Burmese word dictionary using maximal matching. */
class BurmeseBreakDictionary final : public BreakDictionary {
 public:
  std::vector<size_t> opportunities(std::string_view text, std::string_view language) const override;
};

[[nodiscard]] std::shared_ptr<const BreakDictionary> makeBurmeseBreakDictionary();

/** One tailored opportunity; mandatory breaks always win. */
struct TailoredLineBreak {
  size_t byteOffset = 0;
  bool mandatory = false;
  /** True when a hyphen glyph must be drawn at the end of the broken line. */
  bool hyphenate = false;
};

/** CSS-like policy applied on top of the default UAX #14 opportunities. */
struct LineBreakPolicy {
  LineBreakStrictness strictness = LineBreakStrictness::Normal;
  WordBreak wordBreak = WordBreak::Normal;
  /**
   * CSS default: an overlong word overflows instead of being split.
   * Controls that cannot scroll horizontally (for example TextInput) opt into
   * BreakWord explicitly; the library default stays CSS-compatible.
   */
  OverflowWrap overflowWrap = OverflowWrap::Normal;
  std::string language;
  bool hyphenation = false;
  /** CSS hyphenate-limit-chars; 0 uses the language default. */
  uint8_t hyphenationMinWordLength = 0;
  uint8_t hyphenationMinBefore = 0;
  uint8_t hyphenationMinAfter = 0;
  /**
   * CSS hyphenate-limit-zone: the maximum line-end slack (in pixels) at which a
   * hyphen break is still taken. 0 disables the zone check.
   */
  float hyphenateLimitZone = 0.0f;
  /** CSS hyphenate-limit-lines: max consecutive hyphenated lines; 0 = unlimited. */
  uint8_t hyphenateLimitLines = 0;
  std::shared_ptr<const BreakDictionary> dictionary;

  [[nodiscard]] bool isDefault() const noexcept {
    return strictness == LineBreakStrictness::Normal && wordBreak == WordBreak::Normal &&
           overflowWrap == OverflowWrap::Normal && language.empty() && !hyphenation &&
           dictionary == nullptr;
  }

  friend bool operator==(const LineBreakPolicy& left, const LineBreakPolicy& right) {
    return left.strictness == right.strictness && left.wordBreak == right.wordBreak &&
           left.overflowWrap == right.overflowWrap && left.language == right.language &&
           left.hyphenation == right.hyphenation && left.dictionary == right.dictionary;
  }
  friend bool operator!=(const LineBreakPolicy& left, const LineBreakPolicy& right) {
    return !(left == right);
  }
};

/**
 * Composes default opportunities with @p policy.
 *
 * Mandatory boundaries are preserved and take precedence. BreakAll adds all
 * grapheme boundaries; KeepAll removes CJK-to-CJK breaks; dictionary results are
 * merged. Strict/Loose are extension points for punctuation tailoring.
 * When `hyphenation` is set and the language has a built-in Liang table,
 * word-internal hyphenation points are added with `hyphenate = true`.
 */
[[nodiscard]] std::vector<TailoredLineBreak> computeTailoredLineBreaks(
  std::string_view utf8, const LineBreakPolicy& policy);

} // namespace coral::ui

#endif // CORAL_UI_LINEBREAKPOLICY_H
