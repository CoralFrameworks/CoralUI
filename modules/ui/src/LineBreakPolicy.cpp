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

#include <ui/LineBreakPolicy.h>

#include "unicode/GraphemeBreak.h"
#include "unicode/Hyphenation.h"
#include "unicode/LineBreakProperties.generated.h"
#include "unicode/LineBreakIterator.h"

#include <algorithm>
#include <cstdint>

namespace coral::ui {

namespace {

bool continuation(const unsigned char value) { return (value & 0xC0u) == 0x80u; }

size_t sequenceLength(const std::string_view text, const size_t offset) {
  const auto lead = static_cast<unsigned char>(text[offset]);
  if (lead < 0x80u) return 1;
  if (lead < 0xE0u) return offset + 1 < text.size() && continuation(text[offset + 1]) ? 2 : 1;
  if (lead < 0xF0u) return offset + 2 < text.size() && continuation(text[offset + 1]) &&
                            continuation(text[offset + 2]) ? 3 : 1;
  return offset + 3 < text.size() && continuation(text[offset + 1]) &&
                 continuation(text[offset + 2]) && continuation(text[offset + 3]) ? 4 : 1;
}

uint32_t codePointAt(const std::string_view text, const size_t offset, const size_t length) {
  const auto lead = static_cast<unsigned char>(text[offset]);
  if (length == 1) return lead < 0x80u ? lead : 0xFFFDu;
  if (length == 2) {
    return (static_cast<uint32_t>(lead & 0x1Fu) << 6) |
           static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 1]) & 0x3Fu);
  }
  if (length == 3) {
    return (static_cast<uint32_t>(lead & 0x0Fu) << 12) |
           (static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 1]) & 0x3Fu) << 6) |
           static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 2]) & 0x3Fu);
  }
  return (static_cast<uint32_t>(lead & 0x07u) << 18) |
         (static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 1]) & 0x3Fu) << 12) |
         (static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 2]) & 0x3Fu) << 6) |
         static_cast<uint32_t>(static_cast<unsigned char>(text[offset + 3]) & 0x3Fu);
}

size_t previousCodePointStart(const std::string_view text, const size_t offset) {
  if (offset == 0) return 0;
  size_t start = offset - 1;
  while (start > 0 && continuation(static_cast<unsigned char>(text[start]))) --start;
  return start;
}

bool isCjkClass(const unicode::LineBreakClass value) {
  // ID/CJ cover Han/Kana; the Hangul syllable and jamo classes make keep-all
  // behave for Korean too.
  return value == unicode::LineBreakClass::ID || value == unicode::LineBreakClass::CJ ||
         value == unicode::LineBreakClass::H2 || value == unicode::LineBreakClass::H3 ||
         value == unicode::LineBreakClass::JL || value == unicode::LineBreakClass::JV ||
         value == unicode::LineBreakClass::JT;
}

unicode::LineBreakClass classAt(const std::string_view text, const size_t offset) {
  if (offset >= text.size()) return unicode::LineBreakClass::AL;
  return unicode::lineBreakClass(codePointAt(text, offset, sequenceLength(text, offset)));
}

bool isCjkLanguage(const std::string_view language) {
  return language.size() >= 2 && language[0] == 'j' && language[1] == 'a' ||
         language.size() >= 2 && language[0] == 'z' && language[1] == 'h';
}

bool isIterationMark(const uint32_t codePoint) {
  return codePoint == 0x3005u || codePoint == 0x303Bu || codePoint == 0x309Du ||
         codePoint == 0x309Eu || codePoint == 0x30FDu || codePoint == 0x30FEu;
}

bool isCenteredPunctuation(const uint32_t codePoint) {
  return codePoint == 0x30FBu || codePoint == 0xFF1Au || codePoint == 0xFF1Bu ||
         codePoint == 0xFF65u || codePoint == 0x203Cu || codePoint == 0x2047u ||
         codePoint == 0x2048u || codePoint == 0x2049u || codePoint == 0xFF01u ||
         codePoint == 0xFF1Fu;
}

