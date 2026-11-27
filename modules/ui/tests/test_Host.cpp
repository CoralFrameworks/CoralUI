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

#include <ui/host/FramePacket.h>
#include <ui/host/PreviewScheduler.h>
#include <ui/host/RenderHost.h>
#include <ui/host/ThreadAffinity.h>
#include <ui/host/ThreadedRenderHost.h>

#include <graphics/Canvas.h>
#include <graphics/Paint.h>
#include <gpu/Recorder.h>

#include <gtest/gtest.h>

#include "TestSupport.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace {

using namespace coral::ui;

namespace {

class NullRenderHost final : public host::RenderHost {
 public:
  bool submit(FramePacket packet) override {
    lastSequence = packet.sequence;
    return true;
  }

  bool snapshot(uint64_t, Bitmap*) override {
    return false;
  }

  [[nodiscard]] const RendererCapabilities& capabilities() const override {
    static const RendererCapabilities raster = RendererCapabilities::raster();
    return raster;
  }

  uint64_t lastSequence = 0;
};

} // namespace

TEST(UiHost, RenderHostAcceptsAFramePacketAndReportsCapabilities) {
  NullRenderHost host;
  FramePacket packet;
  packet.sequence = 42;
  EXPECT_TRUE(host.submit(std::move(packet)));
  EXPECT_EQ(host.lastSequence, 42u);
  EXPECT_FALSE(host.capabilities().recording);
  EXPECT_FALSE(host.snapshot(1, nullptr));
}

TEST(UiHost, LatestFrameQueueKeepsOnlyTheNewestPacket) {
  LatestFrameQueue queue(2);
  for (uint64_t sequence = 1; sequence <= 4; ++sequence) {
    FramePacket packet;
    packet.sequence = sequence;
    packet.windowId = 7;
    queue.push(std::move(packet));
  }
  EXPECT_EQ(queue.size(), 2u);
  EXPECT_EQ(queue.dropped(), 2u);

  const auto newest = queue.popLatest();
  ASSERT_TRUE(newest.has_value());
  EXPECT_EQ(newest->sequence, 4u);
  EXPECT_EQ(queue.size(), 0u);
  EXPECT_FALSE(queue.popLatest().has_value());
}

TEST(UiHost, WaitingForAFrameReturnsTheNewest) {
  LatestFrameQueue queue(4);
  FramePacket first;
  first.sequence = 1;
  queue.push(std::move(first));
  FramePacket second;
  second.sequence = 2;
  queue.push(std::move(second));

  const auto newest = queue.waitLatest(std::chrono::milliseconds(1));
  ASSERT_TRUE(newest.has_value());
  EXPECT_EQ(newest->sequence, 2u);
  EXPECT_EQ(queue.dropped(), 1u) << "the older queued packet is superseded";
  EXPECT_FALSE(queue.waitLatest(std::chrono::milliseconds(0)).has_value());
}

TEST(UiHost, ThreadAffinityBindsOnFirstUseAndRejectsAnotherThread) {
  host::ThreadAffinity affinity("render");
  EXPECT_FALSE(affinity.isBound());
  EXPECT_TRUE(affinity.isCurrentThread());

  affinity.bindToCurrentThread();
  EXPECT_TRUE(affinity.isBound());
  EXPECT_TRUE(affinity.isCurrentThread());

  bool otherThread = true;
  std::thread worker([&] { otherThread = affinity.isCurrentThread(); });
  worker.join();
  EXPECT_FALSE(otherThread);
}

TEST(UiHost, ThreadAffinityCanRebindForAReplacementThread) {
  host::ThreadAffinity affinity("ui");
  affinity.bindToCurrentThread();
  std::thread::id first;
  {
    std::thread worker([&] {
      first = std::this_thread::get_id();
      affinity.bindToCurrentThread(true);
      EXPECT_TRUE(affinity.isCurrentThread());
    });
    worker.join();
  }
  EXPECT_FALSE(affinity.isCurrentThread()) << "the rebind moved ownership to the worker";
  EXPECT_NE(first, std::this_thread::get_id());
}

