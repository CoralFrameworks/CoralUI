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

#ifndef CORAL_UI_CLIPBOARDLINUXBACKENDS_H
#define CORAL_UI_CLIPBOARDLINUXBACKENDS_H

#include <cstddef>
#include <cstdint>
#include <memory>

namespace coral::ui {
class Clipboard;
}

namespace coral::ui::detail {

[[nodiscard]] std::shared_ptr<Clipboard> createX11Clipboard();
[[nodiscard]] std::shared_ptr<Clipboard> createWaylandClipboard();
/** Records the latest input serial; Wayland's set_selection requires one. */
void setWaylandClipboardSerial(uint32_t serial);

/**
 * Lowers the payload size above which the X11 owner switches to the ICCCM INCR protocol.
 *
 * The owner normally uses INCR only for data that does not fit into one X request (often several
 * megabytes), so `x11_clipboard_smoke` lowers the threshold to exercise the incremental path with a
 * small payload. Pass 0 to restore the server-derived default.
 */
void setX11ClipboardIncrThresholdForTesting(std::size_t bytes);

/**
 * Bumps the process-wide Linux clipboard change counter.
 *
 * The X11 owner (via XFixes) and the Wayland data device call this whenever a foreign selection is
 * observed, so `Clipboard::systemChangeCount()` has the same pull semantics as Win32's sequence
 * number. It returns 0 while no native backend is running (the memory fallback).
 */
void notePlatformClipboardChanged();

} // namespace coral::ui::detail

#endif // CORAL_UI_CLIPBOARDLINUXBACKENDS_H
