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

#include <ui/FileDialog.h>

#include "FileDialogPlatform.h"

#include <ui/Window.h>

namespace coral::ui {

FileDialogResult showFileDialog(const FileDialogKind kind, const FileDialogOptions& options) {
  if (options.parent == nullptr || !options.parentWindow.empty()) {
    return detail::showPlatformFileDialog(kind, options);
  }
  FileDialogOptions withParent = options;
  withParent.parentWindow = options.parent->portalParentHandle();
  return detail::showPlatformFileDialog(kind, withParent);
}

} // namespace coral::ui