TEST(UiHost, ANullContextRecorderStillReportsARecordingCanvas) {
  graphics::gpu::Recorder recorder(16, 16, nullptr);
  graphics::Canvas& canvas = recorder.canvas();
  EXPECT_TRUE(canvas.isRecording());
  graphics::Paint paint;
  paint.color = Color::MakeRGBA8(202, 40, 60, 255);
  canvas.drawRect(Rect::MakeWH(16.0f, 16.0f), paint);
  const graphics::gpu::DrawList list = recorder.finish();
  EXPECT_FALSE(list.isEmpty()) << "the recording must reach the DrawList without a device context";
}

TEST(UiHost, AThreadedHostSubmitsAndSnapshotsOnItsOwnThread) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  CORAL_REQUIRE_RECORDING_BACKEND();
  host::ThreadedRenderHost::Options options;
  options.width = 16;
  options.height = 16;
  options.label = "ui-host-test";
  host::ThreadedRenderHost host(options);
  ASSERT_TRUE(host.waitForReady()) << host.diagnostics();
  EXPECT_NE(host.renderThreadId(), std::this_thread::get_id());
  EXPECT_EQ(host.kind(), RendererKind::GPU);
  std::atomic<int> completed{0};
  host.setPresentationCallback([&](const FrameCompletion& completion) {
    EXPECT_TRUE(completion.presented.load());
    ++completed;
  });

  graphics::gpu::Recorder recorder(16, 16, nullptr);
  graphics::Paint paint;
  paint.color = Color::MakeRGBA8(30, 160, 90, 255);
  recorder.canvas().drawRect(Rect::MakeWH(16.0f, 16.0f), paint);
  std::atomic<bool> fenceRan{false};
  std::thread::id fenceThread;
  FramePacket packet;
  packet.sequence = 1;
  packet.commands = std::make_shared<const graphics::gpu::DrawList>(recorder.finish());
  packet.completion = std::make_shared<FrameCompletion>();
  packet.fences.push_back([&] {
    fenceRan.store(true);
    fenceThread = std::this_thread::get_id();
  });
  ASSERT_TRUE(host.submit(std::move(packet)));

  Bitmap bitmap;
  ASSERT_TRUE(host.snapshot(0, &bitmap));
  const auto pixels = bitmap.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  const uint32_t pixel = pixels.getPixelARGB32(8, 8);
  EXPECT_EQ((pixel >> 16) & 0xFFu, 30u);
  EXPECT_EQ((pixel >> 8) & 0xFFu, 160u);
  EXPECT_EQ(pixel & 0xFFu, 90u);
  EXPECT_GE(host.submittedFrames(), 1u);
  EXPECT_GE(completed.load(), 1);
  EXPECT_TRUE(fenceRan.load());
  EXPECT_EQ(fenceThread, host.renderThreadId());
  EXPECT_NE(fenceThread, std::this_thread::get_id());
}

