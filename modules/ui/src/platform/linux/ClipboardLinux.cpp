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

#include "../../ClipboardPlatform.h"

#include "ClipboardLinuxBackends.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <memory>

namespace coral::ui::detail {

namespace {

/** Written by the X11/Wayland backends, read by any thread polling `Clipboard::systemChangeCount`. */
std::atomic<uint64_t> gClipboardChangeCount{ 0 };

} // namespace anonymous

std::shared_ptr<Clipboard> createPlatformClipboard() {
#if defined(CORAL_UI_WAYLAND)
  if (std::getenv("WAYLAND_DISPLAY") != nullptr) {
    if (auto wayland = createWaylandClipboard(); wayland != nullptr) {
      return wayland;
    }
  }
#endif
#if defined(CORAL_UI_X11)
  if (auto x11 = createX11Clipboard(); x11 != nullptr) {
    return x11;
  }
#endif
  return nullptr;
}

void notePlatformClipboardChanged() {
  gClipboardChangeCount.fetch_add(1, std::memory_order_relaxed);
}

uint64_t platformClipboardChangeCount() {
  return gClipboardChangeCount.load(std::memory_order_relaxed);
}

} // namespace coral::ui::detail
