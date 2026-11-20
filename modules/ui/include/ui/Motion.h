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

#ifndef CORAL_UI_MOTION_H
#define CORAL_UI_MOTION_H

namespace coral::ui {

/**
 * Process-wide reduce-motion preference.
 *
 * When enabled, property/bounds animations, smooth scrolling and shared-element transitions snap to
 * their target instead of interpolating; an indeterminate indicator such as Spinner keeps spinning
 * because it carries information rather than decoration. Applications set this from an accessibility
 * setting; it defaults to off so tests and existing callers are unaffected.
 */
void setReduceMotion(bool enabled);
[[nodiscard]] bool reduceMotion();

} // namespace coral::ui

#endif // CORAL_UI_MOTION_H