TEST(UiHost, AReplacedPacketSettlesAsDroppedInsteadOfHanging) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  CORAL_REQUIRE_RECORDING_BACKEND();
  host::ThreadedRenderHost::Options options;
  options.width = 8;
  options.height = 8;
  options.label = "ui-host-drop-test";
  host::ThreadedRenderHost host(options);
  ASSERT_TRUE(host.waitForReady()) << host.diagnostics();

  // Hold the render thread so both packets are queued; the second one then replaces the first.
  std::atomic<bool> blocked{false};
  std::atomic<bool> release{false};
  ASSERT_TRUE(host.postTask([&] {
    blocked.store(true, std::memory_order_release);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!release.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }));
  for (int attempt = 0; attempt < 2000 && !blocked.load(std::memory_order_acquire); ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_TRUE(blocked.load(std::memory_order_acquire)) << "the render thread never started the task";

  graphics::gpu::Recorder recorder(8, 8, nullptr);
  graphics::Paint paint;
  paint.color = Color::MakeRGBA8(10, 20, 30, 255);
  recorder.canvas().drawRect(Rect::MakeWH(8.0f, 8.0f), paint);
  auto list = std::make_shared<const graphics::gpu::DrawList>(recorder.finish());

  auto firstCompletion = std::make_shared<FrameCompletion>();
  FramePacket first;
  first.sequence = 1;
  first.commands = list;
  first.completion = firstCompletion;
  ASSERT_TRUE(host.submit(std::move(first)));

  auto secondCompletion = std::make_shared<FrameCompletion>();
  FramePacket second;
  second.sequence = 2;
  second.commands = list;
  second.completion = secondCompletion;
  ASSERT_TRUE(host.submit(std::move(second)));

  // Latest-wins must settle the replaced packet: a caller waiting on it would hang otherwise.
  EXPECT_TRUE(firstCompletion->dropped.load(std::memory_order_acquire));
  EXPECT_FALSE(firstCompletion->presented.load(std::memory_order_acquire));
  EXPECT_FALSE(firstCompletion->failed.load(std::memory_order_acquire));

  release.store(true, std::memory_order_release);
  for (int attempt = 0; attempt < 5000 && !secondCompletion->presented.load(std::memory_order_acquire);
       ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_TRUE(secondCompletion->presented.load(std::memory_order_acquire)) << host.diagnostics();
  EXPECT_FALSE(secondCompletion->dropped.load(std::memory_order_acquire));
}

TEST(UiHost, PreviewSchedulerPrioritisesVisibleDueWork) {
  host::PreviewScheduler scheduler;
  std::vector<uint64_t> order;
  const uint64_t low = scheduler.add([&] { order.push_back(1); }, 0.0, 1);
  const uint64_t high = scheduler.add([&] { order.push_back(2); }, 0.0, 10);
  const uint64_t hidden = scheduler.add([&] { order.push_back(3); }, 0.0, 100);
  scheduler.setActive(hidden, false);
  scheduler.setMaxRendersPerTick(1);
  scheduler.tick(std::chrono::steady_clock::now());
  ASSERT_EQ(order.size(), 1u);
  EXPECT_EQ(order.front(), 2u) << "the highest-priority visible preview runs first";
  EXPECT_EQ(scheduler.stats().budgetDropped, 1u);

  order.clear();
  scheduler.setMaxRendersPerTick(0);
  scheduler.tick(std::chrono::steady_clock::now());
  ASSERT_EQ(order.size(), 2u);
  EXPECT_EQ(order[0], 2u);
  EXPECT_EQ(order[1], 1u);
  scheduler.remove(low);
  scheduler.remove(high);
  EXPECT_EQ(scheduler.size(), 1u) << "the hidden preview remains registered";
  EXPECT_TRUE(scheduler.setActive(hidden, true));
}

TEST(UiHost, PreviewSchedulerRecoversVisiblePreviewsInPriorityOrder) {
  host::PreviewScheduler scheduler;
  std::vector<int> order;
  const uint64_t low = scheduler.add([] {}, 0.0, 1,
    [&] { order.push_back(1); return true; });
  const uint64_t high = scheduler.add([] {}, 0.0, 10,
    [&] { order.push_back(2); return false; });
  const uint64_t hidden = scheduler.add([] {}, 0.0, 100,
    [&] { order.push_back(3); return true; });
  scheduler.setActive(hidden, false);

  EXPECT_EQ(scheduler.recoverAll(), 1u);
  ASSERT_EQ(order.size(), 2u);
  EXPECT_EQ(order[0], 2) << "higher priority recovers first";
  EXPECT_EQ(order[1], 1);
  EXPECT_EQ(scheduler.recoverAll(), 1u) << "only active previews are recovered";

  EXPECT_TRUE(scheduler.setRecoveryCallback(low, [&] { order.push_back(4); return true; }));
  order.clear();
  EXPECT_EQ(scheduler.recoverAll(), 1u);
  ASSERT_EQ(order.size(), 2u);
  EXPECT_EQ(order[0], 2);
  EXPECT_EQ(order[1], 4);
}

TEST(UiHost, PreviewSchedulerRunsOnTheRenderThread) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  CORAL_REQUIRE_RECORDING_BACKEND();
  host::ThreadedRenderHost::Options options;
  options.width = 8;
  options.height = 8;
  host::ThreadedRenderHost host(options);
  ASSERT_TRUE(host.waitForReady()) << host.diagnostics();

  std::atomic<int> renders{0};
  std::thread::id renderThread;
  ASSERT_TRUE(host.postTask([&] {
    host.previewScheduler().add([&] {
      renderThread = std::this_thread::get_id();
      renders.fetch_add(1);
    });
  }));
  for (int i = 0; i < 200 && renders.load() == 0; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  EXPECT_GE(renders.load(), 1);
  EXPECT_EQ(renderThread, host.renderThreadId());
  EXPECT_NE(renderThread, std::this_thread::get_id());
}

TEST(UiHost, OneHostSharesItsDeviceAcrossTargets) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  CORAL_REQUIRE_RECORDING_BACKEND();
  host::ThreadedRenderHost host;
  host::ThreadedRenderHost::Options first;
  first.width = 16;
  first.height = 16;
  first.label = "target-1";
  const uint64_t firstId = host.addTarget(first);
  ASSERT_NE(firstId, 0u) << host.diagnostics();

  host::ThreadedRenderHost::Options second = first;
  second.label = "target-2";
  const uint64_t secondId = host.addTarget(second);
  ASSERT_NE(secondId, 0u) << host.diagnostics();
  EXPECT_NE(firstId, secondId);
  EXPECT_EQ(host.targetCount(), 2u);

  auto submitSolid = [&](const uint64_t id, const Color color) {
    graphics::gpu::Recorder recorder(16, 16, nullptr);
    graphics::Paint paint;
    paint.color = color;
    recorder.canvas().drawRect(Rect::MakeWH(16.0f, 16.0f), paint);
    FramePacket packet;
    packet.windowId = id;
    packet.commands = std::make_shared<const graphics::gpu::DrawList>(recorder.finish());
    packet.completion = std::make_shared<FrameCompletion>();
    EXPECT_TRUE(host.submit(std::move(packet)));
  };
  submitSolid(firstId, Color::MakeRGBA8(10, 20, 30, 255));
  submitSolid(secondId, Color::MakeRGBA8(200, 100, 40, 255));

  Bitmap firstBitmap;
  ASSERT_TRUE(host.snapshot(firstId, &firstBitmap));
  ASSERT_TRUE(firstBitmap.makePixmap().isValid());
  EXPECT_EQ(firstBitmap.makePixmap().getPixelARGB32(8, 8) & 0xFFu, 30u);

  Bitmap secondBitmap;
  ASSERT_TRUE(host.snapshot(secondId, &secondBitmap));
  ASSERT_TRUE(secondBitmap.makePixmap().isValid());
  EXPECT_EQ((secondBitmap.makePixmap().getPixelARGB32(8, 8) >> 16) & 0xFFu, 200u);

  const auto firstStats = host.targetStats(firstId);
  const auto secondStats = host.targetStats(secondId);
  EXPECT_EQ(firstStats.submitted, 1u);
  EXPECT_EQ(secondStats.submitted, 1u);
  EXPECT_EQ(firstStats.failed, 0u);
  EXPECT_GT(firstStats.lastPresentedAt.time_since_epoch().count(), 0);
  ASSERT_TRUE(host.recover()) << host.diagnostics();
  EXPECT_EQ(host.targetCount(), 2u) << "recovery preserves every shared target";
  EXPECT_TRUE(host.waitForTarget(firstId));
  EXPECT_TRUE(host.waitForTarget(secondId));

  host.removeTarget(firstId);
  EXPECT_EQ(host.targetCount(), 1u);
}

