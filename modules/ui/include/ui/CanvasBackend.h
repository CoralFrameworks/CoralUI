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

#ifndef CORAL_UI_CANVASBACKEND_H
#define CORAL_UI_CANVASBACKEND_H

#include <ui/Canvas.h>

#include <graphics/Canvas.h>

namespace coral::ui::detail {

/** Host-only factory: bind a UI canvas façade to a live graphics canvas. */
[[nodiscard]] Canvas makeCanvas(graphics::Canvas& canvas);

/**
 * Host-only escape hatch for recording and external-image commands. Control code must not include
 * this header; it exists for `Window`, Layer recording and `RasterRenderer`.
 */
[[nodiscard]] graphics::Canvas& nativeCanvas(Canvas& canvas);

} // namespace coral::ui::detail

#endif // CORAL_UI_CANVASBACKEND_H
