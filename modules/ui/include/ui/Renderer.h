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

#ifndef CORAL_UI_RENDERER_H
#define CORAL_UI_RENDERER_H

#include <ui/Types.h>

#include <ui/RendererCapabilities.h>


namespace coral::ui {

/** Which kind of frame sink a window should use. */
enum class RendererBackend {
  Auto,
  GPU,
  Raster,
};

/** Which kind of frame sink a window is actually using. */
enum class RendererKind {
  Unknown,
  GPU,
  Raster,
};

/**
 * Optional API preference for a GPU renderer. `Auto` is the default and the only value a normal
 * application needs; the others exist for backend conformance tests and benchmarking.
 */
enum class RendererApi {
  Auto,
  OpenGL,
  Vulkan,
};

/**
 * The frame-lifecycle contract a UI host can depend on.
 *
 * `Window`, `Application` and headless test hosts all drive frames through this interface. It
 * deliberately exposes no device, surface, swapchain or backend enum: a caller that only needs to
 * run a frame loop cannot tell whether the implementation is GPU or raster. Backend-specific
 * facilities remain available on `Window` for producers, but are not part of this contract.
 */
class Renderer {
 public:
  virtual ~Renderer() = default;

  /** The actual sink behind this renderer, reported for diagnostics and capability gates. */
  [[nodiscard]] virtual RendererKind rendererKind() const noexcept = 0;

  /** Platform differences this renderer's frames can rely on. */
  [[nodiscard]] virtual const RendererCapabilities& capabilities() const noexcept = 0;

  /** Draws and presents one frame. */
  virtual bool renderFrame() = 0;

  /** Recreates the surface after device/swapchain loss. */
  virtual bool recover() = 0;

  /** Re-renders the last frame into a bitmap, when the implementation supports readback. */
  [[nodiscard]] virtual bool snapshot(Bitmap* out) = 0;
};

} // namespace coral::ui

#endif // CORAL_UI_RENDERER_H
