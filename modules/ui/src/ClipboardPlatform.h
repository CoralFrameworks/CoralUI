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

#ifndef CORAL_UI_CLIPBOARDPLATFORM_H
#define CORAL_UI_CLIPBOARDPLATFORM_H

#include <cstdint>
#include <memory>

namespace coral::ui {
class Clipboard;
}

namespace coral::ui::detail {

/**
 * Creates the multi-format clipboard for the current platform.
 *
 * Implemented once per platform in `src/platform/Clipboard*.cpp`. A null result means the platform
 * has no native implementation and `Clipboard::system()` installs the in-memory fallback.
 */
[[nodiscard]] std::shared_ptr<Clipboard> createPlatformClipboard();

/**
 * A monotonic counter of clipboard changes the native backend observed, including this process's own
 * writes, or 0 when the platform has no such notion.
 *
 * `Clipboard::systemChangeCount()` forwards here. It is the pull side of change detection for a UI
 * that already polls; the push side stays `Clipboard::notifySystemChanged()`/the change handler.
 */
[[nodiscard]] uint64_t platformClipboardChangeCount();

} // namespace coral::ui::detail

#endif // CORAL_UI_CLIPBOARDPLATFORM_H
