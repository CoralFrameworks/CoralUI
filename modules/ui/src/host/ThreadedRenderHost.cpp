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

#include <ui/host/ThreadedRenderHost.h>

#include <ui/host/ThreadAffinity.h>

#include <ui/RendererCapabilities.h>

#include <gpu/Surface.h>
#include <gpu/WindowSurface.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include <vector>

bool coral::ui::FramePacket::hasCommands() const {
  return commands != nullptr && !commands->isEmpty();
}

namespace coral::ui::host {

namespace {

backend::Backend toBackend(const RendererApi api) {
  switch (api) {
    case RendererApi::OpenGL: return backend::Backend::OpenGL;
    case RendererApi::Vulkan: return backend::Backend::Vulkan;
    case RendererApi::Auto:
    default: return backend::Backend::Null;
  }
}

backend::NativeWindowSystem toNativeWindowSystem(const ThreadedRenderHost::NativeWindowKind kind) {
  switch (kind) {
    case ThreadedRenderHost::NativeWindowKind::Windows:
      return backend::NativeWindowSystem::Win32;
    case ThreadedRenderHost::NativeWindowKind::X11:
      return backend::NativeWindowSystem::X11;
    case ThreadedRenderHost::NativeWindowKind::Wayland:
      return backend::NativeWindowSystem::Wayland;
    case ThreadedRenderHost::NativeWindowKind::Unknown:
    default:
      return backend::NativeWindowSystem::Unknown;
  }
}

double milliseconds(const std::chrono::steady_clock::duration duration) {
  return std::chrono::duration<double, std::milli>(duration).count();
}

} // namespace

struct ThreadedRenderHost::Impl {
  struct Target {
    uint64_t id = 0;
    Options options;
    std::unique_ptr<graphics::gpu::Surface> surface;
    std::unique_ptr<graphics::gpu::WindowSurface> windowSurface;
    std::shared_ptr<const RendererCapabilities> capabilities;
    std::atomic<uint64_t> submitted{0};
    std::atomic<uint64_t> failed{0};
    std::atomic<uint64_t> dropped{0};
    std::atomic<uint64_t> lastSequence{0};
    std::atomic<double> lastRenderMs{0.0};
    std::atomic<double> lastPresentMs{0.0};
    std::atomic<int64_t> lastPresentedNs{0};
  };

  struct AddRequest {
    uint64_t id = 0;
    Options options;
    std::shared_ptr<std::promise<bool>> promise;
  };
  struct RemoveRequest {
    uint64_t id = 0;
    std::shared_ptr<std::promise<bool>> promise;
  };
  struct ResizeRequest {
    uint64_t id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
  };
  struct SnapshotRequest {
    uint64_t id = 0;
    Bitmap* out = nullptr;
    std::shared_ptr<std::promise<bool>> promise;
  };
  struct RecoverRequest {
    std::shared_ptr<std::promise<bool>> promise;
  };

  explicit Impl(Options primaryOptions) : primary(std::move(primaryOptions)) {
    thread = std::thread([this] { run(); });
  }

  ~Impl() {
    if (thread.joinable()) thread.join();
  }

  void run();
  bool createTarget(uint64_t id, const Options& options, std::string& error);
  void applyResize(ResizeRequest& request);
  void submitFrame(FramePacket& packet);
  bool recoverTargets();
  void processSnapshot(SnapshotRequest& request);
  [[nodiscard]] Target* targetFor(uint64_t id);

