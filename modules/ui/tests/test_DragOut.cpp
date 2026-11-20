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

#if defined(_WIN32)
#include <ui/Win32DragOut.h>

#include <windows.h>
#include <shellapi.h>

#include <cstring>
#include <string>
#include <vector>

TEST(UiDragOut, TextPayloadBecomesAUnicodeTextBlock) {
  const auto formats = coral::ui::win32DragFormats(coral::ui::DragPayload::text("hello"));
  ASSERT_EQ(formats.size(), 1u);
  EXPECT_EQ(formats.front().format, static_cast<unsigned int>(CF_UNICODETEXT));
  EXPECT_TRUE(formats.front().wide);
  ASSERT_EQ(formats.front().data.size(), 6 * sizeof(wchar_t)) << "5 characters plus the terminator";
  std::wstring text(reinterpret_cast<const wchar_t*>(formats.front().data.data()), 5);
  EXPECT_EQ(text, L"hello");
  EXPECT_EQ(formats.front().data[5 * sizeof(wchar_t)], '\0') << "the block is NUL-terminated";
}

TEST(UiDragOut, FilePayloadBecomesADropFilesBlock) {
  struct DropFilesHeader {
    DWORD pFiles;
    POINT pt;
    BOOL fNC;
    BOOL fWide;
  };
  const auto formats = coral::ui::win32DragFormats(
    coral::ui::DragPayload::files({ "C:\\a.txt", "D:\\b.txt" }));
  ASSERT_EQ(formats.size(), 1u);
  EXPECT_EQ(formats.front().format, static_cast<unsigned int>(CF_HDROP));
  DropFilesHeader header{};
  ASSERT_GE(formats.front().data.size(), sizeof(header));
  std::memcpy(&header, formats.front().data.data(), sizeof(header));
  EXPECT_EQ(header.pFiles, sizeof(DropFilesHeader));
  EXPECT_EQ(header.fWide, TRUE);
  const auto* paths = reinterpret_cast<const wchar_t*>(formats.front().data.data() + sizeof(header));
  EXPECT_STREQ(paths, L"C:\\a.txt");
  EXPECT_STREQ(paths + std::wstring(L"C:\\a.txt").size() + 1, L"D:\\b.txt");
}

TEST(UiDragOut, CustomPayloadUsesARegisteredFormat) {
  auto payload = coral::ui::DragPayload::custom("application/x-coral-clip", "abc");
  payload.setSecondary("text/plain", "abc");
  const auto formats = coral::ui::win32DragFormats(payload);
  ASSERT_EQ(formats.size(), 2u) << "the custom type and the secondary representation";
  EXPECT_NE(formats[0].format, 0u);
  EXPECT_EQ(formats[0].data, "abc");
  EXPECT_EQ(formats[1].format, ::RegisterClipboardFormatW(L"text/plain"));
  EXPECT_EQ(formats[1].data, "abc");
}

TEST(UiDragOut, EmptyPayloadAdvertisesNothing) {
  EXPECT_TRUE(coral::ui::win32DragFormats(coral::ui::DragPayload{}).empty());
}
TEST(UiDragOut, DataObjectAnswersGetDataFromTheSerialisedFormats) {
  auto* object = coral::ui::Win32DragDataObject::create(coral::ui::DragPayload::text("hello"));
  ASSERT_NE(object, nullptr);
  FORMATETC text{ static_cast<CLIPFORMAT>(CF_UNICODETEXT), nullptr, DVASPECT_CONTENT, -1,
    TYMED_HGLOBAL };
  EXPECT_EQ(object->QueryGetData(&text), S_OK);

  FORMATETC files{ static_cast<CLIPFORMAT>(CF_HDROP), nullptr, DVASPECT_CONTENT, -1,
    TYMED_HGLOBAL };
  EXPECT_EQ(object->QueryGetData(&files), DV_E_FORMATETC) << "the payload is text only";

  STGMEDIUM medium{};
  ASSERT_EQ(object->GetData(&text, &medium), S_OK);
  EXPECT_EQ(medium.tymed, static_cast<DWORD>(TYMED_HGLOBAL));
  const auto* data = static_cast<const wchar_t*>(GlobalLock(medium.hGlobal));
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(std::wstring(data), L"hello");
  GlobalUnlock(medium.hGlobal);
  ReleaseStgMedium(&medium);
  EXPECT_EQ(object->Release(), 0u);
}

TEST(UiDragOut, DataObjectRejectsANonGlobalMedium) {
  auto* object = coral::ui::Win32DragDataObject::create(coral::ui::DragPayload::text("x"));
  ASSERT_NE(object, nullptr);
  FORMATETC bitmap{ static_cast<CLIPFORMAT>(CF_UNICODETEXT), nullptr, DVASPECT_CONTENT, -1,
    TYMED_GDI };
  EXPECT_EQ(object->QueryGetData(&bitmap), DV_E_FORMATETC);
  EXPECT_EQ(object->Release(), 0u);
}
TEST(UiDragOut, DropEffectsMapBothWays) {
  using coral::ui::DragEffect;
  EXPECT_EQ(coral::ui::win32DropEffect(DragEffect::None), static_cast<unsigned long>(DROPEFFECT_NONE));
  EXPECT_EQ(coral::ui::win32DropEffect(DragEffect::Copy), static_cast<unsigned long>(DROPEFFECT_COPY));
  EXPECT_EQ(coral::ui::win32DropEffect(DragEffect::Move), static_cast<unsigned long>(DROPEFFECT_MOVE));
  EXPECT_EQ(coral::ui::win32DropEffect(DragEffect::Link), static_cast<unsigned long>(DROPEFFECT_LINK));
  for (const DragEffect effect: { DragEffect::None, DragEffect::Copy, DragEffect::Move, DragEffect::Link }) {
    EXPECT_EQ(coral::ui::dragEffectFromWin32(coral::ui::win32DropEffect(effect)), effect);
  }
  EXPECT_EQ(coral::ui::dragEffectFromWin32(DROPEFFECT_COPY | DROPEFFECT_MOVE), DragEffect::Move)
    << "a combined mask resolves to the most specific effect";
}
TEST(UiDragOut, DragActionFollowsEscapeAndButtonState) {
  using coral::ui::Win32DragAction;
  EXPECT_EQ(coral::ui::win32DragAction(true, true, false), Win32DragAction::Cancel)
    << "Escape cancels even while the button is held";
  EXPECT_EQ(coral::ui::win32DragAction(false, true, false), Win32DragAction::Continue);
  EXPECT_EQ(coral::ui::win32DragAction(false, false, true), Win32DragAction::Continue)
    << "the secondary button keeps the drag alive";
  EXPECT_EQ(coral::ui::win32DragAction(false, false, false), Win32DragAction::Drop)
    << "releasing both buttons drops";
}
#endif // _WIN32
