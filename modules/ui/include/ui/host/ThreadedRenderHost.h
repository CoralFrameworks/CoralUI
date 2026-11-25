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

#ifndef CORAL_UI_HOST_THREADEDRENDERHOST_H
#define CORAL_UI_HOST_THREADEDRENDERHOST_H

#include <ui/host/PreviewScheduler.h>
#include <ui/host/RenderHost.h>

#include <ui/Renderer.h>

#include <backend/Device.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace backend {
class Device;
}

namespace coral::ui::host {

/**
 * A render host with one dedicated render thread and one or more window targets.
 *
 * A session (an `Application`) owns one host and registers every window with `addTarget`; all
 * targets share the host's device and context, which is what makes cross-window content sampling
 * possible. The first target creates the device (and transfers ownership to the host); later
 * targets borrow it with `makeOn`. A single-window `Window` uses the `Options` constructor and the
 * implicit target 0.
 *
 * The host is deliberately small: per-target surfaces, a latest-wins packet queue, resize and
 * snapshot requests. UI-thread recording never touches the device.
 */
class ThreadedRenderHost final : public RenderHost {
 public:
  enum class NativeWindowKind {
    Unknown,
    Windows,
    X11,
    Wayland,
  };

  struct Options {
    uint32_t width = 0;
    uint32_t height = 0;
    RendererApi rendererApi = RendererApi::Auto;
    /**
     * A native window handle (HWND / X11 Window / wl_surface) when the host should present to a
     * window. Null means an offscreen target (`gpu::Surface`), which is the headless/test path.
     */
    void* nativeWindow = nullptr;
    /** X11 `Display*` / Wayland `wl_display*`; ignored on Windows and for offscreen targets. */
    void* nativeDisplay = nullptr;
    NativeWindowKind nativeWindowKind = NativeWindowKind::Unknown;
    /** Swapchain colour space requested by the owning window. */
    backend::SwapchainColorSpace outputColorSpace = backend::SwapchainColorSpace::Srgb;
    const char* label = "ui-threaded-host";
  };

  using PresentationCallback = std::function<void(const FrameCompletion&)>;

  /** Creates a host with one target (the single-window path). */
  explicit ThreadedRenderHost(Options options);
  /** Creates a host with no target; register windows with `addTarget`. */
  ThreadedRenderHost();
  ~ThreadedRenderHost() override;

  ThreadedRenderHost(const ThreadedRenderHost&) = delete;
  ThreadedRenderHost& operator=(const ThreadedRenderHost&) = delete;

  /** The target created by the `Options` constructor, or 0 for a shared host. */
  [[nodiscard]] uint64_t primaryTarget() const noexcept;

  /**
   * The device shared by all targets.
   *
   * Advanced: a `PreviewSession` starts an external-drive engine with this pointer. `start()` must run
   * on the render thread (use `postTask`), because the device context is current there.
   */
  [[nodiscard]] backend::Device* device() const noexcept;

  /** Queues @p task to run on the render thread before the next preview tick. Safe from any thread. */
  bool postTask(std::function<void()> task);

  /** The preview scheduler owned by the render thread; mutate it from `postTask`, not from the UI loop. */
  [[nodiscard]] PreviewScheduler& previewScheduler() noexcept;
  [[nodiscard]] const PreviewScheduler& previewScheduler() const noexcept;

  /** Registers a target and waits until its surface is ready. @return 0 on failure. */
  uint64_t addTarget(const Options& options);

  /** Waits for a target registered with `addTarget`. */
  [[nodiscard]] bool waitForTarget(uint64_t windowId);

  /** Removes a target; safe to call twice. */
  void removeTarget(uint64_t windowId);

  /** Blocks until the primary target is up (or failed). */
  [[nodiscard]] bool waitForReady();

  /**
   * Receives every completed frame on the render thread.
   *
   * The UI host marshals this to its own loop (usually `Window::post`) before touching the layer
   * tree or `FrameClock`. The callback must not block: the render thread is the only thread that
   * can present the next frame. `FrameCompletion::windowId` names the target.
   */
  void setPresentationCallback(PresentationCallback callback);

  /** Per-target callback; preferred when several windows share the host. */
  void setPresentationCallback(uint64_t windowId, PresentationCallback callback);

  bool submit(FramePacket packet) override;
  /** Recreates the device and every target surface on the render thread, preserving target ids/options. */
  bool recover();

  /** Requests a new target size; the render thread resizes/recreates the surface before the next frame. */
  void resize(uint64_t windowId, uint32_t width, uint32_t height);
  /** Convenience for the single-window target. */
  void resize(uint32_t width, uint32_t height) {
    resize(primaryTarget(), width, height);
  }
  bool snapshot(uint64_t windowId, Bitmap* out) override;
  [[nodiscard]] const RendererCapabilities& capabilities() const override;
  [[nodiscard]] const RendererCapabilities& capabilitiesFor(uint64_t windowId) const;
  [[nodiscard]] RendererKind kind() const noexcept;
  void shutdown() override;

  /** The render thread's id, exposed for the thread-affinity test. */
  [[nodiscard]] std::thread::id renderThreadId() const noexcept;
  /** Per-target presentation statistics; independent between windows sharing a host. */
  struct TargetStats {
    uint64_t submitted = 0;
    uint64_t failed = 0;
    uint64_t dropped = 0;
    uint64_t lastSequence = 0;
    double lastRenderMilliseconds = 0.0;
    double lastPresentMilliseconds = 0.0;
    std::chrono::steady_clock::time_point lastPresentedAt{};
    /** Packets queued for this target but not yet rendered; 0 or 1 for the latest-wins queue. */
    uint32_t queued = 0;
  };
  [[nodiscard]] TargetStats targetStats(uint64_t windowId) const;
  /**
   * @return true when @p windowId presents into a native window rather than an offscreen surface.
   *
   * A platform that cannot expose its window handle gets an offscreen target: frames and snapshots
   * succeed, but nothing reaches the screen. Tests and diagnostics use this to avoid treating that
   * fallback as on-screen presentation.
   */
  [[nodiscard]] bool targetIsWindow(uint64_t windowId) const;
  [[nodiscard]] uint64_t submittedFrames() const noexcept;
  [[nodiscard]] uint64_t droppedFrames() const noexcept;
  [[nodiscard]] size_t targetCount() const;
  [[nodiscard]] std::string diagnostics() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> mImpl;
};

} // namespace coral::ui::host

#endif // CORAL_UI_HOST_THREADEDRENDERHOST_H
