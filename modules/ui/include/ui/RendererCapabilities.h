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

#ifndef CORAL_UI_RENDERERCAPABILITIES_H
#define CORAL_UI_RENDERERCAPABILITIES_H

#include <cstdint>
#include <string>

namespace backend {
class Device;
}

namespace coral::ui {

/**
 * Whether one rendering feature is available, and why not when it is not.
 *
 * The design's rule for capabilities is that they are declared rather than guessed, and that an
 * unsupported feature carries a reason so an editor can degrade deliberately (drop a blur, draw a
 * placeholder) instead of showing a hole. `reason` points at a string literal and is never owned;
 * an empty reason on an unsupported feature means "not probed".
 */
struct FeatureSupport {
  bool supported = false;
  const char* reason = "";

  [[nodiscard]] explicit operator bool() const noexcept {
    return supported;
  }
};

/**
 * The platform differences the UI layer is allowed to see.
 *
 * Above this snapshot the tree is backend- and platform-agnostic; below it, `graphics`/`backend`
 * decide how a feature is realised. A `RendererCapabilities` is captured once per window (and
 * refreshed after a device recovery), so a layer's decisions are stable for a frame.
 *
 * It is deliberately a **value snapshot** with static reason strings: it can be copied into a
 * `RenderContext` and read on the UI thread without asking a device (which is GPU-thread affine).
 */
struct RendererCapabilities {
  /** The RHI backend and API version, e.g. "Vulkan 1.4.357", or "software" for a raster canvas. */
  std::string backend;

  /** The device's advertised name, empty for a software canvas. */
  std::string device;

  /** True when the frame is recorded into command lists for a GPU pass rather than rasterised. */
  bool recording = false;

  /** Largest 2D texture dimension the device accepts; 0 when unknown. */
  uint32_t maxTextureDimension = 0;

  /** Largest viewport edge the device accepts; 0 when unknown. */
  uint32_t maxViewport = 0;

  /** Sample count of the frame's target. 1 means no MSAA. */
  uint32_t sampleCount = 1;

  /** `Canvas::drawExternalImage`: sampling a texture another producer on the same device owns. */
  FeatureSupport externalImages;

  /** `Canvas::maskLayer`: an offscreen layer masked by a bitmap (rounded clips use it). */
  FeatureSupport layerMasks;

  /** Shaped clips through `Canvas::clipPath`, drawn as a coverage mask. */
  FeatureSupport shapedClips;

  /** Group opacity through `Canvas::saveLayerAlpha`. */
  FeatureSupport groupOpacity;

  /** Blend modes beyond source-over, including those realised by isolating a layer. */
  FeatureSupport blendModes;

  /**
   * Partial redraw driven by `Window::invalidate` damage rectangles.
   *
   * A hint rather than a pixel guarantee: the renderer scissor and `LoadOp::Load` implement it where
   * the platform preserves its target; a platform that does not still repaints the window and remains
   * correct.
   */
  FeatureSupport damageRects;

  /** Reading a submitted frame back to the CPU (`Surface::snapshot`, `Window::snapshot`). */
  FeatureSupport readback;

  /** The capabilities of a software raster canvas. */
  [[nodiscard]] static RendererCapabilities raster();
};

/**
 * Probes a device into the UI-facing snapshot.
 *
 * @param device the RHI device the window renders through, or null for a software canvas.
 * @param sampleCount the frame target's sample count, which the caller owns (the platform's surface).
 * @param recording true for a canvas that records GPU command lists; false for raster.
 *
 * Everything the UI reports as unsupported here is a real limitation of the current renderer, not a
 * failure to probe. `shapedClips` is supported on the GPU path, where a masked run is realised as a
 * mask-composited layer rather than by multiplying per command; `damageRects` is a partial-update
 * hint and platforms without a preserved target fall back to a full repaint.
 */
[[nodiscard]] RendererCapabilities queryRendererCapabilities(backend::Device* device,
  bool recording, uint32_t sampleCount);

} // namespace coral::ui

#endif // CORAL_UI_RENDERERCAPABILITIES_H
