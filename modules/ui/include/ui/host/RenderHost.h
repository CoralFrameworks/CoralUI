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

#ifndef CORAL_UI_HOST_RENDERHOST_H
#define CORAL_UI_HOST_RENDERHOST_H

#include <ui/host/FramePacket.h>

#include <ui/RendererCapabilities.h>
#include <ui/Types.h>

#include <cstdint>

namespace coral::ui::host {

/**
 * The UI thread's view of the frame sink.
 *
 * A render host owns the thread and the device; the UI thread only records packets and calls
 * `submit`. That is deliberately narrower than `ui::Renderer`: a host has no `renderFrame` because
 * the UI thread must not run one, and it has no `recover` because device loss is reported through
 * the completion/queue channels rather than driven from the recording thread.
 *
 * `ImmediateRenderHost` (P1) keeps the old single-threaded behaviour for tests and for platforms
 * that cannot split; a `ThreadedRenderHost` (P1) owns a `RenderThread`. Both implement this
 * interface, so a `Window` does not branch on which host it has.
 */
class RenderHost {
 public:
  virtual ~RenderHost() = default;

  /**
   * Hands one frame to the host.
   *
   * The call is asynchronous: returning true means the packet was accepted, not that it was
   * presented. `packet.completion` is filled when the frame reaches the display or fails. A host may
   * drop an older pending packet (latest-wins); it must not block the UI thread on the device.
   */
  virtual bool submit(FramePacket packet) = 0;

  /**
   * Re-renders the last frame into @p out.
   *
   * Screenshots and tests need pixels, and a threaded host cannot read them without a round-trip;
   * this is the one explicitly blocking operation. Production code uses it for export/debug only.
   */
  virtual bool snapshot(uint64_t windowId, Bitmap* out) = 0;

  /** Platform capabilities of the host's device; valid after the host is initialized. */
  [[nodiscard]] virtual const RendererCapabilities& capabilities() const = 0;

  /** Requests the host to stop accepting frames and drain its queue. Must not block on the UI thread. */
  virtual void shutdown() {}
};

} // namespace coral::ui::host

#endif // CORAL_UI_HOST_RENDERHOST_H
