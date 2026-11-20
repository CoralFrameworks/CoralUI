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

#ifndef CORAL_UI_XDNDSOURCE_H
#define CORAL_UI_XDNDSOURCE_H

#include <ui/DragDrop.h>

#include <X11/Xlib.h>

#include <memory>
#include <string>
#include <vector>

namespace coral::ui::detail {

/**
 * A blocking XDND drag source: the X11 counterpart of OLE's `DoDragDrop`.
 *
 * The source owns `XdndSelection` for the duration of the drag, announces itself to the window under
 * the pointer with `XdndEnter`/`XdndPosition`, and answers the target's selection request with the
 * payload once the user drops. It runs its own event loop while the pointer is grabbed, which is why
 * it is a separate class: the window backend only has to create it, and `x11_drag_smoke` drives it
 * against a fake target without a GPU or a UI window.
 *
 * One representation is offered per drag (the payload's primary MIME, plus its secondary one when
 * set); the target picks the one it wants, which is exactly the XDND model.
 */
class XdndSource {
 public:
  XdndSource(Display* display, ::Window window);
  ~XdndSource();

  XdndSource(const XdndSource&) = delete;
  XdndSource& operator=(const XdndSource&) = delete;

  /**
   * Runs one drag until the user releases the pointer, presses Escape, or the drag times out.
   *
   * @return the effect the target reported, or `DragEffect::None` when it rejected the drag or the
   *         platform could not start one. Only `DragEffect::Copy` is offered, so a caller that does
   *         not allow a copy gets `None` back without any protocol traffic.
   */
  [[nodiscard]] DragEffect run(const DragPayload& payload, DragEffect allowed);

 private:
  struct Impl;

  Display* mDisplay = nullptr;
  ::Window mWindow = 0;
  std::unique_ptr<Impl> mImpl;
};

} // namespace coral::ui::detail

#endif // CORAL_UI_XDNDSOURCE_H
