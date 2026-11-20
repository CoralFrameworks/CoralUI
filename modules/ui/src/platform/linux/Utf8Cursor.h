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

#ifndef CORAL_UI_UTF8CURSOR_H
#define CORAL_UI_UTF8CURSOR_H

#include <cstddef>
#include <string>

namespace coral::ui::detail {

/**
 * Converts between the UTF-8 byte offsets `TextInput` stores and the character offsets the IME
 * protocols use. A byte offset that lands in the middle of a sequence is treated as the start of
 * the next character, which keeps a malformed string from producing a negative range.
 */
inline size_t utf8CharacterCount(const std::string& text, const size_t byteLimit) {
  size_t count = 0;
  size_t index = 0;
  while (index < text.size() && index < byteLimit) {
    ++count;
    ++index;
    while (index < text.size() && (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80) ++index;
  }
  return count;
}

/** Byte offset of character @p character, clamped to the string. */
inline size_t utf8ByteOffset(const std::string& text, const size_t character) {
  size_t index = 0;
  size_t count = 0;
  while (index < text.size() && count < character) {
    ++index;
    while (index < text.size() && (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80) ++index;
    ++count;
  }
  return index;
}

} // namespace coral::ui::detail

#endif // CORAL_UI_UTF8CURSOR_H