bool isCjkHyphen(const uint32_t codePoint) {
  return codePoint == 0x301Cu || codePoint == 0x30A0u;
}

bool isLooseHyphen(const uint32_t codePoint) {
  return codePoint == 0x2010u || codePoint == 0x2013u;
}

bool isCjkBoundary(const std::string_view text, const size_t offset) {
  if (offset == 0 || offset >= text.size()) return false;
  const size_t previousStart = previousCodePointStart(text, offset);
  const uint32_t previous = codePointAt(text, previousStart, offset - previousStart);
  const size_t nextLength = sequenceLength(text, offset);
  const uint32_t next = codePointAt(text, offset, nextLength);
  return isCjkClass(unicode::resolveLineBreakClass(previous)) &&
         isCjkClass(unicode::resolveLineBreakClass(next));
}

/**
 * CSS break-all/anywhere may add opportunities inside words, but the UAX #14
 * prohibitions around punctuation still apply. This is a conservative subset:
 * no break before closing/attached marks and no break after openers/quotes.
 */
bool allowsIntraWordBreak(const std::string_view text, const size_t offset) {
  if (offset == 0 || offset >= text.size()) return false;
  const size_t previousStart = previousCodePointStart(text, offset);
  const uint32_t previous = codePointAt(text, previousStart, offset - previousStart);
  const uint32_t next = codePointAt(text, offset, sequenceLength(text, offset));
  const auto previousClass = unicode::resolveLineBreakClass(previous);
  const auto nextClass = unicode::resolveLineBreakClass(next);
  switch (nextClass) {
    case unicode::LineBreakClass::CL:
    case unicode::LineBreakClass::CP:
    case unicode::LineBreakClass::EX:
    case unicode::LineBreakClass::IS:
    case unicode::LineBreakClass::SY:
    case unicode::LineBreakClass::BA:
    case unicode::LineBreakClass::HH:
    case unicode::LineBreakClass::HY:
    case unicode::LineBreakClass::NS:
    case unicode::LineBreakClass::IN:
    case unicode::LineBreakClass::EM:
      return false;
    default:
      break;
  }
  switch (previousClass) {
    case unicode::LineBreakClass::OP:
    case unicode::LineBreakClass::QU:
    case unicode::LineBreakClass::GL:
      return false;
    default:
      break;
  }
  return true;
}

/** Adds Liang hyphenation points for ASCII/Latin-1 words; other scripts fall back to UAX #14. */
void appendHyphenationPoints(const std::string_view utf8, const LineBreakPolicy& policy,
    std::vector<TailoredLineBreak>& result) {
  const unicode::HyphenationLimits limits{ policy.hyphenationMinWordLength,
    policy.hyphenationMinBefore, policy.hyphenationMinAfter };
  std::vector<size_t> positions;
  size_t offset = 0;
  while (offset < utf8.size()) {
    const size_t length = sequenceLength(utf8, offset);
    if (!unicode::isHyphenationLetter(codePointAt(utf8, offset, length))) {
      offset += length;
      continue;
    }
    size_t end = offset;
    while (end < utf8.size()) {
      const size_t nextLength = sequenceLength(utf8, end);
      if (!unicode::isHyphenationLetter(codePointAt(utf8, end, nextLength))) break;
      end += nextLength;
    }
    unicode::hyphenateWord(utf8.substr(offset, end - offset), policy.language, positions, limits);
    for (const size_t position: positions) {
      result.push_back({ offset + position, false, true });
    }
    offset = end;
  }
}

} // namespace

