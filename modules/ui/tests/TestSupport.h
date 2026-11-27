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

#ifndef CORAL_UI_TESTSUPPORT_H
#define CORAL_UI_TESTSUPPORT_H

#include <gtest/gtest.h>

#include <ui/Window.h>

namespace coral::ui::test {

/**
 * True when a headless window got the GPU recording backend rather than the CPU raster fallback.
 *
 * The CI Windows runner has neither a Vulkan loader nor an OpenGL 4.1 context. A headless Auto window
 * now falls back to raster, which keeps layout/control tests running; tests that assert recording,
 * GPU image output or frame statistics should skip in that environment.
 */
inline bool gpuRecordingAvailable() {
  static const bool available = [] {
    Window::Options options;
    options.headless = true;
    options.width = 8;
    options.height = 8;
    options.frameLimit = 0;
    const auto window = Window::create(options);
    return window != nullptr && window->rendererCapabilities().recording;
  }();
  return available;
}

} // namespace coral::ui::test

#define CORAL_REQUIRE_RECORDING_BACKEND()                                              \
  do {                                                                                 \
    if (!coral::ui::test::gpuRecordingAvailable()) {                                   \
      GTEST_SKIP() << "requires a GPU recording backend (headless CI fell back to raster)"; \
    }                                                                                  \
  } while (false)

#endif // CORAL_UI_TESTSUPPORT_H
