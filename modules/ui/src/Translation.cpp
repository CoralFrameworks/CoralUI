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

#include <ui/Translation.h>

#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace coral::ui {

namespace {

std::string lowerCase(std::string_view text) {
  std::string result(text);
  for (char& character : result) {
    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }
  return result;
}

std::string upperCase(std::string_view text) {
  std::string result(text);
  for (char& character : result) {
    character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
  }
  return result;
}

std::string_view trim(std::string_view text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
    text.remove_prefix(1);
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.remove_suffix(1);
  }
  return text;
}

/** Expands the two escapes the format allows; any other backslash is kept verbatim. */
std::string unescape(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (value[index] == '\\' && index + 1 < value.size()) {
      const char next = value[index + 1];
      if (next == 'n') {
        result.push_back('\n');
        ++index;
        continue;
      }
      if (next == 't') {
        result.push_back('\t');
        ++index;
        continue;
      }
      if (next == '\\') {
        result.push_back('\\');
        ++index;
        continue;
      }
    }
    result.push_back(value[index]);
  }
  return result;
}

std::string formatCount(const double count) {
  char buffer[64];
  if (std::isfinite(count) && std::floor(count) == count && std::abs(count) < 1e15) {
    std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(count));
  } else {
    std::snprintf(buffer, sizeof(buffer), "%.2f", count);
  }
  return buffer;
}

/** Replaces every `{name}` whose name is given; an unknown placeholder stays visible. */
std::string substituteNamed(std::string text, const Translator::Replacements values) {
  for (const auto& [name, value] : values) {
    std::string token = "{";
    token += name;
    token += "}";
    std::size_t position = 0;
    while ((position = text.find(token, position)) != std::string::npos) {
      text.replace(position, token.size(), value);
      position += value.size();
    }
  }
  return text;
}

/** `{0}` placeholder substitution for the plural forms. */
std::string substituteCount(std::string text, const double count) {
  const std::string value = formatCount(count);
  std::size_t position = 0;
  while ((position = text.find("{0}", position)) != std::string::npos) {
    text.replace(position, 3, value);
    position += value.size();
  }
  return text;
}

bool isInteger(const double value) {
  return std::isfinite(value) && std::floor(value) == value;
}

} // namespace

Locale Locale::parse(const std::string_view tag) {
  Locale locale;
  std::size_t start = 0;
  bool first = true;
  while (start <= tag.size()) {
    const std::size_t separator = tag.find('-', start);
    const std::string_view part = separator == std::string_view::npos
      ? tag.substr(start)
      : tag.substr(start, separator - start);
    if (!part.empty()) {
      if (first) {
        locale.language = lowerCase(part);
        first = false;
      } else if (locale.region.empty()) {
        // A two-letter alphabetic subtag is the region; a numeric one is a UN M.49 code (`es-419`).
        // A longer alphabetic subtag is a script (`Hans`, `Latn`) and does not name a region.
        const bool alphabetic = part.size() == 2 &&
          std::isalpha(static_cast<unsigned char>(part[0])) != 0 &&
          std::isalpha(static_cast<unsigned char>(part[1])) != 0;
        const bool numeric = part.size() == 3 &&
          std::isdigit(static_cast<unsigned char>(part[0])) != 0 &&
          std::isdigit(static_cast<unsigned char>(part[1])) != 0 &&
          std::isdigit(static_cast<unsigned char>(part[2])) != 0;
        if (alphabetic || numeric) locale.region = upperCase(part);
      }
    }
    if (separator == std::string_view::npos) break;
    start = separator + 1;
  }
  if (locale.language.empty()) locale.language = "en";
  return locale;
}

std::string Locale::tag() const {
  return region.empty() ? language : language + "-" + region;
}

bool Locale::rtl() const noexcept {
  // The languages whose primary script is right to left. A language with both scripts (Azerbaijani,
  // Kurdish) is listed only when the RTL form is the common one; the tag's script subtag is ignored by
  // `parse`, so this is the same granularity the rest of the locale model uses.
  static constexpr const char* kRightToLeft[] = {
    "ar",  // Arabic
    "arc", // Aramaic
    "ckb", // Central Kurdish (Sorani)
    "dv",  // Divehi
    "fa",  // Persian
    "he",  // Hebrew
    "ku",  // Kurdish (Arabic script)
    "nqo", // N'Ko
    "ps",  // Pashto
    "sd",  // Sindhi
    "ug",  // Uyghur
    "ur",  // Urdu
    "yi",  // Yiddish
  };
  for (const char* language: kRightToLeft) {
    if (this->language == language) return true;
  }
  return false;
}

PluralCategory pluralCategory(const Locale& locale, const double rawCount) {
  const double count = std::fabs(rawCount);
  const std::string& language = locale.language;
  const auto integer = static_cast<long long>(count);
  const bool whole = isInteger(count);

  if (language == "zh" || language == "ja" || language == "ko" || language == "vi" ||
      language == "th" || language == "id" || language == "ms") {
    return PluralCategory::Other;
  }
  if (language == "ru" || language == "uk" || language == "be") {
    if (!whole) return PluralCategory::Other;
    const long long mod10 = integer % 10;
    const long long mod100 = integer % 100;
    if (mod10 == 1 && mod100 != 11) return PluralCategory::One;
    if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14)) return PluralCategory::Few;
    return PluralCategory::Many;
  }
  if (language == "pl") {
    if (!whole) return PluralCategory::Other;
    const long long mod10 = integer % 10;
    const long long mod100 = integer % 100;
    if (integer == 1) return PluralCategory::One;
    if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14)) return PluralCategory::Few;
    return PluralCategory::Many;
  }
  if (language == "ar") {
    if (!whole) return PluralCategory::Other;
    if (integer == 0) return PluralCategory::Zero;
    if (integer == 1) return PluralCategory::One;
    if (integer == 2) return PluralCategory::Two;
    if (integer >= 3 && integer <= 10) return PluralCategory::Few;
    if (integer >= 11 && integer <= 99) return PluralCategory::Many;
    return PluralCategory::Other;
  }
  if (language == "fr" || language == "pt") {
    // Both languages treat 0 and 1 as singular.
    if (whole && (integer == 0 || integer == 1)) return PluralCategory::One;
    return PluralCategory::Other;
  }
  if (whole && integer == 1) return PluralCategory::One;
  return PluralCategory::Other;
}

