/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file SystemLocale.cpp
 *
 * Where the operating system's locale answer comes from. It is a separate translation unit because it
 * is the only part of the translation stack that talks to the OS: `Locale::parse`, the plural rules
 * and the catalogs stay platform-free and therefore testable without one.
 */

#include <ui/Translation.h>

#include <cstdlib>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace coral::ui {

namespace {

#if defined(_WIN32)
/** @return the UTF-8 form of a wide string, or an empty string when it cannot be converted. */
[[nodiscard]] std::string toUtf8(const wchar_t* text) {
  if (text == nullptr || *text == L'\0') return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (size <= 1) return {};
  std::string result(static_cast<std::size_t>(size - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size - 1, nullptr, nullptr);
  return result;
}
#endif

#if !defined(_WIN32)
/**
 * @return the locale named by a POSIX environment variable, or empty when it names none.
 *
 * A value is `ll_CC.encoding@modifier`; the encoding and modifier are not part of the tag. `C` and
 * `POSIX` mean "no locale" and are treated as absent, which is what lets the caller fall through to
 * the next variable and ultimately to English.
 */
[[nodiscard]] std::string fromPosixEnvironment() {
  for (const char* name: { "LC_ALL", "LC_MESSAGES", "LANG" }) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') continue;
    std::string tag(value);
    if (const std::size_t cut = tag.find_first_of("."); cut != std::string::npos) tag.resize(cut);
    if (const std::size_t cut = tag.find('@'); cut != std::string::npos) tag.resize(cut);
    for (char& character: tag) {
      if (character == '_') character = '-';
    }
    if (tag == "C" || tag == "POSIX" || tag.empty()) continue;
    return tag;
  }
  return {};
}
#endif

} // namespace

Locale Locale::system() {
#if defined(_WIN32)
  // LOCALE_NAME_MAX_LENGTH is the documented bound for a locale name, including the terminator.
  wchar_t buffer[LOCALE_NAME_MAX_LENGTH] = {};
  if (GetUserDefaultLocaleName(buffer, LOCALE_NAME_MAX_LENGTH) > 1) {
    const std::string tag = toUtf8(buffer);
    if (!tag.empty()) return parse(tag);
  }
  return Locale{};
#else
  const std::string tag = fromPosixEnvironment();
  return tag.empty() ? Locale{} : parse(tag);
#endif
}

} // namespace coral::ui
