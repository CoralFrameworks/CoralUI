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

#ifndef CORAL_UI_EVENT_H
#define CORAL_UI_EVENT_H

#include <ui/DragDrop.h>
#include <ui/Geometry.h>

#include <chrono>
#include <string>
#include <vector>

namespace coral::ui {

enum class PointerButton : uint8_t {
  None,
  Primary,
  Secondary,
  Middle,
};

/** Which kind of device produced a pointer event. */
enum class PointerKind : uint8_t {
  Mouse,
  Touch,
  Pen,
};

struct PointerEvent {
  Point position;
  PointerButton button = PointerButton::None;
  bool inside = true;
  /** Shift extends a text selection on press instead of collapsing it. */
  bool shift = false;
  /** The device that produced the event; a mouse for everything a test synthesises by default. */
  PointerKind kind = PointerKind::Mouse;
  /**
   * Identifies one contact for as long as it exists.
   *
   * Mouse events use 0. Touch reports one id per finger, so a layer that tracks a drag can tell two
   * simultaneous contacts apart; a pen uses the same space.
   */
  std::uint32_t pointerId = 0;
  /**
   * Pen pressure in 0..1; 1 for a mouse and for a touch contact that reports no pressure.
   *
   * A hover reads 0, which is what lets a pen tool distinguish it from a contact.
   */
  float pressure = 1.0f;
};

/**
 * A drop; position is in logical coordinates.
 *
 * `paths` stays as the convenient file view, while `payload` is the general drag/drop model: a
 * platform file drop fills both (type `text/uri-list`), an internal drag fills `payload` only.
 */
struct DropEvent {
  Point position;
  std::vector<std::string> paths;
  DragPayload payload;
  DragEffect effect = DragEffect::None;
  /** True for a drop that came from outside the application. */
  bool external = false;
};

/**
 * Logical keys, mapped from the platform by the window.
 *
 * A–Z, the digits and F1–F12 are contiguous groups so a platform map can do arithmetic on the range.
 * Punctuation names follow the physical key on a US layout, which is what every platform's key
 * translation is defined against; a caller that needs the produced character reads
 * `KeyEvent::character` instead.
 */
enum class Key : uint16_t {
  Unknown,
  Escape,
  Enter,
  Space,
  Tab,
  Backspace,
  Left,
  Right,
  Up,
  Down,
  A,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,
  Delete,
  Home,
  End,
  PageUp,
  PageDown,
  Insert,
  Digit0,
  Digit1,
  Digit2,
  Digit3,
  Digit4,
  Digit5,
  Digit6,
  Digit7,
  Digit8,
  Digit9,
  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,
  Comma,
  Period,
  Slash,
  Backslash,
  Semicolon,
  Apostrophe,
  LeftBracket,
  RightBracket,
  Minus,
  Equal,
  Grave,
  /**
   * Modifier keys, left and right distinct.
   *
   * A `KeyEvent` also reports the modifier state in its `control`/`shift`/`alt`/`meta` booleans, which
   * is what a `Shortcut` matches. These values exist so a layer can observe a modifier press itself
   * (a "hold Ctrl" hint, a custom chord) or tell the two sides apart.
   */
  LeftShift,
  RightShift,
  LeftControl,
  RightControl,
  LeftAlt,
  RightAlt,
  LeftMeta,
  RightMeta,
  CapsLock,
  NumLock,
  ScrollLock,
  PrintScreen,
  Pause,
  Menu,
  /** The numeric keypad, independent of the digit row above `Q`. */
  Numpad0,
  Numpad1,
  Numpad2,
  Numpad3,
  Numpad4,
  Numpad5,
  Numpad6,
  Numpad7,
  Numpad8,
  Numpad9,
  NumpadMultiply,
  NumpadAdd,
  NumpadSubtract,
  NumpadDecimal,
  NumpadDivide,
  NumpadEnter,
  NumpadEqual,
};

struct KeyEvent {
  Key key = Key::Unknown;
  bool repeat = false;
  bool shift = false;
  bool control = false;
  bool alt = false;
  /**
   * The platform's command modifier (Cmd on macOS, unused on Windows/Linux for now).
   *
   * Keeping it separate from `control` means a command bound with `Shortcut::command` will not fire
   * on Ctrl by accident once Cocoa is implemented.
   */
  bool meta = false;
  /** The character a text-input layer should insert, when the platform produced one. */
  char32_t character = 0;
};

enum class ScrollPhase : uint8_t {
  None,
  Began,
  Changed,
  Ended,
  Cancelled,
  MayBegin,
  Stationary,
};

struct ScrollEvent {
  Point position;
  float deltaX = 0.0f;
  float deltaY = 0.0f;
  /** Trackpad/wheel gesture phase when the platform reports one. */
  ScrollPhase phase = ScrollPhase::None;
  /** Momentum/inertia phase, separate from the finger-driven `phase`. */
  ScrollPhase momentumPhase = ScrollPhase::None;
  /** True for a trackpad/precise device, false for a line-based wheel. */
  bool precise = false;
  /** Platform wheel timestamp; zero falls back to the delivery time. */
  std::chrono::steady_clock::time_point timestamp{};
};

enum class GestureKind : uint8_t {
  Magnify,
  Rotate,
  Swipe,
};

enum class GesturePhase : uint8_t {
  Began,
  Changed,
  Ended,
  Cancelled,
};

/**
 * A trackpad/gesture event.
 *
 * `magnitude` is magnification for Magnify and degrees for Rotate; Swipe uses `deltaX`/`deltaY`
 * (-1, 0 or 1). `position` is in device pixels at the platform boundary and is converted to logical
 * units by `WindowImpl`, exactly like PointerEvent/ScrollEvent.
 */
struct GestureEvent {
  GestureKind kind = GestureKind::Magnify;
  GesturePhase phase = GesturePhase::Changed;
  Point position;
  float magnitude = 0.0f;
  float deltaX = 0.0f;
  float deltaY = 0.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_EVENT_H
