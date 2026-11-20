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

#ifndef CORAL_UI_CONTENTHOST_H
#define CORAL_UI_CONTENTHOST_H

#include <ui/Geometry.h>
#include <ui/ContentFrame.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <mutex>
#include <vector>

namespace coral::ui {

/**
 * A budget shared by every preview in a window.
 *
 * Each `ContentHost` has its own in-flight cap; a page with many previews needs one cap across the
 * producers so eight thumbnails cannot reserve eight independent rings. Acquire before a producer
 * renders, release after the frame is consumed or the preview becomes inactive.
 */
class ContentFrameBudget {
 public:
  explicit ContentFrameBudget(uint32_t maxSlots = 8);

  void setMaxSlots(uint32_t maxSlots);
  [[nodiscard]] uint32_t maxSlots() const;
  [[nodiscard]] uint32_t inUse() const;

  [[nodiscard]] bool tryAcquire();
  void release();

 private:
  mutable std::mutex mMutex;
  uint32_t mMaxSlots = 8;
  uint32_t mInUse = 0;
};

/**
 * The coordination point between a content producer and the view that shows it.
 *
 * The producer owns the texture (an effect graph's `TextureOutputNode`, a render surface); the host
 * only remembers the latest frame and the requests the UI makes. Nothing is copied and no device
 * object is created here, which is what keeps the embedding zero-copy.
 *
 * ## Who calls what, on which thread
 *
 * | call | thread | purpose |
 * |---|---|---|
 * | `publish` | producer | hand over the newest frame; invokes the frame-available callback |
 * | `setFrameAvailableCallback` | UI | wake the UI so it can request a frame |
 * | `setActive` | UI | stop the producer when the preview is hidden or scrolled away |
 * | `setRequestedSize` | UI | ask the producer for a device-size that matches the view |
 *
 * `publish`/`latest`/`generation` are mutex-guarded so a producer on a media thread and a UI frame on
 * the render thread cannot tear the record. The texture itself must outlive the frame it was published
 * for, which is the producer's own lifetime contract (a `TextureOutputNode` keeps its ring slots in
 * flight; a `gpu::Surface` producer must not submit over the target it published).
 */
class ContentHost {
 public:
  /** Called on the producer's thread after a publish; the UI hops to its own thread from there. */
  using FrameAvailableCallback = std::function<void()>;
  /** Called after device recovery has invalidated the published frame; rebuild and republish here. */
  using RecoveryCallback = std::function<void()>;
  /** Called when the view hides/shows the preview, outside the host lock. */
  using ActiveChangedCallback = std::function<void(bool)>;
  /**
   * Waits for the published frame's GPU work to finish.
   *
   * The producer owns the fence: a `TextureOutputNode` passes a wait for its ring slot, a surface
   * producer passes a device-fence wait. The view calls it immediately before recording the sample,
   * so a frame cannot be consumed while its slot is still being written. Empty means "already safe".
   */
  using FrameFence = std::function<void()>;

  /** A producer-side in-flight reservation. Invalid when the cap is reached. */
  struct Slot {
    uint32_t id = 0;
    [[nodiscard]] bool isValid() const noexcept { return id != 0; }
  };

  /** Maximum frames a producer may have reserved at once. Defaults to two. */
  void setMaxInFlight(uint32_t count);
  [[nodiscard]] uint32_t maxInFlight() const;
  [[nodiscard]] uint32_t inFlight() const;

  /** Reserves an in-flight slot, or null when the producer should skip this frame. */
  [[nodiscard]] std::optional<Slot> acquireSlot();

  /** Publishes through a slot; the host releases pending slots after the frame fence runs. */
  void publish(Slot slot, const ContentFrame& image, FrameFence fence = {});

  /** Releases a reservation manually. Safe to call twice. */
  void releaseSlot(Slot slot);

  /** Replaces the latest frame. An invalid image clears the host. Does not block on the callback. */
  void publish(const ContentFrame& image, FrameFence fence = {});

  /** Waits for the latest frame's fence, if one was published. */
  void waitForLatestFrame();

  /**
   * Releases the in-flight reservations consumed by the latest sample.
   *
   * `waitForLatestFrame()` does this after its fence; a threaded consumer executes the fence on the
   * render thread and calls this afterwards, so the producer can reuse the slots.
   */
  void releaseConsumedFrame();

  /** The frame to draw, or an invalid frame when nothing has been published yet. */
  [[nodiscard]] ContentFrame latest() const;

  /**
   * The latest frame and its fence, taken together.
   *
   * This is the render-thread form of `waitForLatestFrame()` + `latest()`: the caller records the
   * fence into the frame packet and executes it on the GPU thread, so the UI thread never blocks on
   * a producer's ring slot.
   */
  struct LatestSample {
    ContentFrame frame;
    FrameFence fence;
  };
  [[nodiscard]] LatestSample latestSample() const;

  /** Monotonic count of successful publishes; the UI can use it to skip redraws. */
  [[nodiscard]] uint64_t generation() const;

  /**
   * Whether the producer should keep rendering.
   *
   * The view sets this from its visibility: a preview that is hidden, collapsed or scrolled out of
   * its viewport stops asking for frames instead of decoding behind the page. Defaults to true.
   */
  void setActive(bool active);
  [[nodiscard]] bool active() const;

  /**
   * The size the view wants the producer to render at, in **device** pixels.
   *
   * The view derives it from its logical bounds and the frame's `uiScale`, so a DPR change or a dock
   * resize asks for a matching target instead of stretching the old frame. A producer polls this when
   * it configures its output (an effect engine's `setRenderSize`, a surface's resize).
   */
  void setRequestedSize(const Size& size);
  [[nodiscard]] Size requestedSize() const;

  /**
   * Subscribes to "a new frame is available".
   *
   * The callback runs on the publishing thread, so it must be cheap and must not touch the layer
   * tree: the usual body is `window->post([window] { window->requestFrame(); })`.
   */
  void setFrameAvailableCallback(FrameAvailableCallback callback);

  /** Sets the producer/engine callback that runs after device recovery invalidates the frame. */
  void setRecoveryCallback(RecoveryCallback callback);

  /**
   * Invalidates the published frame because the GPU device was lost, then invokes the recovery
   * callback. The producer should call `EffectEngine::recover()` and publish a fresh frame.
   */
  void notifyDeviceRecovered();

  /** Reports active changes (visibility) outside the lock; an engine can pause/resume here. */
  void setActiveChangedCallback(ActiveChangedCallback callback);

  /**
   * Assigns a shared budget. Set it before producers acquire slots; changing it while slots are
   * outstanding is rejected by keeping the previous budget for those slots.
   */
  void setFrameBudget(std::shared_ptr<ContentFrameBudget> budget);
  [[nodiscard]] std::shared_ptr<ContentFrameBudget> frameBudget() const;

  void clear();

 private:
  mutable std::mutex mMutex;
  ContentFrame mImage;
  FrameFence mFence;
  std::vector<uint32_t> mPendingSlots;
  std::vector<uint32_t> mReservedSlots;
  uint32_t mMaxInFlight = 3;
  uint32_t mNextSlot = 1;
  uint64_t mGeneration = 0;
  bool mActive = true;
  Size mRequestedSize{};
  FrameAvailableCallback mFrameAvailable;
  RecoveryCallback mRecovery;
  ActiveChangedCallback mActiveChanged;
  std::shared_ptr<ContentFrameBudget> mBudget;
};

} // namespace coral::ui

#endif // CORAL_UI_CONTENTHOST_H
