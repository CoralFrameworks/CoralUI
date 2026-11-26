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

/**
 * @file NativeAlertWin32.cpp
 *
 * Native Windows alerts: `TaskDialogIndirect` gives the same custom button list, default button and
 * severity icon the other platform backends receive. `TaskDialogIndirect` needs the version 6
 * common-controls manifest, so this object carries the dependency itself; a consumer that links the
 * toolkit gets the manifest entry without touching its own build. A host that somehow still ends up
 * without v6 (for example a custom manifest that removes the dependency) gets a `MessageBoxW`
 * fallback instead of a silent no-op.
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600 // TaskDialogIndirect and the task-dialog structures are Vista+.
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x06000000
#endif

#include <ui/NativeAlert.h>
#include <ui/Window.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

namespace coral::ui {

namespace {

constexpr int kFirstButtonId = 1000;
// -1 is a valid "dismissed without a button" result, so the task-dialog path needs its own failure
// value to tell "common controls v6 is unavailable" apart from "the user pressed Esc".
constexpr int kTaskDialogUnavailable = std::numeric_limits<int>::min();

bool alertLogEnabled() {
  return std::getenv("CORAL_UI_NATIVE_ALERT_LOG") != nullptr;
}

std::wstring utf8ToUtf16(const std::string& text) {
  if (text.empty()) return {};
  const int length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
    static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) return {};
  std::wstring wide(static_cast<size_t>(length), L'\0');
  ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
    static_cast<int>(text.size()), wide.data(), length);
  return wide;
}

/** The HWND of @p parent, or null when it has none; a parent on another thread is rejected. */
HWND parentHandle(Window* parent, bool* wrongThread) {
  *wrongThread = false;
  if (parent == nullptr) return nullptr;
  HWND hwnd = static_cast<HWND>(parent->nativeWindowHandle());
  if (hwnd == nullptr || !::IsWindow(hwnd)) return nullptr;
  DWORD processId = 0;
  const DWORD threadId = ::GetWindowThreadProcessId(hwnd, &processId);
  if (threadId != 0 && threadId != ::GetCurrentThreadId()) {
    *wrongThread = true;
    return nullptr;
  }
  return hwnd;
}

int showWithTaskDialog(const NativeAlertOptions& options, HWND owner) {
  const std::wstring title = utf8ToUtf16(options.title);
  const std::wstring message = utf8ToUtf16(options.message);

  // NSAlert treats `title` as the bold line and `message` as the body. TaskDialog has the same two
  // slots; fall back to the other string when one of them is empty.
  const std::wstring fallbackTitle = L"CoralUI";
  const std::wstring& mainInstruction = !title.empty() ? title
    : (!message.empty() ? message : fallbackTitle);
  const bool hasContent = !title.empty() && !message.empty();

  std::vector<std::wstring> labels;
  labels.reserve(options.buttons.size());
  for (const std::string& label: options.buttons) {
    labels.push_back(utf8ToUtf16(label.empty() ? "OK" : label));
  }
  if (labels.empty()) labels.emplace_back(L"OK");

  std::vector<TASKDIALOG_BUTTON> buttons;
  buttons.reserve(labels.size());
  for (size_t index = 0; index < labels.size(); ++index) {
    buttons.push_back({ kFirstButtonId + static_cast<int>(index), labels[index].c_str() });
  }

  const int defaultIndex = std::clamp(options.defaultButton, 0,
    static_cast<int>(buttons.size()) - 1);

  TASKDIALOGCONFIG config{};
  config.cbSize = sizeof(config);
  config.hwndParent = owner;
  config.hInstance = ::GetModuleHandleW(nullptr);
  config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
  config.dwCommonButtons = 0;
  config.pszMainInstruction = mainInstruction.c_str();
  config.pszContent = hasContent ? message.c_str() : nullptr;
  config.pButtons = buttons.data();
  config.cButtons = static_cast<UINT>(buttons.size());
  config.nDefaultButton = kFirstButtonId + defaultIndex;
  switch (options.style) {
    case AlertStyle::Warning:
      config.pszMainIcon = TD_WARNING_ICON;
      break;
    case AlertStyle::Critical:
      config.pszMainIcon = TD_ERROR_ICON;
      break;
    case AlertStyle::Informational:
    default:
      config.pszMainIcon = TD_INFORMATION_ICON;
      break;
  }

  int pressed = 0;
  const HRESULT result = ::TaskDialogIndirect(&config, &pressed, nullptr, nullptr);
  if (alertLogEnabled()) {
    std::fprintf(stderr, "[alert] TaskDialogIndirect hr=0x%08lx pressed=%d buttons=%zu\n",
      static_cast<unsigned long>(result), pressed, buttons.size());
  }
  if (FAILED(result)) return kTaskDialogUnavailable;
  if (pressed >= kFirstButtonId && pressed < kFirstButtonId + static_cast<int>(buttons.size())) {
    return pressed - kFirstButtonId;
  }
  return -1;
}

/** Last resort for a host whose manifest does not activate common controls v6. */
int showWithMessageBox(const NativeAlertOptions& options, HWND owner) {
  const std::wstring title = utf8ToUtf16(options.title);
  const std::wstring message = utf8ToUtf16(options.message);
  const int count = options.buttons.empty() ? 1 : static_cast<int>(options.buttons.size());
  const int defaultIndex = std::clamp(options.defaultButton, 0, count - 1);

  UINT type = count > 1 ? MB_OKCANCEL : MB_OK;
  switch (options.style) {
    case AlertStyle::Warning:
      type |= MB_ICONWARNING;
      break;
    case AlertStyle::Critical:
      type |= MB_ICONERROR;
      break;
    case AlertStyle::Informational:
    default:
      type |= MB_ICONINFORMATION;
      break;
  }

  const wchar_t* text = message.empty() ? title.c_str() : message.c_str();
  const int result = ::MessageBoxW(owner, text, title.empty() ? L"CoralUI" : title.c_str(), type);
  if (alertLogEnabled()) {
    std::fprintf(stderr, "[alert] MessageBoxW result=%d buttons=%d\n", result, count);
  }
  if (result == IDOK) return defaultIndex;
  if (result == IDCANCEL) {
    for (int index = 0; index < count; ++index) {
      if (index != defaultIndex) return index;
    }
  }
  return -1;
}

} // namespace

int showNativeAlert(const NativeAlertOptions& options, Window* parent) {
  bool wrongThread = false;
  const HWND owner = parentHandle(parent, &wrongThread);
  if (alertLogEnabled()) {
    std::fprintf(stderr, "[alert] show owner=%p wrongThread=%d buttons=%zu default=%d\n",
      static_cast<void*>(owner), wrongThread ? 1 : 0, options.buttons.size(),
      options.defaultButton);
  }
  if (wrongThread) return -1;

  const int taskDialogResult = showWithTaskDialog(options, owner);
  if (taskDialogResult != kTaskDialogUnavailable) return taskDialogResult;
  return showWithMessageBox(options, owner);
}

bool supportsNativeAlerts() noexcept {
  return true;
}

} // namespace coral::ui
