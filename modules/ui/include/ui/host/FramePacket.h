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

#ifndef CORAL_UI_HOST_FRAMEPACKET_H
#define CORAL_UI_HOST_FRAMEPACKET_H

#include <ui/ContentFrame.h>
#include <ui/ContentHost.h>
#include <ui/Geometry.h>
#include <ui/Types.h>

#include <gpu/DrawList.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <condition_variable>
#include <utility>
#include <vector>

namespace coral::ui {

/**
 * The render thread's completion record for one packet.
 *
 * The UI thread keeps the packet's shared state until it no longer needs the presentation timing or
 * the fence result. A render host fills it after `present`, and the UI thread may then recycle any
 * per-frame bookkeeping. It is a value the GPU side owns; the UI side only reads it after the
 * completion is signalled.
 *
 * Exactly one of `presented`, `dropped` and `failed` settles for a packet that a host accepted:
 * a newer packet for the same window may replace it before it is rendered (`dropped`), the present
 * may succeed (`presented`) or the surface may reject it (`failed`). Nothing ever waits forever.
 */
struct FrameCompletion {
  std::atomic<bool> presented{false};
  std::atomic<bool> failed{false};
  /** The packet was superseded by a newer one for the same window before it reached the surface. */
  std::atomic<bool> dropped{false};
  double presentMilliseconds = 0.0;
  double renderMilliseconds = 0.0;
  uint64_t sequence = 0;
  uint64_t windowId = 0;
  /** When the UI thread started recording; used for the submitted-to-presented total. */
  std::chrono::steady_clock::time_point submittedAt{};
  /** When the render thread observed the present. */
  std::chrono::steady_clock::time_point presentedAt{};
  std::shared_ptr<void> owner; // optional: keeps a backend resource alive until completion
};

/**
 * One immutable UI frame, recorded on the UI thread and submitted to the render thread.
 *
 * `commands` is the device-space `DrawList` recorded by `gpu::Recorder`; it is not a second display
 * list format. `contentFrames` keeps every externally produced texture view alive until the render
 * thread has submitted the commands, and `fences` are executed on the render thread immediately
 * before sampling (never on the UI thread).
 *
 * The packet deliberately carries no `WindowSurface`, `Device` or thread handle: those live in the
 * render host, so a packet can be copied/moved through a queue and inspected by tests.
 */
/**
 * One list a frame lends instead of recording: a repaint boundary, a composited viewport or an overlay.
 *
 * The packet carries these beside `commands` so the render thread can submit the frame as several lists
 * (which is what the frame plan produces) without the UI thread flattening them into one recording.
 */
struct FrameBoundaryList {
  std::shared_ptr<const graphics::gpu::DrawList> list;
  /** Device-pixel clip; empty means the whole target. */
  Rect clip{};
  /** Device-pixel translation applied when the list is submitted. */
  Point translation{};
};

struct FramePacket {
  uint64_t sequence = 0;
  uint64_t windowId = 0;
  std::shared_ptr<const graphics::gpu::DrawList> commands;
  /**
   * The lists the frame lends, in paint order after `commands`.
   *
   * Empty for an ordinary frame. When not empty the render thread submits `commands` and then these, so
   * the frame reaches the device with no copy of any list.
   */
  std::vector<FrameBoundaryList> boundaries;
  /** Device-pixel damage; an empty rectangle means a full repaint. */
  Rect damage{};
  Size pixelSize{};
  float uiScale = 1.0f;
  std::vector<ContentFrame> contentFrames;
  std::vector<ContentHost::FrameFence> fences;
  std::shared_ptr<FrameCompletion> completion;

  [[nodiscard]] bool hasCommands() const;
};

/**
 * A bounded latest-wins queue between the UI thread and the render thread.
 *
 * A UI frame has no value once a newer one exists: queueing a backlog would add input-to-present
 * latency without improving the picture. `push` therefore drops the oldest packet when the bound is
 * reached, and `popLatest` always returns the newest. The counters are what a diagnostics overlay
 * reports as "dropped UI frames".
 */
class LatestFrameQueue {
 public:
  explicit LatestFrameQueue(std::size_t capacity = 2)
    : mCapacity(capacity == 0 ? 1 : capacity) {}

  LatestFrameQueue(const LatestFrameQueue&) = delete;
  LatestFrameQueue& operator=(const LatestFrameQueue&) = delete;

  /** @return true when a packet was dropped to make room. */
  bool push(FramePacket packet) {
    bool dropped = false;
    {
      const std::lock_guard<std::mutex> lock(mMutex);
      while (mPackets.size() >= mCapacity) {
        mPackets.pop_front();
        ++mDropped;
        dropped = true;
      }
      mPackets.push_back(std::move(packet));
    }
    mWait.notify_one();
    return dropped;
  }

  /** Pops the newest packet if one is queued; older packets are dropped. */
  [[nodiscard]] std::optional<FramePacket> popLatest() {
    const std::lock_guard<std::mutex> lock(mMutex);
    if (mPackets.empty()) return std::nullopt;
    FramePacket newest = std::move(mPackets.back());
    const std::size_t older = mPackets.size() - 1;
    mPackets.clear();
    mDropped += older;
    return newest;
  }

  /** Waits up to @p timeout for a packet and returns the newest, or null on timeout. */
  [[nodiscard]] std::optional<FramePacket> waitLatest(const std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mMutex);
    if (!mWait.wait_for(lock, timeout, [this] { return !mPackets.empty(); })) {
      return std::nullopt;
    }
    FramePacket newest = std::move(mPackets.back());
    const std::size_t older = mPackets.size() - 1;
    mPackets.clear();
    mDropped += older;
    return newest;
  }

  [[nodiscard]] std::size_t size() const {
    const std::lock_guard<std::mutex> lock(mMutex);
    return mPackets.size();
  }

  [[nodiscard]] uint64_t dropped() const {
    const std::lock_guard<std::mutex> lock(mMutex);
    return mDropped;
  }

  void clear() {
    const std::lock_guard<std::mutex> lock(mMutex);
    mPackets.clear();
  }

 private:
  std::size_t mCapacity;
  mutable std::mutex mMutex;
  std::condition_variable mWait;
  std::deque<FramePacket> mPackets;
  uint64_t mDropped = 0;
};

} // namespace coral::ui

#endif // CORAL_UI_HOST_FRAMEPACKET_H
