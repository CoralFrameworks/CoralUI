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

#ifndef CORAL_UI_NATIVEALERT_H
#define CORAL_UI_NATIVEALERT_H

#include <cstdint>
#include <string>
#include <vector>

namespace coral::ui {

class Window;

enum class AlertStyle : uint8_t {
  Informational,
  Warning,
  Critical,
};

/** A native system alert. Buttons are returned by index, matching the order in `buttons`. */
struct NativeAlertOptions {
  std::string title;
  std::string message;
  std::vector<std::string> buttons{ "OK" };
  /** Index of the button that receives Return; clamped to the button list. */
  int defaultButton = 0;
  AlertStyle style = AlertStyle::Informational;
  /** Optional native accessory view (`NSView*` on macOS); ignored by other backends. */
  void* accessoryView = nullptr;
};

/**
 * Shows a platform-native alert and waits for a button.
 *
 * macOS uses `NSAlert`, attached as a sheet when the active parent window is available. Windows uses
 * `TaskDialogIndirect` (with a `MessageBoxW` fallback for a host without the common-controls v6
 * manifest). Other platforms currently return -1 (`supportsNativeAlerts()` is false); they keep
 * their in-window `Dialog` for now.
 *
 * @return the zero-based button index, or -1 when the platform has no native alert, the call is not
 *         on the UI thread, or the alert was dismissed without a button.
 */
[[nodiscard]] int showNativeAlert(const NativeAlertOptions& options, Window* parent = nullptr);

[[nodiscard]] bool supportsNativeAlerts() noexcept;

} // namespace coral::ui

#endif // CORAL_UI_NATIVEALERT_H
