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

#include "../../ClipboardPlatform.h"

#include <ui/Clipboard.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace coral::ui::detail {

namespace {

/** Closes the Win32 clipboard in every return path. */
class ClipboardOpenScope final {
public:
  ClipboardOpenScope() : mOpen(OpenClipboard(nullptr) != FALSE) {
  }

  ~ClipboardOpenScope() {
    if (mOpen) {
      CloseClipboard();
    }
  }

  ClipboardOpenScope(const ClipboardOpenScope&) = delete;
  ClipboardOpenScope& operator=(const ClipboardOpenScope&) = delete;

  [[nodiscard]] bool isOpen() const noexcept {
    return mOpen;
  }

private:
  bool mOpen;
};

/** Locks an HGLOBAL and unlocks it on destruction. */
class GlobalLockScope final {
public:
  explicit GlobalLockScope(const HGLOBAL handle) : mHandle(handle) {
    if (mHandle != nullptr) {
      mData = GlobalLock(mHandle);
    }
  }

  ~GlobalLockScope() {
    if (mData != nullptr) {
      GlobalUnlock(mHandle);
    }
  }

  GlobalLockScope(const GlobalLockScope&) = delete;
  GlobalLockScope& operator=(const GlobalLockScope&) = delete;

  [[nodiscard]] bool valid() const noexcept {
    return mData != nullptr;
  }

  template<class T>
  [[nodiscard]] T* as() const noexcept {
    return static_cast<T*>(mData);
  }

private:
  HGLOBAL mHandle = nullptr;
  void* mData = nullptr;
};

/** Frees an HGLOBAL unless ownership was transferred to the clipboard. */
class GlobalFreeScope final {
public:
  explicit GlobalFreeScope(const HGLOBAL handle) : mHandle(handle) {
  }

  ~GlobalFreeScope() {
    if (mHandle != nullptr) {
      GlobalFree(mHandle);
    }
  }

  GlobalFreeScope(const GlobalFreeScope&) = delete;
  GlobalFreeScope& operator=(const GlobalFreeScope&) = delete;

  [[nodiscard]] HGLOBAL release() noexcept {
    const HGLOBAL handle = mHandle;
    mHandle = nullptr;
    return handle;
  }

private:
  HGLOBAL mHandle;
};

std::string utf16ToUtf8(const wchar_t* text, const size_t length) {
  if (text == nullptr || length == 0) {
    return {};
  }
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(length), nullptr, 0,
    nullptr, nullptr);
  if (bytes <= 0) {
    return {};
  }
  std::string result(static_cast<size_t>(bytes), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(length), result.data(), bytes, nullptr,
    nullptr);
  return result;
}

std::wstring utf8ToUtf16(const std::string_view text) {
  if (text.empty()) {
    return {};
  }
  const int characters = MultiByteToWideChar(CP_UTF8, 0, text.data(),
    static_cast<int>(text.size()), nullptr, 0);
  if (characters <= 0) {
    return {};
  }
  std::wstring result(static_cast<size_t>(characters), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(),
    characters);
  return result;
}

/** The CF_HDROP header (same layout as shellapi's DROPFILES, which is not always visible). */
struct DropFilesHeader {
  DWORD pFiles;
  POINT pt;
  BOOL fNC;
  BOOL fWide;
};

class Win32Clipboard final : public Clipboard {
public:
  [[nodiscard]] std::string get(const std::string& mime) const override {
    if (mime == "text/plain") return readText();
    if (mime == "text/uri-list") return readFiles();
    if (mime == "text/html") return readHtml();
    if (mime == "image/png") return readPng();
    return {};
  }

