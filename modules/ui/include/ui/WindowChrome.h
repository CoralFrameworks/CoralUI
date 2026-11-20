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

#ifndef CORAL_UI_WINDOWCHROME_H
#define CORAL_UI_WINDOWCHROME_H

#include <ui/Geometry.h>

#include <cstdint>

namespace coral::ui {

/** What a pointer at a window-local point would do in a client-side decorated window. */
enum class WindowHit : uint8_t {
  None,
  Client,
  Caption,
  Minimize,
  Maximize,
  Close,
  ResizeLeft,
  ResizeRight,
  ResizeTop,
  ResizeBottom,
  ResizeTopLeft,
  ResizeTopRight,
  ResizeBottomLeft,
  ResizeBottomRight,
};

/** Geometry of a client-side title bar, in logical units. */
struct WindowChromeMetrics {
  float titleBarHeight = 32.0f;
  float resizeBorder = 6.0f;
  float buttonWidth = 46.0f;
  bool resizable = true;
  bool closable = true;
  bool minimizable = true;
  bool maximizable = true;
};

/**
 * Hit-tests a point against a client-side decorated window.
 *
 * The rules are the ones desktop users expect: a resize border around all four edges (with the
 * corners winning over the edges), a draggable caption band along the top, and the window buttons
 * at its right end (close furthest right, then maximize, then minimize). A point outside the window
 * returns `None`, which the platform maps to "let the OS decide".
 *
 * This lives in the framework rather than in each platform backend so the same geometry drives the
 * Win32 `WM_NCHITTEST` answer, a future `xdg-decoration` server-side path and the tests.
 */
[[nodiscard]] WindowHit hitTestWindowChrome(Size size, Point point,
  const WindowChromeMetrics& metrics = {});

} // namespace coral::ui

#endif // CORAL_UI_WINDOWCHROME_H