const char* pluralCategoryName(const PluralCategory category) {
  switch (category) {
    case PluralCategory::Zero: return "zero";
    case PluralCategory::One: return "one";
    case PluralCategory::Two: return "two";
    case PluralCategory::Few: return "few";
    case PluralCategory::Many: return "many";
    case PluralCategory::Other: return "other";
  }
  return "other";
}

Translator::Translator(Locale locale) : mLocale(std::move(locale)) {
  if (mLocale.language.empty()) mLocale.language = "en";
}

Translator::Translator(Locale locale, const std::string_view catalog) : Translator(std::move(locale)) {
  (void)load(catalog);
}

bool Translator::load(const std::string_view text, std::string* error) {
  // Parse into a scratch catalog first: a syntax error must leave the working catalog untouched.
  Translator candidate(mLocale);
  if (!candidate.addCatalog(text, error)) return false;
  mEntries = std::move(candidate.mEntries);
  return true;
}

bool Translator::addCatalog(const std::string_view text, std::string* error) {
  std::unordered_map<std::string, std::string> parsed;
  std::size_t lineNumber = 0;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t newline = text.find('\n', start);
    const std::string_view raw = newline == std::string_view::npos
      ? text.substr(start)
      : text.substr(start, newline - start);
    start = newline == std::string_view::npos ? text.size() + 1 : newline + 1;
    ++lineNumber;

    const std::string_view line = trim(raw);
    if (line.empty() || line.front() == '#') continue;
    const std::size_t separator = line.find('=');
    if (separator == std::string_view::npos) {
      if (error != nullptr) {
        *error = "line " + std::to_string(lineNumber) + ": expected key=value";
      }
      return false;
    }
    const std::string_view key = trim(line.substr(0, separator));
    if (key.empty()) {
      if (error != nullptr) *error = "line " + std::to_string(lineNumber) + ": empty key";
      return false;
    }
    parsed.emplace(std::string(key), unescape(trim(line.substr(separator + 1))));
  }
  // `addCatalog` merges so a locale catalog can sit on a base one; `load` cleared first.
  for (auto& entry : parsed) mEntries[entry.first] = std::move(entry.second);
  return true;
}

bool Translator::loadFile(const std::string& path, std::string* error) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    if (error != nullptr) *error = "cannot read " + path;
    return false;
  }
  std::ostringstream text;
  text << file.rdbuf();
  return load(text.str(), error);
}

bool Translator::addCatalogFile(const std::string& path, std::string* error) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    if (error != nullptr) *error = "cannot read " + path;
    return false;
  }
  std::ostringstream text;
  text << file.rdbuf();
  return addCatalog(text.str(), error);
}

void Translator::setLocale(Locale locale) {
  if (locale.language.empty()) locale.language = "en";
  mLocale = std::move(locale);
}

const std::string* Translator::find(const std::string_view key) const {
  const auto found = mEntries.find(std::string(key));
  return found == mEntries.end() ? nullptr : &found->second;
}

std::string Translator::translate(const std::string_view key) const {
  if (const std::string* value = find(key)) return *value;
  return std::string(key);
}

std::string Translator::translate(const std::string_view key, const Replacements values) const {
  return substituteNamed(translate(key), values);
}

std::string Translator::translatePlural(const std::string_view key, const double count) const {
  return translatePlural(key, count, Replacements{});
}

std::string Translator::translatePlural(const std::string_view key, const double count,
    const Replacements values) const {
  const std::string category = pluralCategoryName(pluralCategory(mLocale, count));
  const std::string suffixed = std::string(key) + "." + category;
  if (const std::string* value = find(suffixed)) {
    return substituteNamed(substituteCount(*value, count), values);
  }
  if (const std::string* other = find(std::string(key) + ".other")) {
    return substituteNamed(substituteCount(*other, count), values);
  }
  return substituteNamed(translate(key), values);
}

bool Translator::has(const std::string_view key) const {
  return find(key) != nullptr;
}

bool loadCatalogDirectory(Translator& translator, const std::string& directory, std::string* error) {
  const Locale& locale = translator.locale();
  const auto fileExists = [](const std::string& path) {
    std::ifstream probe(path);
    return probe.good();
  };
  const auto pathFor = [&directory](const std::string& tag) {
    std::string path = directory;
    if (!path.empty() && path.back() != '/' && path.back() != '\\') path += '/';
    return path + tag + ".catalog";
  };

  std::vector<std::string> tags = { "en" };
  if (locale.language != "en") tags.push_back(locale.language);
  if (!locale.region.empty()) tags.push_back(locale.language + "-" + locale.region);

  std::string tried;
  bool loaded = false;
  for (const std::string& tag : tags) {
    const std::string path = pathFor(tag);
    if (!fileExists(path)) {
      tried += (tried.empty() ? "" : ", ") + path;
      continue;
    }
    if (!translator.addCatalogFile(path, error)) return false;
    loaded = true;
  }
  if (!loaded) {
    if (error != nullptr) *error = "no catalog found (tried " + tried + ")";
  }
  return loaded;
}

} // namespace coral::ui
