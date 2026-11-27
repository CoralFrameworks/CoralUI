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

#include "WindowPlatform.h"

#include <cstdlib>
#include <cstdio>

namespace coral::ui::detail {

std::unique_ptr<WindowPlatform> createWindowPlatform(const Window::Options& options,
    backend::Device* sharedDevice, std::string* diagnostics) {
  if (options.rendererBackend == RendererBackend::Raster) {
    return createRasterPlatform(options, sharedDevice, diagnostics);
  }
  if (options.headless) {
    std::string headlessDiagnostics;
    if (auto headless = createHeadlessPlatform(options, sharedDevice, &headlessDiagnostics)) {
      return headless;
    }
    // CI and other display-less machines may have no Vulkan loader and no OpenGL 4.1 context. A
    // headless Auto window can still exercise the full UI/layer tree through the CPU raster sink;
    // fall back only when the requested GPU surface could not be created.
    if (auto raster = createRasterPlatform(options, sharedDevice, diagnostics)) {
      if (diagnostics != nullptr) {
        *diagnostics = headlessDiagnostics.empty()
          ? "using the raster fallback for a headless window" : headlessDiagnostics + "; using raster";
      }
      return raster;
    }
    if (diagnostics != nullptr && !headlessDiagnostics.empty()) *diagnostics = headlessDiagnostics;
    return nullptr;
  }

#if defined(_WIN32)
  return createWin32Platform(options, sharedDevice, diagnostics);
#elif defined(CORAL_UI_COCOA)
  return createCocoaPlatform(options, sharedDevice, diagnostics);
#elif defined(CORAL_UI_X11) || defined(CORAL_UI_WAYLAND)
  std::string waylandError;
#if defined(CORAL_UI_WAYLAND)
  const bool waylandRequested = options.platform == Window::NativePlatform::Wayland ||
    (options.platform == Window::NativePlatform::Auto && std::getenv("WAYLAND_DISPLAY") != nullptr);
  if (waylandRequested) {
    if (auto wayland = createWaylandPlatform(options, sharedDevice, &waylandError)) {
      return wayland;
    }
    if (std::getenv("CORAL_UI_PLATFORM_DEBUG") != nullptr) {
      std::fprintf(stderr, "wayland create failed: %s\n", waylandError.c_str());
    }
    if (options.platform == Window::NativePlatform::Wayland) {
      if (diagnostics != nullptr) *diagnostics = waylandError;
      return nullptr;
    }
  }
#endif
#if defined(CORAL_UI_X11)
  if (options.platform != Window::NativePlatform::Wayland) {
    if (auto x11 = createX11Platform(options, sharedDevice, diagnostics)) {
      return x11;
    }
    if (diagnostics != nullptr && !waylandError.empty()) {
      *diagnostics = waylandError + "; " + *diagnostics;
    }
    return nullptr;
  }
#else
  if (diagnostics != nullptr) *diagnostics = waylandError.empty()
    ? "Wayland is not available" : waylandError;
#endif
  return nullptr;
#else
  if (diagnostics != nullptr) *diagnostics = "no native UI platform is compiled in";
  return nullptr;
#endif
}

} // namespace coral::ui::detail
