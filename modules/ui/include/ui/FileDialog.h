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

#ifndef CORAL_UI_FILEDIALOG_H
#define CORAL_UI_FILEDIALOG_H

#include <string>
#include <vector>

namespace coral::ui {

class Window;

enum class FileDialogKind {
  OpenFile,
  SaveFile,
};

/** One file-type filter; extensions are written without the leading dot. */
struct FileDialogFilter {
  std::string label;
  std::vector<std::string> extensions;
};

struct FileDialogOptions {
  std::string title;
  std::string defaultPath;
  std::vector<FileDialogFilter> filters;
  bool allowMultiple = false;
  /**
   * The portal's `parent_window` handle, for example `x11:0x1234`.
   *
   * Empty means no parent. Wayland needs an xdg-foreign exported handle, which the application can
   * obtain through its compositor integration; the framework passes the string through unchanged.
   */
  std::string parentWindow;
  /**
   * The window the dialog belongs to.
   *
   * When set and `parentWindow` is empty, `showFileDialog` fills the platform handle from
   * `Window::portalParentHandle()` before calling the portal/zenity/kdialog backend.
   */
  Window* parent = nullptr;
};

struct FileDialogResult {
  bool accepted = false;
  std::vector<std::string> paths;
};

/**
 * Shows a native open/save dialog and blocks until it closes.
 *
 * On Linux this first tries `zenity`/`kdialog`; a headless or minimal compositor without either
 * returns `accepted == false`. The xdg-desktop-portal backend is a later addition.
 */
[[nodiscard]] FileDialogResult showFileDialog(FileDialogKind kind,
  const FileDialogOptions& options = {});

} // namespace coral::ui

#endif // CORAL_UI_FILEDIALOG_H
