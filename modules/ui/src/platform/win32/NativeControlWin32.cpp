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

#include "NativeControlWin32.h"

#include <atomic>
#include <utility>

namespace coral::ui::win32::detail {

std::wstring toWide(std::string_view utf8) {
  if (utf8.empty()) return {};
  auto convert = [&](const DWORD flags) {
    return ::MultiByteToWideChar(CP_UTF8, flags, utf8.data(), static_cast<int>(utf8.size()),
      nullptr, 0);
  };
  int length = convert(MB_ERR_INVALID_CHARS);
  if (length <= 0) length = convert(0); // Keep a malformed byte instead of dropping the text.
  if (length <= 0) return {};
  std::wstring wide(static_cast<size_t>(length), L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
  return wide;
}

std::string toUtf8(std::wstring_view wide) {
  if (wide.empty()) return {};
  const int length = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(),
    static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
  if (length <= 0) return {};
  std::string utf8(static_cast<size_t>(length), '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), length,
    nullptr, nullptr);
  return utf8;
}

int nextControlId() {
  static std::atomic<int> next{ 20000 };
  return next.fetch_add(1);
}

namespace {

void ensureCommonControls() {
  static const bool initialized = [] {
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_DATE_CLASSES;
    return ::InitCommonControlsEx(&controls) != FALSE;
  }();
  (void)initialized;
}

} // namespace

HWND createControl(const wchar_t* className, const DWORD style, const DWORD exStyle,
    const int controlId) {
  if (className == nullptr) return nullptr;
  ensureCommonControls();
  HWND control = ::CreateWindowExW(exStyle, className, L"", style | WS_CHILD, 0, 0, 1, 1,
    ::GetDesktopWindow(), reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
    ::GetModuleHandleW(nullptr), nullptr);
  if (control != nullptr) applySystemFont(control);
  return control;
}

void applySystemFont(const HWND control) {
  if (control == nullptr) return;
  ::SendMessageW(control, WM_SETFONT,
    reinterpret_cast<WPARAM>(::GetStockObject(DEFAULT_GUI_FONT)), TRUE);
}

NativeControlHost::NativeControlHost(const HWND control, const int controlId)
    : mControl(control), mControlId(controlId) {}

NativeControlHost::~NativeControlHost() {
  detach();
  // The host owns the child HWND: WindowWin32 only detaches it, mirroring the NSView borrow model
  // in NativeViewLayer. This also destroys a control created for a headless window, whose platform
  // has no native-view hook to remove it.
  if (mControl != nullptr && ::IsWindow(mControl)) {
    ::DestroyWindow(mControl);
  }
}

void NativeControlHost::setHandlers(CommandHandler command, NotifyHandler notify,
    ScrollHandler scroll) {
  mCommand = std::move(command);
  mNotify = std::move(notify);
  mScroll = std::move(scroll);
}

void NativeControlHost::attach(Window* window) {
  detach();
  if (window == nullptr) return;
  mHost = static_cast<HWND>(window->nativeWindowHandle());
  if (mHost == nullptr || !::IsWindow(mHost)) {
    mHost = nullptr;
    return;
  }
  ::SetWindowSubclass(mHost, &NativeControlHost::hostProc,
    static_cast<UINT_PTR>(mControlId), reinterpret_cast<DWORD_PTR>(this));
}

void NativeControlHost::detach() {
  if (mHost != nullptr) {
    if (::IsWindow(mHost)) {
      ::RemoveWindowSubclass(mHost, &NativeControlHost::hostProc,
        static_cast<UINT_PTR>(mControlId));
    }
    mHost = nullptr;
  }
}

LRESULT CALLBACK NativeControlHost::hostProc(HWND hwnd, const UINT message, const WPARAM wParam,
    const LPARAM lParam, const UINT_PTR subclassId, const DWORD_PTR reference) {
  (void)subclassId;
  auto* self = reinterpret_cast<NativeControlHost*>(reference);
  if (self != nullptr) {
    if (message == WM_COMMAND && LOWORD(wParam) == self->mControlId && self->mCommand) {
      if (self->mCommand(static_cast<int>(HIWORD(wParam)))) return 0;
    } else if (message == WM_NOTIFY && self->mNotify) {
      const auto* header = reinterpret_cast<const NMHDR*>(lParam);
      if (header != nullptr && header->hwndFrom == self->mControl &&
          header->idFrom == static_cast<UINT_PTR>(self->mControlId) &&
          self->mNotify(*header)) {
        return 0;
      }
    } else if ((message == WM_HSCROLL || message == WM_VSCROLL) && self->mScroll) {
      if (reinterpret_cast<HWND>(lParam) == self->mControl &&
          self->mScroll(static_cast<int>(LOWORD(wParam)))) {
        return 0;
      }
    }
  }
  return ::DefSubclassProc(hwnd, message, wParam, lParam);
}

} // namespace coral::ui::win32::detail
