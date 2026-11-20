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

#ifndef CORAL_UI_PLATFORM_LINUX_IMEBACKEND_H
#define CORAL_UI_PLATFORM_LINUX_IMEBACKEND_H

#include <ui/Geometry.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui::detail {

/**
 * The platform's text-input service, independent of the windowing protocol.
 *
 * XIM and IBus both own a native connection and an input context, but the window code should not
 * know which one is in use. The backend turns native key events into composition/commit strings and
 * filters the events the input method consumed; the focused control remains the owner of the text.
 */
struct ImeHost {
  virtual ~ImeHost() = default;
  virtual void onImeComposition(const std::string& text) = 0;
  virtual void onImeCommit(const std::string& text) = 0;
  /** A lookup table from an input method that reports one (IBus); XIM leaves it empty. */
  virtual void onImeCandidates(std::vector<std::string> candidates, size_t selected) {
    (void)candidates;
    (void)selected;
  }
  [[nodiscard]] virtual Rect imeCursorRect() const = 0;
};

class ImeBackend {
 public:
  virtual ~ImeBackend() = default;

  /** @return false when no input method is available; the caller keeps its plain key path. */
  virtual bool initialize() = 0;
  virtual void shutdown() = 0;

  /** @return true when the event belongs to the input method and must not reach the key path. */
  virtual bool filterEvent(void* nativeEvent) = 0;

  /** UTF-8 lookup for the event, or 0 when the input method has nothing. */
  virtual int lookupString(void* nativeKeyEvent, char* utf8, int capacity, uint32_t* keysym,
    int* status) = 0;

  virtual void focusIn() = 0;
  virtual void focusOut() = 0;
  virtual void setCursorRect(const Rect& logicalRect) = 0;
  /**
   * Sends the focused field's text and caret to the input method.
   *
   * Offsets are UTF-8 byte offsets; a backend that needs characters converts them. XIM has no
   * surrounding-text request, so the default is a no-op.
   */
  virtual void setSurroundingText(const std::string& text, size_t cursor, size_t anchor) {
    (void)text;
    (void)cursor;
    (void)anchor;
  }
  /** @return true when the candidate navigation key was consumed by the input method. */
  virtual bool handleKey(uint32_t key) {
    (void)key;
    return false;
  }
  [[nodiscard]] virtual const char* name() const = 0;
};

/** Xlib XIM. `display` is Display*, `window` is Window. */
[[nodiscard]] std::unique_ptr<ImeBackend> makeXimIme(void* display, unsigned long window,
  ImeHost* host);

#if defined(CORAL_UI_HAS_IBUS)
/** IBus over the session bus. The same X11 display/window are used for the input context. */
[[nodiscard]] std::unique_ptr<ImeBackend> makeIbusIme(void* display, unsigned long window,
  ImeHost* host);
#endif

} // namespace coral::ui::detail

#endif // CORAL_UI_PLATFORM_LINUX_IMEBACKEND_H
