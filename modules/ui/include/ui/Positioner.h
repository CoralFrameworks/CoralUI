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

#ifndef CORAL_UI_POSITIONER_H
#define CORAL_UI_POSITIONER_H

#include <ui/Geometry.h>
#include <ui/Types.h>

namespace coral::ui {

/** Where a floating surface prefers to sit relative to its anchor. */
enum class Placement : uint8_t {
  /** Below the anchor, left edges aligned (menus, dropdowns). */
  BelowStart,
  /** Below the anchor, right edges aligned. */
  BelowEnd,
  /** Above the anchor, left edges aligned. */
  AboveStart,
  /** Above the anchor, right edges aligned. */
  AboveEnd,
  /** To the right of the anchor, vertically centred (submenus). */
  RightStart,
  /** To the left of the anchor, vertically centred. */
  LeftStart,
  /** Centred in the available rectangle (dialogs). */
  Center,
};

struct PlacementRequest {
  /** The control the surface points at; for `Center` only its position is irrelevant. */
  Rect anchor{};
  /** The measured size of the floating surface. */
  Size size{};
  /** The rectangle it must stay inside, usually the window's logical bounds. Empty means "no clamp". */
  Rect available{};
  Placement preferred = Placement::BelowStart;
  /** Distance between the anchor and the surface. */
  float gap = 2.0f;
  /** Minimum distance to the edges of `available` (also the fallback inset when shifting). */
  Insets margin{ 8.0f, 8.0f, 8.0f, 8.0f };
};

struct PlacementResult {
  Rect bounds{};
  /** The placement actually used after flipping. */
  Placement placement = Placement::BelowStart;
  /** True when the preferred placement did not fit and the opposite side was used. */
  bool flipped = false;
  /** True when the surface had to slide along an axis to stay inside `available`. */
  bool shifted = false;
};

/**
 * Places a floating surface (menu, popup, tooltip, dialog) relative to an anchor.
 *
 * The policy is the one desktop UIs converge on: prefer the requested side, flip to the opposite side
 * when the preferred one would overflow and the opposite one fits, then slide along the other axis so
 * the surface stays fully inside `available` (minus `margin`). It never resizes the surface: a surface
 * larger than `available` is clamped at the top/left edge and allowed to overflow at the far edge,
 * which is more predictable than a silently shrunken menu.
 */
[[nodiscard]] PlacementResult placeFloating(const PlacementRequest& request);

} // namespace coral::ui

#endif // CORAL_UI_POSITIONER_H
