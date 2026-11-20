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

#ifndef CORAL_UI_PLATFORM_WIN32_DROPTARGETWIN32_H
#define CORAL_UI_PLATFORM_WIN32_DROPTARGETWIN32_H

// Keep the Win32 macros out of the UI headers: geometry uses `min`/`max` names.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <memory>

namespace coral::ui::detail {

class WindowHost;

/**
 * An OLE drop target for one window.
 *
 * `WM_DROPFILES` only delivers a final drop; OLE gives enter/over/leave so a control can highlight
 * itself, report the effect (copy/move) and receive the drop with the same payload model an internal
 * drag uses. The wrapper installs `RegisterDragDrop` and revokes it on destruction; the COM object
 * behind it is reference counted by OLE.
 */
class Win32DropTarget {
 public:
  virtual ~Win32DropTarget() = default;
};

/** @return a registered target, or null when OLE is unavailable (the caller keeps WM_DROPFILES). */
[[nodiscard]] std::shared_ptr<Win32DropTarget> createWin32DropTarget(HWND window, WindowHost* host);

} // namespace coral::ui::detail

#endif // CORAL_UI_PLATFORM_WIN32_DROPTARGETWIN32_H
