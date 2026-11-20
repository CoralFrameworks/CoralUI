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

#include <gtest/gtest.h>

#if defined(__linux__)

#include "platform/linux/Utf8Cursor.h"

namespace {

using coral::ui::detail::utf8ByteOffset;
using coral::ui::detail::utf8CharacterCount;

} // namespace

TEST(LinuxIme, Utf8CharacterOffsetsHandleAsciiAndCjk) {
  const std::string ascii = "abc";
  EXPECT_EQ(utf8CharacterCount(ascii, ascii.size()), 3u);
  EXPECT_EQ(utf8ByteOffset(ascii, 2), 2u);

  // Three CJK characters are nine bytes; the IME counts three characters.
  const std::string cjk = "中文测";
  EXPECT_EQ(cjk.size(), 9u);
  EXPECT_EQ(utf8CharacterCount(cjk, cjk.size()), 3u);
  EXPECT_EQ(utf8ByteOffset(cjk, 1), 3u);
  EXPECT_EQ(utf8ByteOffset(cjk, 2), 6u);
  EXPECT_EQ(utf8ByteOffset(cjk, 3), 9u);
}

TEST(LinuxIme, Utf8CharacterOffsetsHandleEmojiAndByteLimits) {
  const std::string mixed = "a中🙂";
  EXPECT_EQ(utf8CharacterCount(mixed, mixed.size()), 3u);
  EXPECT_EQ(utf8ByteOffset(mixed, 1), 1u);
  EXPECT_EQ(utf8ByteOffset(mixed, 2), 4u);
  EXPECT_EQ(utf8ByteOffset(mixed, 3), mixed.size());

  // A caret in the middle of a multi-byte sequence clamps to the next character boundary instead
  // of producing a negative or overlapping delete range.
  EXPECT_EQ(utf8CharacterCount(mixed, 2), 2u);
  EXPECT_EQ(utf8ByteOffset(mixed, 99), mixed.size());
}

TEST(LinuxIme, DeleteSurroundingConvertsCharactersToBytes) {
  // Mirrors the platform conversion: delete two characters before the caret in a CJK string.
  const std::string text = "中文测";
  const size_t cursorByte = text.size();
  const size_t cursorChars = utf8CharacterCount(text, cursorByte);
  const size_t startChar = cursorChars > 2 ? cursorChars - 2 : 0;
  const size_t startByte = utf8ByteOffset(text, startChar);
  EXPECT_EQ(startByte, 3u);
  EXPECT_EQ(cursorByte - startByte, 6u);
}

#endif // __linux__