  bool set(const std::string& mime, std::string data) override {
    if (mime == "text/plain") {
      writeText(std::move(data));
      notifyChanged();
      return true;
    }
    if (mime == "text/uri-list") {
      std::vector<std::string> paths;
      size_t offset = 0;
      while (offset < data.size()) {
        size_t end = data.find('\n', offset);
        if (end == std::string::npos) end = data.size();
        std::string line = data.substr(offset, end - offset);
        offset = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && line.front() != '#') paths.push_back(std::move(line));
      }
      writeFiles(paths);
      notifyChanged();
      return true;
    }
    if (mime == "text/html") {
      writeHtml(std::move(data));
      notifyChanged();
      return true;
    }
    if (mime == "image/png") {
      writePng(std::move(data));
      notifyChanged();
      return true;
    }
    return false;
  }

  [[nodiscard]] std::vector<std::string> formats() const override {
    std::vector<std::string> result;
    if (IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE) result.emplace_back("text/plain");
    if (IsClipboardFormatAvailable(CF_HDROP) != FALSE) result.emplace_back("text/uri-list");
    if (IsClipboardFormatAvailable(htmlFormat()) != FALSE) result.emplace_back("text/html");
    if (IsClipboardFormatAvailable(pngFormat()) != FALSE) result.emplace_back("image/png");
    return result;
  }

