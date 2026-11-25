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

#ifndef CORAL_UI_SCREENCAPTURE_H
#define CORAL_UI_SCREENCAPTURE_H

#include <cstdint>

namespace coral::ui {

enum class ScreenCaptureAccess : uint8_t {
  Unavailable,
  Denied,
  Granted,
};

/** @return the current screen-recording permission state (CGPreflightScreenCaptureAccess). */
[[nodiscard]] ScreenCaptureAccess screenCaptureAccess() noexcept;

/** Requests screen-recording permission; may show the system prompt. */
bool requestScreenCaptureAccess();

} // namespace coral::ui

#endif // CORAL_UI_SCREENCAPTURE_H