TEST(UiHost, APacketWithoutCommandsIsEmptyButStillCarriesIdentity) {
  FramePacket packet;
  packet.windowId = 3;
  packet.pixelSize = Size{ 320.0f, 200.0f };
  EXPECT_FALSE(packet.hasCommands());
  EXPECT_EQ(packet.windowId, 3u);
  EXPECT_FALSE(packet.completion);
}

} // namespace

TEST(UiHost, SchedulerDoesNotStarveBudgetDroppedPreviews) {
  coral::ui::host::PreviewScheduler scheduler;
  scheduler.setMaxRendersPerTick(1);
  int high = 0;
  int lowA = 0;
  int lowB = 0;
  const auto now = coral::ui::host::PreviewScheduler::Clock::now();
  scheduler.add([&high] { ++high; }, 0.0, 10);
  scheduler.add([&lowA] { ++lowA; }, 0.0, 0);
  scheduler.add([&lowB] { ++lowB; }, 0.0, 0);

  scheduler.tick(now + std::chrono::milliseconds(10));
  EXPECT_EQ(high, 1);
  EXPECT_EQ(lowA, 0);
  EXPECT_EQ(lowB, 0);

  scheduler.tick(now + std::chrono::milliseconds(11));
  EXPECT_EQ(lowA, 1) << "a dropped preview runs before fresh work once budget allows";
  EXPECT_EQ(lowB, 0);

  scheduler.tick(now + std::chrono::milliseconds(12));
  EXPECT_EQ(lowB, 1) << "the second dropped preview must not starve";
  EXPECT_EQ(high, 1);
}

