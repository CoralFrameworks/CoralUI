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

#ifndef CORAL_UI_TRANSLATION_H
#define CORAL_UI_TRANSLATION_H

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace coral::ui {

/** A language tag, split the way the catalog and the plural rules need it. */
struct Locale {
  /** Lower-case language subtag, e.g. `en`, `zh`, `ru`. */
  std::string language = "en";
  /** Upper-case region subtag, optional: `US`, `CN`, `RU`. */
  std::string region;

  /** Parses `en`, `en-US`, `zh-Hans-CN` (the script subtag is ignored by the rules here). */
  [[nodiscard]] static Locale parse(std::string_view tag);
  /** `en` or `en-US`. */
  [[nodiscard]] std::string tag() const;
  /**
   * True for a language written right to left.
   *
   * The list is the RTL scripts' languages, not a property of the tag's region: `ar-EG` is RTL and
   * `en-EG` is not. A UI mirrors its layout from this, and a text layer that shapes bidi runs reads the
   * same answer.
   */
  [[nodiscard]] bool rtl() const noexcept;

  /**
   * The locale the operating system is set to.
   *
   * Windows answers `GetUserDefaultLocaleName`; a POSIX system is read from `LC_ALL`, `LC_MESSAGES`
   * or `LANG` (in that order). A machine that reports `C`/`POSIX` or nothing keeps `Locale{}`, which
   * is English -- the catalogs' fallback.
   */
  [[nodiscard]] static Locale system();
};

/** CLDR plural categories, in the order the CLDR lists them. */
enum class PluralCategory {
  Zero,
  One,
  Two,
  Few,
  Many,
  Other,
};

/**
 * The plural category @p count falls into for @p locale.
 *
 * This covers the categories the shipped catalogs need (`en`, `zh`/`ja`/`ko`, `ru`, `pl`, `ar`) and
 * falls back to the one/other split for every other language, which is what a catalog that only
 * provides `.one` and `.other` can answer. Counts are taken as their absolute value, and integers
 * and fractions are distinguished only where the CLDR rule for that language depends on it (`pl`).
 */
[[nodiscard]] PluralCategory pluralCategory(const Locale& locale, double count);

/** The catalog key suffix for @p category: `one`, `few`, ... `other`. */
[[nodiscard]] const char* pluralCategoryName(PluralCategory category);

/**
 * A message catalog: `key=value` lines, `#` starts a comment, blank lines are ignored, and `\n`/`\t`
 * are the only escapes (so a value can hold a line break without the file becoming a list).
 *
 * Plural entries are keys with a category suffix:
 *
 * ```
 * # coral.catalog
 * editor.title=Coral Editor
 * timeline.clips.one={0} clip
 * timeline.clips.other={0} clips
 * ```
 *
 * `{0}` is replaced by the count in `translatePlural`. A missing key returns the key itself, so a
 * missing translation is visible in the UI instead of blank.
 */
class Translator {
 public:
  /**
   * A translator with no catalog, for the locale the application names.
   *
   * The default is the operating system's locale, so a control that asks for a label without a catalog
   * still pluralizes and formats for the user's language. Pass a `Locale` when the app has its own
   * setting; the catalogs' fallback is English either way.
   */
  explicit Translator(Locale locale = Locale::system());
  Translator(Locale locale, std::string_view catalog);

  /** Replaces the catalog; on a syntax error nothing is changed and @p error gets the line. */
  bool load(std::string_view text, std::string* error = nullptr);
  /** Loads a catalog from a file; false when it cannot be read (the catalog is left unchanged). */
  bool loadFile(const std::string& path, std::string* error = nullptr);

  /**
   * Merges a catalog into the current one without clearing it.
   *
   * This is how a fallback chain is built: load the base language first, then the locale catalog, and
   * a key the locale file does not translate keeps the base text. Later additions win, so the order is
   * base -> locale, and the same file may be re-added to override one string at run time.
   */
  bool addCatalog(std::string_view text, std::string* error = nullptr);
  /** Merges a catalog file; false when it cannot be read (the catalog is left unchanged). */
  bool addCatalogFile(const std::string& path, std::string* error = nullptr);
  /** Drops every entry; the locale and the plural rules stay. */
  void clearCatalog() noexcept {
    mEntries.clear();
  }

  void setLocale(Locale locale);
  [[nodiscard]] const Locale& locale() const noexcept {
    return mLocale;
  }

  /** A named placeholder value: `{name}` in the catalog text is replaced by @p second. */
  using Replacement = std::pair<std::string_view, std::string_view>;
  using Replacements = std::initializer_list<Replacement>;

  /** The translation for @p key, or @p key itself when the catalog has no entry. */
  [[nodiscard]] std::string translate(std::string_view key) const;
  /**
   * Like `translate`, then replaces every `{name}` placeholder whose name appears in @p values.
   *
   * An unknown placeholder is left untouched on purpose: a missing argument shows up in the UI as
   * `{count}` instead of silently disappearing.
   */
  [[nodiscard]] std::string translate(std::string_view key, Replacements values) const;
  /**
   * The plural form for @p count: looks for `key.<category>`, then `key.other`, then @p key; `{0}` in
   * the value is replaced by @p count formatted without a fraction when it is integral.
   */
  [[nodiscard]] std::string translatePlural(std::string_view key, double count) const;
  /** Like the count overload, and also replaces the named @p values. */
  [[nodiscard]] std::string translatePlural(std::string_view key, double count,
    Replacements values) const;
  /** Like `translatePlural` with an integer count. */
  [[nodiscard]] std::string translatePlural(std::string_view key, int count) const {
    return translatePlural(key, static_cast<double>(count));
  }

  [[nodiscard]] bool has(std::string_view key) const;
  [[nodiscard]] std::size_t size() const noexcept {
    return mEntries.size();
  }

 private:
  [[nodiscard]] const std::string* find(std::string_view key) const;

  Locale mLocale;
  std::unordered_map<std::string, std::string> mEntries;
};

/**
 * Loads the catalog chain for the translator locale from @p directory.
 *
 * The files are named by tag: `en.catalog`, `<language>.catalog`, `<language>-<region>.catalog`. `en`
 * is the base, then the language, then the regional variant; each file is merged over the previous one
 * with `Translator::addCatalog`, so a locale that only translates a few strings keeps the base text for
 * the rest. A missing file is skipped; the call returns false (with the tried names in @p error) only
 * when none of them could be read. The translator locale is what selects the chain, so the caller sets
 * it before calling.
 */
bool loadCatalogDirectory(Translator& translator, const std::string& directory,
  std::string* error = nullptr);

} // namespace coral::ui

#endif // CORAL_UI_TRANSLATION_H