std::vector<TailoredLineBreak> computeTailoredLineBreaks(const std::string_view utf8,
    const LineBreakPolicy& policy) {
  std::vector<TailoredLineBreak> result;
  const std::vector<unicode::LineBreakOpportunity> defaults =
    unicode::computeLineBreakOpportunities(utf8);
  result.reserve(defaults.size() + utf8.size() / 8 + 8);
  for (const unicode::LineBreakOpportunity& opportunity: defaults) {
    if (opportunity.action == unicode::BreakAction::MandatoryBreak) {
      result.push_back({ opportunity.byteOffset, true });
    } else if (policy.wordBreak != WordBreak::KeepAll ||
               !isCjkBoundary(utf8, opportunity.byteOffset)) {
      result.push_back({ opportunity.byteOffset, false });
    }
  }

  if (policy.wordBreak == WordBreak::BreakAll || policy.overflowWrap == OverflowWrap::Anywhere) {
    // Every grapheme boundary is a legal break-all opportunity, except sot/eot.
    unicode::GraphemeBreakState grapheme;
    size_t offset = 0;
    while (offset < utf8.size()) {
      const size_t length = sequenceLength(utf8, offset);
      const uint32_t codePoint = codePointAt(utf8, offset, length);
      if (grapheme.isBoundaryBefore(codePoint) && offset > 0 &&
          allowsIntraWordBreak(utf8, offset)) {
        result.push_back({ offset, false });
      }
      offset += length;
    }
  }

  const bool cjkLanguage = isCjkLanguage(policy.language);
  size_t offset = 0;
  while (offset < utf8.size()) {
    const size_t length = sequenceLength(utf8, offset);
    const uint32_t codePoint = codePointAt(utf8, offset, length);
    if (offset > 0) {
      // Normal/loose allow breaks before the CJK hyphen-like characters for
      // Chinese/Japanese; strict forbids them.
      if (policy.strictness != LineBreakStrictness::Strict && cjkLanguage &&
          isCjkHyphen(codePoint)) {
        result.push_back({ offset, false });
      }
      if (policy.strictness == LineBreakStrictness::Loose) {
        const auto currentClass = classAt(utf8, offset);
        const size_t previousStart = previousCodePointStart(utf8, offset);
        const auto previousClass = classAt(utf8, previousStart);
        const bool looseCjk = cjkLanguage &&
                              (isCenteredPunctuation(codePoint) ||
                                ((currentClass == unicode::LineBreakClass::PO ||
                                   currentClass == unicode::LineBreakClass::PR) &&
                                  (unicode::lineBreakFlags(codePoint) &
                                    unicode::kLineBreakFlagEastAsian) != 0));
        if (currentClass == unicode::LineBreakClass::CJ || isIterationMark(codePoint) ||
            looseCjk || (isLooseHyphen(codePoint) &&
                          previousClass == unicode::LineBreakClass::ID)) {
          result.push_back({ offset, false });
        }
        if (currentClass == unicode::LineBreakClass::IN &&
            previousClass == unicode::LineBreakClass::IN) {
          result.push_back({ offset, false });
        }
      }
    }
    offset += length;
  }

  if (policy.dictionary != nullptr) {
    for (const size_t offset: policy.dictionary->opportunities(utf8, policy.language)) {
      if (offset > 0 && offset < utf8.size()) result.push_back({ offset, false });
    }
  }

  if (policy.hyphenation && unicode::hasHyphenation(policy.language)) {
    appendHyphenationPoints(utf8, policy, result);
  }

  std::sort(result.begin(), result.end(),
    [](const TailoredLineBreak& left, const TailoredLineBreak& right) {
      if (left.byteOffset != right.byteOffset) return left.byteOffset < right.byteOffset;
      if (left.mandatory != right.mandatory) return left.mandatory;
      return left.hyphenate && !right.hyphenate;
    });
  result.erase(std::unique(result.begin(), result.end(),
                  [](const TailoredLineBreak& left, const TailoredLineBreak& right) {
                    return left.byteOffset == right.byteOffset;
                  }),
    result.end());
  return result;
}

} // namespace coral::ui