private:
  static UINT htmlFormat() {
    static const UINT format = RegisterClipboardFormatW(L"HTML Format");
    return format;
  }

  [[nodiscard]] static std::string readText() {
    ClipboardOpenScope clipboard;
    if (!clipboard.isOpen()) {
      return {};
    }
    const HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data == nullptr) {
      return {};
    }
    GlobalLockScope lock(static_cast<HGLOBAL>(data));
    if (!lock.valid()) {
      return {};
    }
    const wchar_t* text = lock.as<wchar_t>();
    return utf16ToUtf8(text, std::wcslen(text));
  }

  /** The registered "PNG" format; browsers and editors use it for a lossless image clip. */
  [[nodiscard]] static UINT pngFormat() {
    static const UINT format = ::RegisterClipboardFormatW(L"PNG");
    return format;
  }

  [[nodiscard]] static std::string readPng() {
    ClipboardOpenScope clipboard;
    if (!clipboard.isOpen()) return {};
    const HANDLE data = GetClipboardData(pngFormat());
    if (data == nullptr) return {};
    GlobalLockScope lock(static_cast<HGLOBAL>(data));
    if (!lock.valid()) return {};
    const auto* bytes = lock.as<char>();
    const SIZE_T size = GlobalSize(static_cast<HGLOBAL>(data));
    if (bytes == nullptr || size == 0) return {};
    // PNG is binary and may legitimately contain NUL bytes, so the whole block is the payload.
    return std::string(bytes, size);
  }

  static void writePng(std::string png) {
    ClipboardOpenScope clipboard;
    if (!clipboard.isOpen()) return;
    if (EmptyClipboard() == FALSE) return;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, png.size());
    if (memory == nullptr) return;
    GlobalFreeScope freeScope(memory);
    {
      GlobalLockScope lock(memory);
      if (!lock.valid()) return;
      std::memcpy(lock.as<char>(), png.data(), png.size());
    }
    if (SetClipboardData(pngFormat(), memory) != nullptr) freeScope.release();
  }
  [[nodiscard]] static std::string readHtml() {
    ClipboardOpenScope clipboard;
    if (!clipboard.isOpen()) {
      return {};
    }
    const HANDLE data = GetClipboardData(htmlFormat());
    if (data == nullptr) {
      return {};
    }
    GlobalLockScope lock(static_cast<HGLOBAL>(data));
    if (!lock.valid()) {
      return {};
    }
    // CF_HTML is a byte stream (a header plus the fragment); it is already UTF-8 in practice.
    const auto* bytes = lock.as<char>();
    const SIZE_T size = GlobalSize(static_cast<HGLOBAL>(data));
    if (bytes == nullptr || size == 0) return {};
    size_t length = 0;
    while (length < size && bytes[length] != '\0') ++length;
    return std::string(bytes, length);
  }

  [[nodiscard]] static std::string readFiles() {
    ClipboardOpenScope clipboard;
    if (!clipboard.isOpen()) {
      return {};
    }
    const HANDLE data = GetClipboardData(CF_HDROP);
    if (data == nullptr) {
      return {};
    }
    const auto drop = static_cast<HDROP>(data);
    const UINT count = DragQueryFileW(drop, 0xFFFFFFFFu, nullptr, 0);
    std::string list;
    for (UINT index = 0; index < count; ++index) {
      const UINT length = DragQueryFileW(drop, index, nullptr, 0);
      if (length == 0) continue;
      std::wstring path(static_cast<size_t>(length) + 1, L'\0');
      if (DragQueryFileW(drop, index, path.data(), length + 1) == 0) continue;
      path.resize(length);
      if (!list.empty()) list += "\r\n";
      list += utf16ToUtf8(path.c_str(), path.size());
    }
    return list;
  }

  static void writeFiles(const std::vector<std::string>& paths) {
    std::wstring block;
    for (const std::string& path: paths) {
      const std::wstring wide = utf8ToUtf16(path);
      if (wide.empty()) continue;
      block += wide;
      block.push_back(L'\0');
    }
    if (block.empty()) return;
    block.push_back(L'\0');
    const size_t bytes = sizeof(DropFilesHeader) + block.size() * sizeof(wchar_t);
    ClipboardOpenScope clipboard;
    if (!clipboard.isOpen()) return;
    if (EmptyClipboard() == FALSE) return;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) return;
    GlobalFreeScope freeScope(memory);
    {
      GlobalLockScope lock(memory);
      if (!lock.valid()) return;
      auto* drop = lock.as<DropFilesHeader>();
      drop->pFiles = static_cast<DWORD>(sizeof(DropFilesHeader));
      drop->fWide = TRUE;
      std::memcpy(reinterpret_cast<char*>(drop) + sizeof(DropFilesHeader), block.data(),
        block.size() * sizeof(wchar_t));
    }
    if (SetClipboardData(CF_HDROP, memory) != nullptr) freeScope.release();
  }

  static void writeHtml(std::string html) {
    const size_t bytes = html.size() + 1;
    ClipboardOpenScope clipboard;
    if (!clipboard.isOpen()) return;
    if (EmptyClipboard() == FALSE) return;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) return;
    GlobalFreeScope freeScope(memory);
    {
      GlobalLockScope lock(memory);
      if (!lock.valid()) return;
      std::memcpy(lock.as<char>(), html.c_str(), bytes);
    }
    if (SetClipboardData(htmlFormat(), memory) != nullptr) freeScope.release();
  }

  static void writeText(std::string text) {
    const std::wstring wide = utf8ToUtf16(text);
    ClipboardOpenScope clipboard;
    if (!clipboard.isOpen()) {
      return;
    }
    if (EmptyClipboard() == FALSE) {
      return;
    }
    const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) {
      return;
    }
    GlobalFreeScope freeScope(memory);
    {
      GlobalLockScope lock(memory);
      if (!lock.valid()) {
        return;
      }
      std::memcpy(lock.as<wchar_t>(), wide.c_str(), bytes);
    }
    // On success the clipboard owns the handle; on failure the scope frees it.
    if (SetClipboardData(CF_UNICODETEXT, memory) != nullptr) {
      freeScope.release();
    }
  }
};

} // namespace anonymous

std::shared_ptr<Clipboard> createPlatformClipboard() {
  return std::make_shared<Win32Clipboard>();
}

uint64_t platformClipboardChangeCount() {
  // GetClipboardSequenceNumber is cheap (a per-thread cached counter) and needs no open handle. It
  // counts writes from any process, so it is the pull-side twin of WM_CLIPBOARDUPDATE.
  return static_cast<uint64_t>(::GetClipboardSequenceNumber());
}

} // namespace coral::ui::detail
