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

/*
 * Generated from the ICU Thai/Khmer/Lao/Burmese break dictionaries.
 * DO NOT EDIT BY HAND; see tools/unicode/generate_word_tries.py.
 */

#ifndef CORAL_UI_UNICODE_WORDTRIES_GENERATED_H
#define CORAL_UI_UNICODE_WORDTRIES_GENERATED_H

#include "unicode/WordTrie.h"

namespace coral::ui::unicode {

[[nodiscard]] const WordTrie& thaiTrie();
[[nodiscard]] const WordTrie& khmerTrie();
[[nodiscard]] const WordTrie& laoTrie();
[[nodiscard]] const WordTrie& burmeseTrie();

} // namespace coral::ui::unicode

#endif // CORAL_UI_UNICODE_WORDTRIES_GENERATED_H