  Options primary;
  std::thread thread;
  mutable std::mutex mutex;
  std::condition_variable wake;
  std::map<uint64_t, std::unique_ptr<Target>> targets;
  std::map<uint64_t, FramePacket> frames;
  std::deque<std::shared_ptr<AddRequest>> adds;
  std::deque<std::shared_ptr<RemoveRequest>> removes;
  std::deque<std::shared_ptr<ResizeRequest>> resizes;
  std::deque<std::shared_ptr<SnapshotRequest>> snapshots;
  std::deque<std::shared_ptr<RecoverRequest>> recovers;
  std::deque<std::function<void()>> tasks;
  PreviewScheduler previewScheduler;
  std::unique_ptr<backend::Device> device;
  std::shared_ptr<const RendererCapabilities> primaryCapabilities;
  PresentationCallback presentationCallback;
  std::map<uint64_t, PresentationCallback> presentationCallbacks;
  std::thread::id renderThread;
  /** Debug guard for "device/surface work stays on the render thread". */
  host::ThreadAffinity renderAffinity{ "render-host" };
  std::atomic<bool> running{true};
  std::atomic<bool> ready{false};
  std::atomic<uint64_t> primaryId{0};
  std::atomic<uint64_t> nextId{1};
  std::atomic<uint64_t> submitted{0};
  std::atomic<uint64_t> dropped{0};
  std::string diagnostics;
};

ThreadedRenderHost::Impl::Target* ThreadedRenderHost::Impl::targetFor(const uint64_t id) {
  auto it = targets.find(id);
  return it != targets.end() ? it->second.get() : nullptr;
}

bool ThreadedRenderHost::Impl::createTarget(const uint64_t id, const Options& options,
    std::string& error) {
  renderAffinity.assertCurrentThread("ThreadedRenderHost::createTarget");
  auto target = std::make_unique<Target>();
  target->id = id;
  target->options = options;
  if (options.nativeWindow != nullptr) {
    graphics::gpu::WindowSurface::Configuration configuration;
    configuration.nativeWindow = options.nativeWindow;
    configuration.nativeDisplay = options.nativeDisplay;
    configuration.nativeWindowSystem = toNativeWindowSystem(options.nativeWindowKind);
    configuration.width = options.width;
    configuration.height = options.height;
    configuration.sampleCount = 1;
    configuration.presentMode = backend::PresentMode::Mailbox;
    configuration.outputColorSpace = options.outputColorSpace;
    configuration.preferred = toBackend(options.rendererApi);
    if (device != nullptr) {
      target->windowSurface = graphics::gpu::WindowSurface::makeOn(device.get(), configuration);
    } else {
      target->windowSurface = graphics::gpu::WindowSurface::make(configuration);
      if (target->windowSurface != nullptr) {
        device.reset(target->windowSurface->releaseDevice());
      }
    }
    if (target->windowSurface == nullptr || device == nullptr) {
      error = "the threaded render host could not create a window target";
      return false;
    }
  } else {
    graphics::gpu::SurfaceConfiguration configuration;
    configuration.width = options.width;
    configuration.height = options.height;
    configuration.preferred = toBackend(options.rendererApi);
    configuration.stencil = true;
    configuration.sampleCount = 1;
    configuration.maxFramesInFlight = 1;
    configuration.label = options.label;
    if (device != nullptr) {
      target->surface = graphics::gpu::Surface::makeOn(device.get(), configuration);
    } else {
      target->surface = graphics::gpu::Surface::make(configuration);
      if (target->surface != nullptr) {
        device.reset(target->surface->releaseDevice());
      }
    }
    if (target->surface == nullptr || device == nullptr) {
      error = "the threaded render host could not create an offscreen target";
      return false;
    }
  }
  const uint32_t sampleCount = target->windowSurface != nullptr
    ? target->windowSurface->sampleCount() : target->surface->sampleCount();
  target->capabilities = std::make_shared<const RendererCapabilities>(
    queryRendererCapabilities(device.get(), true, sampleCount));
  {
    const std::lock_guard<std::mutex> lock(mutex);
    targets[id] = std::move(target);
  }
  return true;
}

void ThreadedRenderHost::Impl::applyResize(ResizeRequest& request) {
  renderAffinity.assertCurrentThread("ThreadedRenderHost::applyResize");
  Target* target = targetFor(request.id);
  if (target == nullptr || request.width == 0 || request.height == 0) return;
  target->options.width = request.width;
  target->options.height = request.height;
  if (target->windowSurface != nullptr) {
    if (!target->windowSurface->resize(request.width, request.height)) {
      const std::lock_guard<std::mutex> lock(mutex);
      diagnostics = target->windowSurface->diagnostics();
    }
  } else if (target->surface != nullptr) {
    graphics::gpu::SurfaceConfiguration configuration;
    configuration.width = request.width;
    configuration.height = request.height;
    configuration.preferred = toBackend(target->options.rendererApi);
    configuration.stencil = true;
    configuration.sampleCount = 1;
    configuration.maxFramesInFlight = 1;
    configuration.label = target->options.label;
    auto replacement = graphics::gpu::Surface::makeOn(device.get(), configuration);
    if (replacement != nullptr) {
      target->surface = std::move(replacement);
    } else {
      const std::lock_guard<std::mutex> lock(mutex);
      diagnostics = "the threaded render host could not resize its offscreen target";
    }
  }
}

bool ThreadedRenderHost::Impl::recoverTargets() {
  renderAffinity.assertCurrentThread("ThreadedRenderHost::recoverTargets");
  std::vector<std::pair<uint64_t, Options>> saved;
  saved.reserve(targets.size());
  for (const auto& entry: targets) saved.emplace_back(entry.first, entry.second->options);
  targets.clear();
  frames.clear();
  device.reset();
  bool ok = true;
  for (const auto& entry: saved) {
    std::string error;
    if (!createTarget(entry.first, entry.second, error)) {
      const std::lock_guard<std::mutex> lock(mutex);
      diagnostics = error;
      ok = false;
    }
  }
  if (!targets.empty()) {
    primaryCapabilities = targets.begin()->second->capabilities;
  }
  return ok;
}

void ThreadedRenderHost::Impl::submitFrame(FramePacket& packet) {
  renderAffinity.assertCurrentThread("ThreadedRenderHost::submitFrame");
  Target* target = targetFor(packet.windowId == 0 ? primaryId.load() : packet.windowId);
  const auto start = std::chrono::steady_clock::now();
  if (target == nullptr) {
    if (packet.completion != nullptr) {
      packet.completion->failed.store(true, std::memory_order_release);
      packet.completion->presentedAt = std::chrono::steady_clock::now();
      packet.completion->windowId = packet.windowId;
    }
    return;
  }
  for (const ContentHost::FrameFence& fence: packet.fences) {
    if (fence) fence();
  }
  bool rendered = false;
  if (!packet.boundaries.empty()) {
    // ## A frame the UI thread assembled from lists
    //
    // The lists are submitted in one command buffer, so the frame reaches the device once, in paint
    // order: the recorded content first, then the lent lists. A surface that cannot take them falls back
    // to folding them into the content recording, which is what an unplanned frame pays anyway.
    std::vector<graphics::gpu::Surface::ListItem> items;
    items.reserve(packet.boundaries.size());
    for (const FrameBoundaryList& boundary: packet.boundaries) {
      if (boundary.list == nullptr) continue;
      graphics::gpu::Surface::ListItem item;
      item.list = boundary.list;
      item.clip = boundary.clip;
      item.translation = boundary.translation;
      items.push_back(std::move(item));
    }
    if (target->surface != nullptr) {
      const auto renderStart = std::chrono::steady_clock::now();
      if (target->surface->submitRecording(packet.commands) && target->surface->submitLists(items)) {
        rendered = true;
      } else {
        rendered = target->surface->submit();
      }
      const auto renderEnd = std::chrono::steady_clock::now();
      if (packet.completion != nullptr) {
        packet.completion->renderMilliseconds = milliseconds(renderEnd - renderStart);
      }
    } else if (target->windowSurface != nullptr) {
      std::vector<std::shared_ptr<const graphics::gpu::DrawList>> lists;
      lists.reserve(items.size());
      for (const graphics::gpu::Surface::ListItem& item: items) lists.push_back(item.list);
      const auto renderStart = std::chrono::steady_clock::now();
      rendered = target->windowSurface->presentRecordings(lists);
      const auto renderEnd = std::chrono::steady_clock::now();
      if (packet.completion != nullptr) {
        packet.completion->renderMilliseconds = milliseconds(renderEnd - renderStart);
      }
    }
  } else if (packet.commands != nullptr && !packet.commands->isEmpty()) {
    if (target->windowSurface != nullptr) {
      const auto renderStart = std::chrono::steady_clock::now();
      rendered = target->windowSurface->presentRecording(packet.commands, packet.damage);
      const auto renderEnd = std::chrono::steady_clock::now();
      if (packet.completion != nullptr) {
        packet.completion->renderMilliseconds = milliseconds(renderEnd - renderStart);
      }
    } else if (target->surface != nullptr) {
      const auto renderStart = std::chrono::steady_clock::now();
      rendered = target->surface->submitRecording(packet.commands) && target->surface->submit();
      const auto renderEnd = std::chrono::steady_clock::now();
      if (packet.completion != nullptr) {
        packet.completion->renderMilliseconds = milliseconds(renderEnd - renderStart);
      }
    }
  }
  const auto end = std::chrono::steady_clock::now();
  if (packet.completion != nullptr) {
    packet.completion->windowId = packet.windowId == 0 ? primaryId.load() : packet.windowId;
    // `presentRecording` blocks until the swapchain hands the frame over, so the call duration is
    // the present cost; the queue-to-call span is the render-thread latency.
    packet.completion->presentMilliseconds = milliseconds(end - start) -
      packet.completion->renderMilliseconds;
    packet.completion->presentedAt = end;
    packet.completion->sequence = packet.sequence;
    packet.completion->presented.store(rendered, std::memory_order_release);
    packet.completion->failed.store(!rendered, std::memory_order_release);
  }
  if (rendered) {
    submitted.fetch_add(1, std::memory_order_relaxed);
    target->submitted.fetch_add(1, std::memory_order_relaxed);
    target->lastSequence.store(packet.sequence, std::memory_order_relaxed);
    target->lastRenderMs.store(packet.completion != nullptr ? packet.completion->renderMilliseconds : 0.0,
      std::memory_order_relaxed);
    target->lastPresentMs.store(packet.completion != nullptr ? packet.completion->presentMilliseconds : 0.0,
      std::memory_order_relaxed);
    target->lastPresentedNs.store(std::chrono::duration_cast<std::chrono::nanoseconds>(
      end.time_since_epoch()).count(), std::memory_order_relaxed);
  } else {
    target->failed.fetch_add(1, std::memory_order_relaxed);
    const std::lock_guard<std::mutex> lock(mutex);
    diagnostics = "the threaded render host failed to submit a frame";
  }
  PresentationCallback callback;
  {
    const std::lock_guard<std::mutex> lock(mutex);
    auto it = presentationCallbacks.find(packet.completion != nullptr ? packet.completion->windowId : 0);
    if (it != presentationCallbacks.end()) {
      callback = it->second;
    } else {
      callback = presentationCallback;
    }
  }
  if (callback && packet.completion != nullptr) callback(*packet.completion);
}

void ThreadedRenderHost::Impl::processSnapshot(SnapshotRequest& request) {
  renderAffinity.assertCurrentThread("ThreadedRenderHost::processSnapshot");
  Target* target = targetFor(request.id == 0 ? primaryId.load() : request.id);
  if (target == nullptr || request.out == nullptr) {
    request.promise->set_value(false);
    return;
  }
  Bitmap snapshot;
  if (target->windowSurface != nullptr) {
    target->windowSurface->readback(&snapshot);
  } else if (target->surface != nullptr) {
    snapshot = target->surface->snapshot();
  }
  *request.out = snapshot;
  request.promise->set_value(!snapshot.isEmpty());
}

void ThreadedRenderHost::Impl::run() {
  {
    const std::lock_guard<std::mutex> lock(mutex);
    renderThread = std::this_thread::get_id();
  }
  renderAffinity.bindToCurrentThread();
  for (;;) {
    std::map<uint64_t, FramePacket> frames;
    std::deque<std::shared_ptr<AddRequest>> adds;
    std::deque<std::shared_ptr<RemoveRequest>> removes;
    std::deque<std::shared_ptr<ResizeRequest>> resizes;
    std::deque<std::shared_ptr<SnapshotRequest>> snapshots;
    std::deque<std::shared_ptr<RecoverRequest>> recovers;
    std::deque<std::function<void()>> tasks;
    {
      std::unique_lock<std::mutex> lock(mutex);
      const auto ready = [this] {
        return !running || !this->frames.empty() || !this->adds.empty() ||
          !this->removes.empty() || !this->resizes.empty() || !this->snapshots.empty() ||
          !this->recovers.empty() || !this->tasks.empty();
      };
      // A preview scheduler has no clock of its own to wake us; poll at a bounded interval while
      // previews are registered. The scheduler's own target FPS decides whether a tick renders.
      if (this->previewScheduler.size() > 0) {
        wake.wait_for(lock, std::chrono::milliseconds(16), ready);
      } else {
        wake.wait(lock, ready);
      }
      if (!running && this->frames.empty() && this->adds.empty() && this->removes.empty() &&
          this->resizes.empty() && this->snapshots.empty() && this->recovers.empty() &&
          this->tasks.empty()) {
        break;
      }
      frames.swap(this->frames);
      adds.swap(this->adds);
      removes.swap(this->removes);
      resizes.swap(this->resizes);
      snapshots.swap(this->snapshots);
      recovers.swap(this->recovers);
      tasks.swap(this->tasks);
    }

    for (std::function<void()>& task: tasks) {
      if (task) task();
    }
    previewScheduler.tick(std::chrono::steady_clock::now());

    for (const std::shared_ptr<AddRequest>& request: adds) {
      if (request == nullptr) continue;
      std::string error;
      const bool created = createTarget(request->id, request->options, error);
      if (!created) {
        const std::lock_guard<std::mutex> lock(mutex);
        diagnostics = error;
      }
      request->promise->set_value(created);
    }

    for (const std::shared_ptr<RemoveRequest>& request: removes) {
      if (request == nullptr) continue;
      {
        const std::lock_guard<std::mutex> lock(mutex);
        targets.erase(request->id);
        presentationCallbacks.erase(request->id);
      }
      request->promise->set_value(true);
    }

    for (const std::shared_ptr<ResizeRequest>& request: resizes) {
      if (request != nullptr) applyResize(*request);
    }

    for (const std::shared_ptr<RecoverRequest>& request: recovers) {
      if (request != nullptr) request->promise->set_value(recoverTargets());
    }

    for (auto& entry: frames) {
      FramePacket packet = std::move(entry.second);
      submitFrame(packet);
    }

    for (const std::shared_ptr<SnapshotRequest>& request: snapshots) {
      if (request != nullptr) processSnapshot(*request);
    }
  }

  {
    const std::lock_guard<std::mutex> lock(mutex);
    for (const std::shared_ptr<AddRequest>& request: adds) {
      if (request != nullptr) request->promise->set_value(false);
    }
    for (const std::shared_ptr<RemoveRequest>& request: removes) {
      if (request != nullptr) request->promise->set_value(false);
    }
    for (const std::shared_ptr<SnapshotRequest>& request: snapshots) {
      if (request != nullptr) request->promise->set_value(false);
    }
    for (const std::shared_ptr<RecoverRequest>& request: recovers) {
      if (request != nullptr) request->promise->set_value(false);
    }
    targets.clear();
    frames.clear();
    tasks.clear();
  }
  previewScheduler.clear();
  device.reset();
}

ThreadedRenderHost::ThreadedRenderHost(Options options)
  : ThreadedRenderHost() {
  const uint64_t id = addTarget(options);
  if (id == 0) return;
  mImpl->primaryId.store(id, std::memory_order_release);
  {
    const std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->targets.find(id);
    if (it != mImpl->targets.end()) mImpl->primaryCapabilities = it->second->capabilities;
  }
  mImpl->ready.store(true, std::memory_order_release);
}

ThreadedRenderHost::ThreadedRenderHost()
  : mImpl(std::make_unique<Impl>(Options{})) {
}

ThreadedRenderHost::~ThreadedRenderHost() {
  shutdown();
  if (mImpl->thread.joinable()) mImpl->thread.join();
}

uint64_t ThreadedRenderHost::primaryTarget() const noexcept {
  return mImpl->primaryId.load(std::memory_order_acquire);
}

backend::Device* ThreadedRenderHost::device() const noexcept {
  return mImpl->device.get();
}

bool ThreadedRenderHost::postTask(std::function<void()> task) {
  if (!task) return false;
  {
    const std::lock_guard<std::mutex> lock(mImpl->mutex);
    if (!mImpl->running) return false;
    mImpl->tasks.push_back(std::move(task));
  }
  mImpl->wake.notify_one();
  return true;
}

PreviewScheduler& ThreadedRenderHost::previewScheduler() noexcept {
  return mImpl->previewScheduler;
}

const PreviewScheduler& ThreadedRenderHost::previewScheduler() const noexcept {
  return mImpl->previewScheduler;
}

uint64_t ThreadedRenderHost::addTarget(const Options& options) {
  const uint64_t id = mImpl->nextId.fetch_add(1, std::memory_order_relaxed);
  auto request = std::make_shared<Impl::AddRequest>();
  request->id = id;
  request->options = options;
  request->promise = std::make_shared<std::promise<bool>>();
  std::future<bool> result = request->promise->get_future();
  {
    const std::lock_guard<std::mutex> lock(mImpl->mutex);
    if (!mImpl->running) return 0;
    mImpl->adds.push_back(std::move(request));
  }
  mImpl->wake.notify_one();
  const bool created = result.get();
  if (created) mImpl->ready.store(true, std::memory_order_release);
  return created ? id : 0;
}

bool ThreadedRenderHost::recover() {
  auto request = std::make_shared<Impl::RecoverRequest>();
  request->promise = std::make_shared<std::promise<bool>>();
  std::future<bool> result = request->promise->get_future();
  {
    const std::lock_guard<std::mutex> lock(mImpl->mutex);
    if (!mImpl->running || !mImpl->ready.load(std::memory_order_acquire)) return false;
    mImpl->recovers.push_back(std::move(request));
  }
  mImpl->wake.notify_one();
  return result.get();
}

bool ThreadedRenderHost::waitForTarget(const uint64_t windowId) {
  if (windowId == 0) return false;
  const std::lock_guard<std::mutex> lock(mImpl->mutex);
  return mImpl->targets.find(windowId) != mImpl->targets.end();
}

void ThreadedRenderHost::removeTarget(const uint64_t windowId) {
  if (windowId == 0) return;
  auto request = std::make_shared<Impl::RemoveRequest>();
  request->id = windowId;
  request->promise = std::make_shared<std::promise<bool>>();
  std::future<bool> result = request->promise->get_future();
  {
    const std::lock_guard<std::mutex> lock(mImpl->mutex);
    if (!mImpl->running) return;
    mImpl->removes.push_back(std::move(request));
  }
  mImpl->wake.notify_one();
  result.get();
}

bool ThreadedRenderHost::waitForReady() {
  const uint64_t primary = primaryTarget();
  if (primary == 0) return false;
  return waitForTarget(primary);
}

void ThreadedRenderHost::setPresentationCallback(PresentationCallback callback) {
  const std::lock_guard<std::mutex> lock(mImpl->mutex);
  mImpl->presentationCallback = std::move(callback);
}

void ThreadedRenderHost::setPresentationCallback(uint64_t windowId, PresentationCallback callback) {
  const std::lock_guard<std::mutex> lock(mImpl->mutex);
  mImpl->presentationCallbacks[windowId == 0 ? mImpl->primaryId.load() : windowId] =
    std::move(callback);
}

bool ThreadedRenderHost::submit(FramePacket packet) {
  const std::lock_guard<std::mutex> lock(mImpl->mutex);
  if (!mImpl->running || !mImpl->ready.load(std::memory_order_acquire)) return false;
  const uint64_t windowId = packet.windowId == 0 ? mImpl->primaryId.load() : packet.windowId;
  packet.windowId = windowId;
  // One latest packet per target: queueing more only adds latency, not smoothness. The map is keyed
  // by window, so a busy window replaces its own packet and never evicts another window's. The
  // replaced packet must still settle, otherwise a caller waiting on its completion would hang.
  auto existing = mImpl->frames.find(windowId);
  if (existing != mImpl->frames.end()) {
    mImpl->dropped.fetch_add(1, std::memory_order_relaxed);
    if (auto* target = mImpl->targetFor(windowId); target != nullptr) {
      target->dropped.fetch_add(1, std::memory_order_relaxed);
    }
    if (existing->second.completion != nullptr) {
      existing->second.completion->windowId = windowId;
      existing->second.completion->sequence = existing->second.sequence;
      existing->second.completion->presentedAt = std::chrono::steady_clock::now();
      existing->second.completion->dropped.store(true, std::memory_order_release);
    }
    existing->second = std::move(packet);
  } else {
    mImpl->frames.emplace(windowId, std::move(packet));
  }
  mImpl->wake.notify_one();
  return true;
}

void ThreadedRenderHost::resize(const uint64_t windowId, const uint32_t width, const uint32_t height) {
  if (width == 0 || height == 0) return;
  auto request = std::make_shared<Impl::ResizeRequest>();
  request->id = windowId == 0 ? primaryTarget() : windowId;
  request->width = width;
  request->height = height;
  {
    const std::lock_guard<std::mutex> lock(mImpl->mutex);
    if (!mImpl->running) return;
    mImpl->resizes.push_back(std::move(request));
  }
  mImpl->wake.notify_one();
}

bool ThreadedRenderHost::snapshot(uint64_t windowId, Bitmap* out) {
  if (out == nullptr) return false;
  auto request = std::make_shared<Impl::SnapshotRequest>();
  request->id = windowId == 0 ? primaryTarget() : windowId;
  request->out = out;
  request->promise = std::make_shared<std::promise<bool>>();
  std::future<bool> result = request->promise->get_future();
  {
    const std::lock_guard<std::mutex> lock(mImpl->mutex);
    if (!mImpl->running) return false;
    mImpl->snapshots.push_back(std::move(request));
  }
  mImpl->wake.notify_one();
  return result.get();
}

const RendererCapabilities& ThreadedRenderHost::capabilities() const {
  static const RendererCapabilities fallback = RendererCapabilities::raster();
  const std::lock_guard<std::mutex> lock(mImpl->mutex);
  return mImpl->primaryCapabilities != nullptr ? *mImpl->primaryCapabilities : fallback;
}

const RendererCapabilities& ThreadedRenderHost::capabilitiesFor(const uint64_t windowId) const {
  static const RendererCapabilities fallback = RendererCapabilities::raster();
  const std::lock_guard<std::mutex> lock(mImpl->mutex);
  auto it = mImpl->targets.find(windowId == 0 ? mImpl->primaryId.load() : windowId);
  if (it != mImpl->targets.end() && it->second->capabilities != nullptr) {
    return *it->second->capabilities;
  }
  return fallback;
}

RendererKind ThreadedRenderHost::kind() const noexcept {
  return RendererKind::GPU;
}

std::thread::id ThreadedRenderHost::renderThreadId() const noexcept {
  const std::lock_guard<std::mutex> lock(mImpl->mutex);
  return mImpl->renderThread;
}

uint64_t ThreadedRenderHost::submittedFrames() const noexcept {
  return mImpl->submitted.load(std::memory_order_relaxed);
}

ThreadedRenderHost::TargetStats ThreadedRenderHost::targetStats(const uint64_t windowId) const {
  TargetStats stats;
  const std::lock_guard<std::mutex> lock(mImpl->mutex);
  const auto it = mImpl->targets.find(windowId == 0 ? mImpl->primaryId.load() : windowId);
  if (it == mImpl->targets.end()) return stats;
  const Impl::Target& target = *it->second;
  stats.submitted = target.submitted.load(std::memory_order_relaxed);
  stats.failed = target.failed.load(std::memory_order_relaxed);
  stats.dropped = target.dropped.load(std::memory_order_relaxed);
  stats.lastSequence = target.lastSequence.load(std::memory_order_relaxed);
  stats.lastRenderMilliseconds = target.lastRenderMs.load(std::memory_order_relaxed);
  stats.lastPresentMilliseconds = target.lastPresentMs.load(std::memory_order_relaxed);
  const auto ns = target.lastPresentedNs.load(std::memory_order_relaxed);
  stats.lastPresentedAt = std::chrono::steady_clock::time_point(std::chrono::nanoseconds(ns));
  stats.queued = mImpl->frames.count(windowId == 0 ? mImpl->primaryId.load() : windowId) != 0 ? 1u : 0u;
  return stats;
}

uint64_t ThreadedRenderHost::droppedFrames() const noexcept {
  return mImpl->dropped.load(std::memory_order_relaxed);
}

bool ThreadedRenderHost::targetIsWindow(const uint64_t windowId) const {
  const std::lock_guard<std::mutex> lock(mImpl->mutex);
  const auto it = mImpl->targets.find(windowId == 0 ? mImpl->primaryId.load() : windowId);
  if (it == mImpl->targets.end()) return false;
  return it->second->windowSurface != nullptr;
}

size_t ThreadedRenderHost::targetCount() const {
  const std::lock_guard<std::mutex> lock(mImpl->mutex);
  return mImpl->targets.size();
}

std::string ThreadedRenderHost::diagnostics() const {
  const std::lock_guard<std::mutex> lock(mImpl->mutex);
  return mImpl->diagnostics;
}

void ThreadedRenderHost::shutdown() {
  {
    const std::lock_guard<std::mutex> lock(mImpl->mutex);
    if (!mImpl->running) return;
    mImpl->running = false;
  }
  mImpl->wake.notify_all();
}

} // namespace coral::ui::host
