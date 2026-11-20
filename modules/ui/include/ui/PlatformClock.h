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

#ifndef CORAL_UI_PLATFORMCLOCK_H
#define CORAL_UI_PLATFORMCLOCK_H

namespace coral::ui::platform {

/**
 * The platform/backend clock calibration in ppm.
 *
 * A backend/driver that can query a real display/input clock installs it here at startup. 0 means
 * "not calibrated"; RecyclingListView then falls back to its own EWMA estimate. No environment
 * variable is consulted: calibration is either supplied by the platform or absent.
 */
[[nodiscard]] double clockRatePpm() noexcept;

/** Backend/driver entry point; pass 0 to clear the calibration. */
void setClockRatePpm(double ppm) noexcept;

} // namespace coral::ui::platform

#endif // CORAL_UI_PLATFORMCLOCK_H
