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

#ifndef CORAL_UI_SPELLCHECK_H
#define CORAL_UI_SPELLCHECK_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace coral::ui {

/** A misspelled byte range in the UTF-8 text passed to `spellCheck`. */
struct SpellIssue {
  std::size_t begin = 0;
  std::size_t end = 0;
};

/**
 * Returns the misspelled ranges of @p utf8 using the platform spell checker.
 *
 * macOS uses `NSSpellChecker`; other platforms currently return no issues. The ranges are byte
 * offsets into @p utf8, so a caller can map them onto a text control's storage without knowing the
 * platform's UTF-16 units. An empty @p language asks the checker for its current language.
 */
[[nodiscard]] std::vector<SpellIssue> spellCheck(std::string_view utf8,
  std::string_view language = {});

/** Replacement candidates for a misspelled word (empty when the platform has none). */
[[nodiscard]] std::vector<std::string> spellSuggestions(std::string_view word,
  std::string_view language = {});

/** The platform's single autocorrect replacement for @p word, or empty when it has none. */
[[nodiscard]] std::string spellCorrection(std::string_view word,
  std::string_view language = {});

[[nodiscard]] bool supportsSpellChecking() noexcept;

} // namespace coral::ui

#endif // CORAL_UI_SPELLCHECK_H
