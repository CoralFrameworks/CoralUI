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

#include <ui/PlatformClock.h>

#include <atomic>

namespace coral::ui::platform {

namespace {

std::atomic<double> gClockRatePpm { 0.0 };

} // namespace

double clockRatePpm() noexcept {
  return gClockRatePpm.load(std::memory_order_relaxed);
}

void setClockRatePpm(const double ppm) noexcept {
  if (ppm > -50000.0 && ppm < 50000.0) {
    gClockRatePpm.store(ppm, std::memory_order_relaxed);
  }
}

} // namespace coral::ui::platform
