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

#include "unicode/WordTrie.h"
#include "unicode/WordTries.generated.h"

#include <memory>

namespace coral::ui {

namespace {

bool languageMatches(const std::string_view language, const char first, const char second) {
  return !language.empty() &&
         language.size() >= 2 && language[0] == first && language[1] == second;
}

} // namespace

std::vector<size_t> ThaiBreakDictionary::opportunities(const std::string_view text,
    const std::string_view language) const {
  if (!languageMatches(language, 't', 'h')) return {};
  return unicode::scanWordTrie(text, unicode::thaiTrie());
}

std::vector<size_t> KhmerBreakDictionary::opportunities(const std::string_view text,
    const std::string_view language) const {
  if (!languageMatches(language, 'k', 'm')) return {};
  return unicode::scanWordTrie(text, unicode::khmerTrie());
}

std::vector<size_t> LaoBreakDictionary::opportunities(const std::string_view text,
    const std::string_view language) const {
  if (!languageMatches(language, 'l', 'o')) return {};
  return unicode::scanWordTrie(text, unicode::laoTrie());
}

std::vector<size_t> BurmeseBreakDictionary::opportunities(const std::string_view text,
    const std::string_view language) const {
  if (!languageMatches(language, 'm', 'y')) return {};
  return unicode::scanWordTrie(text, unicode::burmeseTrie());
}

std::shared_ptr<const BreakDictionary> makeThaiBreakDictionary() {
  return std::make_shared<const ThaiBreakDictionary>();
}

std::shared_ptr<const BreakDictionary> makeKhmerBreakDictionary() {
  return std::make_shared<const KhmerBreakDictionary>();
}

std::shared_ptr<const BreakDictionary> makeLaoBreakDictionary() {
  return std::make_shared<const LaoBreakDictionary>();
}

std::shared_ptr<const BreakDictionary> makeBurmeseBreakDictionary() {
  return std::make_shared<const BurmeseBreakDictionary>();
}

} // namespace coral::ui
