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

#include <ui/ThemeFile.h>

#include <array>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace coral::ui {

namespace {

std::string_view trim(std::string_view text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
    text.remove_prefix(1);
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.remove_suffix(1);
  }
  return text;
}

bool parseHexDigit(const char character, uint8_t* out) {
  if (character >= '0' && character <= '9') {
    *out = static_cast<uint8_t>(character - '0');
    return true;
  }
  if (character >= 'a' && character <= 'f') {
    *out = static_cast<uint8_t>(character - 'a' + 10);
    return true;
  }
  if (character >= 'A' && character <= 'F') {
    *out = static_cast<uint8_t>(character - 'A' + 10);
    return true;
  }
  return false;
}

/** @return true when @p text is `#RRGGBB` or `#RRGGBBAA` and fills @p out. */
bool parseColor(const std::string_view text, Color* out) {
  if (text.size() != 7 && text.size() != 9) return false;
  if (text.front() != '#') return false;
  uint8_t bytes[4] = { 0, 0, 0, 255 };
  for (size_t index = 0; index < (text.size() - 1) / 2; ++index) {
    uint8_t high = 0;
    uint8_t low = 0;
    if (!parseHexDigit(text[1 + index * 2], &high) || !parseHexDigit(text[2 + index * 2], &low)) {
      return false;
    }
    bytes[index] = static_cast<uint8_t>(high * 16 + low);
  }
  *out = Color::MakeRGBA8(bytes[0], bytes[1], bytes[2], bytes[3]);
  return true;
}

std::string hexOf(const Color& color) {
  const std::array<uint8_t, 4> bytes = color.toR8G8B8A8();
  char buffer[10];
  std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X", bytes[0], bytes[1], bytes[2], bytes[3]);
  return buffer;
}

/** The colour keys share their names with `ThemeColorRole`, minus the nesting for controls. */
bool applyColorKey(Theme* theme, const std::string_view key, const Color& color) {
  if (key == "background") theme->background = color;
  else if (key == "surface") theme->surface = color;
  else if (key == "surfaceRaised") theme->surfaceRaised = color;
  else if (key == "text") theme->text = color;
  else if (key == "textMuted") theme->textMuted = color;
  else if (key == "accent") theme->accent = color;
  else if (key == "scrim") theme->scrim = color;
  else if (key == "controls.background") theme->controls.background = color;
  else if (key == "controls.backgroundHover") theme->controls.backgroundHover = color;
  else if (key == "controls.backgroundPressed") theme->controls.backgroundPressed = color;
  else if (key == "controls.border") theme->controls.border = color;
  else if (key == "controls.borderFocused") theme->controls.borderFocused = color;
  else if (key == "controls.foreground") theme->controls.foreground = color;
  else if (key == "controls.muted") theme->controls.muted = color;
  else if (key == "controls.accent") theme->controls.accent = color;
  else if (key == "controls.disabled") theme->controls.disabled = color;
  else if (key == "controls.track") theme->controls.track = color;
  else if (key == "controls.knob") theme->controls.knob = color;
  else return false;
  return true;
}

} // namespace

std::optional<Theme> parseTheme(const std::string_view text, const Theme& base, std::string* error) {
  Theme theme = base;
  size_t lineNumber = 0;
  size_t start = 0;
  while (start <= text.size()) {
    const size_t newline = text.find('\n', start);
    const std::string_view raw = newline == std::string_view::npos
      ? text.substr(start)
      : text.substr(start, newline - start);
    start = newline == std::string_view::npos ? text.size() + 1 : newline + 1;
    ++lineNumber;

    std::string_view line = trim(raw);
    if (line.empty() || line.front() == '#') continue;
    const size_t separator = line.find('=');
    if (separator == std::string_view::npos) {
      if (error != nullptr) *error = "line " + std::to_string(lineNumber) + ": expected key=value";
      return std::nullopt;
    }
    const std::string_view key = trim(line.substr(0, separator));
    const std::string_view value = trim(line.substr(separator + 1));
    if (key.empty() || value.empty()) {
      if (error != nullptr) *error = "line " + std::to_string(lineNumber) + ": empty key or value";
      return std::nullopt;
    }

    if (key == "focusRingWidth") {
      // Apple's libc++ lacks the floating-point from_chars overload; parse with strtof and verify
      // that the whole value was consumed.
      const std::string text(value);
      char* end = nullptr;
      errno = 0;
      const float width = std::strtof(text.c_str(), &end);
      if (end != text.c_str() + text.size() || errno == ERANGE || !std::isfinite(width) ||
          width < 0.0f) {
        if (error != nullptr) *error = "line " + std::to_string(lineNumber) + ": invalid number";
        return std::nullopt;
      }
      theme.focusRingWidth = width;
      continue;
    }

    Color color;
    if (!parseColor(value, &color)) {
      if (error != nullptr) {
        *error = "line " + std::to_string(lineNumber) + ": '" + std::string(value) +
          "' is not #RRGGBB or #RRGGBBAA";
      }
      return std::nullopt;
    }
    if (!applyColorKey(&theme, key, color)) {
      if (error != nullptr) *error = "line " + std::to_string(lineNumber) + ": unknown key '" +
        std::string(key) + "'";
      return std::nullopt;
    }
  }
  return theme;
}

std::string formatTheme(const Theme& theme) {
  std::ostringstream out;
  out << "# CoralUI theme\n";
  out << "background=" << hexOf(theme.background) << '\n';
  out << "surface=" << hexOf(theme.surface) << '\n';
  out << "surfaceRaised=" << hexOf(theme.surfaceRaised) << '\n';
  out << "text=" << hexOf(theme.text) << '\n';
  out << "textMuted=" << hexOf(theme.textMuted) << '\n';
  out << "accent=" << hexOf(theme.accent) << '\n';
  out << "scrim=" << hexOf(theme.scrim) << '\n';
  out << "controls.background=" << hexOf(theme.controls.background) << '\n';
  out << "controls.backgroundHover=" << hexOf(theme.controls.backgroundHover) << '\n';
  out << "controls.backgroundPressed=" << hexOf(theme.controls.backgroundPressed) << '\n';
  out << "controls.border=" << hexOf(theme.controls.border) << '\n';
  out << "controls.borderFocused=" << hexOf(theme.controls.borderFocused) << '\n';
  out << "controls.foreground=" << hexOf(theme.controls.foreground) << '\n';
  out << "controls.muted=" << hexOf(theme.controls.muted) << '\n';
  out << "controls.accent=" << hexOf(theme.controls.accent) << '\n';
  out << "controls.disabled=" << hexOf(theme.controls.disabled) << '\n';
  out << "controls.track=" << hexOf(theme.controls.track) << '\n';
  out << "controls.knob=" << hexOf(theme.controls.knob) << '\n';
  out << "focusRingWidth=" << theme.focusRingWidth << '\n';
  return out.str();
}

} // namespace coral::ui
