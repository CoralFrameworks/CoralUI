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

#ifndef CORAL_UI_DETAIL_NATIVECONTROLWIN32_H
#define CORAL_UI_DETAIL_NATIVECONTROLWIN32_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 // The native controls use Vista+ common-control messages.
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x06010000
#endif

#include <ui/Window.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include <functional>
#include <string>
#include <string_view>

namespace coral::ui::win32::detail {

/** UTF-8/UTF-16 conversion for the Win32 control wrappers. */
std::wstring toWide(std::string_view utf8);
std::string toUtf8(std::wstring_view wide);

/** A process-unique child control id (used for both WM_COMMAND ids and window subclass ids). */
int nextControlId();

/**
 * Creates a child control under the desktop window. `NativeViewLayer` reparents it to the real
 * window when the layer is attached, which lets the wrapper be created before it has a window.
 */
HWND createControl(const wchar_t* className, DWORD style, DWORD exStyle, int controlId);

/** Applies the system UI font so the control matches the surrounding Win32 chrome. */
void applySystemFont(HWND control);

/**
 * Routes a child control's notifications back to its wrapper.
 *
 * A Win32 control sends `WM_COMMAND`/`WM_NOTIFY`/`WM_HSCROLL` to its *parent*, not to itself, so the
 * host window is subclassed once per control id. The host owns no HWND and never destroys it: the
 * `WindowWin32` native-view hook owns the child HWND lifetime.
 */
class NativeControlHost {
 public:
  using CommandHandler = std::function<bool(int notificationCode)>;
  using NotifyHandler = std::function<bool(const NMHDR& header)>;
  using ScrollHandler = std::function<bool(int scrollCode)>;

  NativeControlHost(HWND control, int controlId);
  ~NativeControlHost();

  NativeControlHost(const NativeControlHost&) = delete;
  NativeControlHost& operator=(const NativeControlHost&) = delete;

  void setHandlers(CommandHandler command, NotifyHandler notify = {}, ScrollHandler scroll = {});

  /** Subclasses the window owning the control, or detaches from the previous window. */
  void attach(Window* window);
  void detach();

 private:
  static LRESULT CALLBACK hostProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR reference);

  HWND mControl = nullptr;
  int mControlId = 0;
  HWND mHost = nullptr;
  CommandHandler mCommand;
  NotifyHandler mNotify;
  ScrollHandler mScroll;
};

} // namespace coral::ui::win32::detail

#endif // CORAL_UI_DETAIL_NATIVECONTROLWIN32_H
