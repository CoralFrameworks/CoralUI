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

#ifndef CORAL_UI_THEMEFILE_H
#define CORAL_UI_THEMEFILE_H

#include <ui/Theme.h>

#include <optional>
#include <string>
#include <string_view>

namespace coral::ui {

/**
 * Parses a theme from the line-oriented text form.
 *
 * The format is deliberately small: one `key=value` per line, `#` starts a comment, blank lines are
 * ignored. Colours are `#RRGGBB` or `#RRGGBBAA`; `focusRingWidth` is a number. Every key is a field the
 * `Theme` struct already has, so a designer can change a colour without a rebuild and the code has no
 * second vocabulary to learn:
 *
 * ```
 * # coral.theme
 * background=#181A20
 * surface=#23262E
 * accent=#569CFF
 * controls.background=#23262E
 * focusRingWidth=1.5
 * ```
 *
 * @param text  the file contents
 * @param base  the theme the keys are applied on top of (defaults to the dark skin)
 * @param error when non-null and the text does not parse, receives a line-numbered reason
 * @return the theme, or `std::nullopt` when a line is malformed or a key is unknown
 */
[[nodiscard]] std::optional<Theme> parseTheme(std::string_view text, const Theme& base = Theme::dark(),
  std::string* error = nullptr);

/** Serialises a theme back to the text form; the parser reads what this writes. */
[[nodiscard]] std::string formatTheme(const Theme& theme);

} // namespace coral::ui

#endif // CORAL_UI_THEMEFILE_H