TEST(UiHost, AStalledPreviewDoesNotStarveTheOthers) {
  coral::ui::host::PreviewScheduler scheduler;
  scheduler.setMaxRendersPerTick(1);
  int slow = 0;
  int fastA = 0;
  int fastB = 0;
  const auto start = std::chrono::steady_clock::now();
  scheduler.add([&slow] {
    ++slow;
    // Stand-in for a 4K preview: one tick's render pass blocks for a noticeable time.
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }, 0.0, 100);
  scheduler.add([&fastA] { ++fastA; }, 0.0, 0);
  scheduler.add([&fastB] { ++fastB; }, 0.0, 0);

  auto now = start + std::chrono::milliseconds(1);
  scheduler.tick(now);
  EXPECT_EQ(slow, 1);
  EXPECT_EQ(fastA, 0);
  EXPECT_EQ(fastB, 0);

  now += std::chrono::milliseconds(33);
  scheduler.tick(now);
  now += std::chrono::milliseconds(33);
  scheduler.tick(now);
  EXPECT_EQ(fastA, 1) << "a fast preview still gets a tick after the stalled one";
  EXPECT_EQ(fastB, 1);
}

TEST(UiHost, AThreadedHostPresentsAFrameFromLentBoundaryLists) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  CORAL_REQUIRE_RECORDING_BACKEND();
  // M4's contract: a packet can carry the lists a plan-built frame lends, and the render thread submits
  // them in paint order after the content -- the same picture, without the UI thread flattening them.
  host::ThreadedRenderHost::Options options;
  options.width = 32;
  options.height = 32;
  options.label = "ui-host-lists-test";
  host::ThreadedRenderHost host(options);
  ASSERT_TRUE(host.waitForReady()) << host.diagnostics();

  graphics::gpu::Recorder content(32, 32, nullptr);
  graphics::Paint background;
  background.color = Color::MakeRGBA8(20, 20, 20, 255);
  content.canvas().drawRect(Rect::MakeWH(32.0f, 32.0f), background);

  graphics::gpu::Recorder patch(32, 32, nullptr);
  graphics::Paint colour;
  colour.color = Color::MakeRGBA8(200, 60, 20, 255);
  patch.canvas().drawRect(Rect::MakeLTRB(8.0f, 8.0f, 24.0f, 24.0f), colour);

  FramePacket packet;
  packet.sequence = 1;
  packet.commands = std::make_shared<const graphics::gpu::DrawList>(content.finish());
  FrameBoundaryList boundary;
  boundary.list = std::make_shared<const graphics::gpu::DrawList>(patch.finish());
  packet.boundaries.push_back(std::move(boundary));
  packet.completion = std::make_shared<FrameCompletion>();
  ASSERT_TRUE(host.submit(std::move(packet)));

  Bitmap bitmap;
  ASSERT_TRUE(host.snapshot(0, &bitmap));
  const auto pixels = bitmap.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  const uint32_t inside = pixels.getPixelARGB32(16, 16);
  EXPECT_EQ((inside >> 16) & 0xFFu, 200u) << "the lent list was not painted";
  const uint32_t outside = pixels.getPixelARGB32(2, 2);
  EXPECT_EQ((outside >> 16) & 0xFFu, 20u) << "the content list was not painted";
}