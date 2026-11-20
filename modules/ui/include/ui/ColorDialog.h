/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CORAL_UI_COLORDIALOG_H
#define CORAL_UI_COLORDIALOG_H

#include <ui/Types.h>

#include <string>

namespace coral::ui {

struct ColorDialogOptions {
  std::string title = "Choose color";
  /** Native dialogs on Windows do not expose alpha; the value is preserved from the input colour. */
  bool preserveAlpha = true;
};

struct ColorDialogResult {
  bool accepted = false;
  Color color;
};

/**
 * Shows the platform colour chooser. Windows uses the native `ChooseColorW` dialog; other platforms
 * currently return `accepted == false` (the UI picker remains available).
 */
[[nodiscard]] ColorDialogResult showColorDialog(Color initial,
  const ColorDialogOptions& options = {});

/**
 * Blocking screen eyedropper.
 *
 * Waits for a primary click and samples that screen pixel; Escape cancels. It returns false when the
 * platform has no implementation or the user cancels. Call on the UI thread only after hiding any
 * occluding windows; this is intentionally a small utility, not a magnifier overlay.
 */
[[nodiscard]] bool pickScreenColor(Color& out, std::string* error = nullptr);

} // namespace coral::ui

#endif // CORAL_UI_COLORDIALOG_H
