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

#ifndef CORAL_UI_GEOMETRY_H
#define CORAL_UI_GEOMETRY_H

#include <geometry/Rect.h>
#include <geometry/Size.h>

namespace coral::ui {

using Rect = graphics::Rect;
using Point = graphics::Point;
using Size = graphics::Size;

/** Space inside a layer's bounds. */
struct Insets {
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;

  constexpr float horizontal() const noexcept {
    return left + right;
  }

  constexpr float vertical() const noexcept {
    return top + bottom;
  }

  constexpr Insets all(const float value) noexcept {
    return { value, value, value, value };
  }

  friend constexpr bool operator==(const Insets& a, const Insets& b) noexcept {
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
  }
};

} // namespace coral::ui

#endif // CORAL_UI_GEOMETRY_H
