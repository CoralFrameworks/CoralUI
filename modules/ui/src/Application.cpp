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

#include <ui/Application.h>
#include <ui/RendererDevice.h>
#include <ui/host/ThreadedRenderHost.h>

#include "WindowPlatform.h"

#include <backend/Device.h>
#include <core/event/EventHandler.h>
#include <core/event/EventLooper.h>
#include <core/event/EventThread.h>
#include <core/event/Timer.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

namespace coral::ui {

/**
 * The UI task queue.
 *
 * Delayed work captures this object instead of the `Application`: a timer may outlive the
 * application by a moment, and posting into a still-alive queue that is already marked dead is safe,
 * while calling a member of a destroyed application would not be.
 */
struct ApplicationTaskQueue {
  std::mutex mutex;
  std::condition_variable wait;
  std::deque<std::function<void()>> tasks;
  std::atomic<bool> alive{ true };
};

namespace {

/**
 * How long an idle application sleeps before pumping its windows again.
 *
 * Platform input is not yet an fd/handle source registered with a shared looper, so the multi-window
 * loop cannot block indefinitely: it sleeps here and polls. Eight milliseconds keeps an idle editor
 * far below one frame of CPU wakeups while bounding input latency to half a 60 Hz frame. A single
 * window normally uses `Window::run()` instead, which blocks in the platform wait.
 */
constexpr std::chrono::milliseconds kIdleWait{ 8 };

/** Enqueues a UI task, or drops it when the application is shutting down. */
bool enqueueTask(const std::shared_ptr<ApplicationTaskQueue>& queue, std::function<void()> task) {
  if (queue == nullptr || !task) return false;
  {
    std::lock_guard<std::mutex> lock(queue->mutex);
    if (!queue->alive.load(std::memory_order_acquire)) return false;
    queue->tasks.push_back(std::move(task));
  }
  queue->wait.notify_all();
  return true;
}

} // namespace

Application::Application()
    : mQueue(std::make_shared<ApplicationTaskQueue>()) {}

Application::~Application() {
  // Close the queue first: timer callbacks and other threads may still be posting while the timer
  // thread is joined below. They capture the queue, not `this`.
  if (mQueue != nullptr) {
    {
      std::lock_guard<std::mutex> lock(mQueue->mutex);
      mQueue->alive.store(false, std::memory_order_release);
      mQueue->tasks.clear();
    }
    mQueue->wait.notify_all();
  }
  mRunning.store(false, std::memory_order_release);

  std::shared_ptr<EventHandler> timerHandler;
  std::shared_ptr<EventThread> timerThread;
  {
    std::lock_guard<std::mutex> lock(mTimerMutex);
    timerHandler = std::move(mTimerHandler);
    timerThread = std::move(mTimerThread);
  }
  if (timerHandler != nullptr) timerHandler->removeAllCallbacksAndMessages();
  // The EventThread destructor quits and joins; doing it after withdrawing the timer messages keeps
  // an in-flight callback from starting new work during teardown.
  timerThread.reset();

  // Windows must stop using the device before it is released.
  mWindows.clear();
  mDevice.reset();
  mSharedDevice.reset();
#if defined(__linux__)
  // The next Application/test gets a fresh Wayland connection instead of inheriting a display whose
  // EGL state belonged to the device that just went away.
  detail::shutdownPlatformDisplay();
#endif
  mQueue.reset();
}

std::unique_ptr<Application> Application::create() {
  return std::make_unique<Application>();
}

std::shared_ptr<Window> Application::createWindow(Window::Options options) {
  if (mClearColor.has_value()) options.clearColor = *mClearColor;
  if (options.renderHost == RenderHostKind::RenderThread && options.sharedRenderHost == nullptr) {
    if (mRenderHost == nullptr) mRenderHost = std::make_shared<host::ThreadedRenderHost>();
    options.sharedRenderHost = mRenderHost;
  }
  if (mSharedDevice != nullptr && options.sharedDevice == nullptr) {
    options.sharedDevice = mSharedDevice;
  }
  std::unique_ptr<Window> created = Window::create(options);
  if (created == nullptr) return nullptr;
  auto window = std::shared_ptr<Window>(std::move(created));
  if (mSharedDevice == nullptr) {
    std::shared_ptr<RendererDevice> released = window->releaseRendererDevice();
    if (released != nullptr) {
      mDevice = released;
      mSharedDevice = released;
    }
  }
  if (mTheme.has_value()) window->setTheme(*mTheme);
  mWindows.push_back(window);
  return window;
}

void Application::setTheme(Theme theme) {
  mTheme = theme;
  for (const std::shared_ptr<Window>& window: mWindows) window->setTheme(theme);
}

void Application::setClearColor(const Color color) {
  mClearColor = color;
  for (const std::shared_ptr<Window>& window: mWindows) window->setClearColor(color);
}

void Application::setOnWindowClosed(std::function<void(Window*)> callback) {
  mOnWindowClosed = std::move(callback);
}

std::vector<WindowLayout> Application::captureWindowLayout() const {
  std::vector<WindowLayout> layout;
  layout.reserve(mWindows.size());
  for (const std::shared_ptr<Window>& window: mWindows) {
    layout.push_back(WindowLayout{ window->title(), window->bounds(), window->isFullscreen() });
  }
  return layout;
}

bool Application::saveWindowLayout(const std::string_view path) const {
  if (path.empty()) return false;
  std::ofstream file{ std::string(path), std::ios::binary | std::ios::trunc };
  if (!file) return false;
  file << "CORAL_WINDOW_LAYOUT 1\n";
  for (const WindowLayout& entry: captureWindowLayout()) {
    file << (entry.fullscreen ? 1 : 0) << ' ' << entry.bounds.getLeft() << ' '
         << entry.bounds.getTop() << ' ' << entry.bounds.getRight() << ' '
         << entry.bounds.getBottom() << ' ' << std::quoted(entry.title) << '\n';
  }
  return file.good();
}

bool Application::loadWindowLayout(const std::string_view path) {
  if (path.empty()) return false;
  std::ifstream file{ std::string(path), std::ios::binary };
  if (!file) return false;
  std::string header;
  std::getline(file, header);
  if (header != "CORAL_WINDOW_LAYOUT 1") return false;
  std::vector<WindowLayout> layout;
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty()) continue;
    std::istringstream input(line);
    int fullscreen = 0;
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    WindowLayout entry;
    if (!(input >> fullscreen >> left >> top >> right >> bottom >> std::quoted(entry.title))) {
      return false;
    }
    entry.bounds = Rect::MakeLTRB(left, top, right, bottom);
    entry.fullscreen = fullscreen != 0;
    layout.push_back(std::move(entry));
  }
  return applyWindowLayout(layout);
}

bool Application::applyWindowLayout(const std::vector<WindowLayout>& layout) {
  if (layout.size() != mWindows.size()) return false;
  for (size_t i = 0; i < mWindows.size(); ++i) {
    mWindows[i]->restoreGeometry(layout[i].bounds);
    mWindows[i]->setFullscreen(layout[i].fullscreen);
  }
  return true;
}

void Application::activateWindow(Window& window) {
  window.raise();
}

size_t Application::closeAllWindows() {
  size_t requested = 0;
  for (const std::shared_ptr<Window>& window: mWindows) {
    if (!window->running()) continue;
    window->requestClose();
    ++requested;
  }
  return requested;
}

bool Application::recoverRenderDevice() {
  if (mRenderHost != nullptr) {
    if (!mRenderHost->recover()) return false;
    for (const std::shared_ptr<Window>& window: mWindows) window->recoverFromSharedHost();
    return true;
  }
  bool recovered = true;
  for (const std::shared_ptr<Window>& window: mWindows) {
    if (!window->recoverDevice()) recovered = false;
  }
  return recovered;
}

bool Application::post(std::function<void()> task) {
  return enqueueTask(mQueue, std::move(task));
}

void Application::ensureTimerThread() {
  std::lock_guard<std::mutex> lock(mTimerMutex);
  if (mTimerThread != nullptr) return;
  auto thread = std::make_shared<EventThread>("coral-ui-timers");
  thread->start();
  const std::shared_ptr<EventLooper> looper = thread->getLooper();
  if (looper == nullptr) {
    thread->quit();
    return;
  }
  auto handler = EventHandler::create(looper, nullptr);
  if (handler == nullptr) {
    thread->quit();
    return;
  }
  mTimerThread = std::move(thread);
  mTimerHandler = std::move(handler);
}

bool Application::postDelayed(std::function<void()> task, const std::chrono::milliseconds delay) {
  if (!task) return false;
  if (delay <= std::chrono::milliseconds::zero()) return post(std::move(task));

  ensureTimerThread();
  std::shared_ptr<EventHandler> handler;
  {
    std::lock_guard<std::mutex> lock(mTimerMutex);
    handler = mTimerHandler;
  }
  if (handler == nullptr) return post(std::move(task));

  // The timer thread only hops back to the UI thread; the closure itself is not run there, so a
  // delayed task cannot race the layer tree.
  const std::shared_ptr<ApplicationTaskQueue> queue = mQueue;
  return handler->postDelay([queue, task = std::move(task)]() mutable {
    enqueueTask(queue, std::move(task));
  }, delay);
}

std::shared_ptr<Timer> Application::createTimer(const std::chrono::milliseconds interval,
    std::function<void()> callback, const bool repeating) {
  if (!callback) return nullptr;
  ensureTimerThread();

  std::shared_ptr<EventHandler> handler;
  std::shared_ptr<EventLooper> looper;
  {
    std::lock_guard<std::mutex> lock(mTimerMutex);
    handler = mTimerHandler;
    if (mTimerThread != nullptr) looper = mTimerThread->getLooper();
  }
  if (handler == nullptr || looper == nullptr) return nullptr;

  const std::shared_ptr<ApplicationTaskQueue> queue = mQueue;
  // Copy the callback into each fire: a repeating timer must stay callable after the first fire.
  auto onTimerThread = [queue, callback] { enqueueTask(queue, callback); };
  return Timer::create(looper, interval, std::move(onTimerThread), repeating);
}

void Application::drainTasks() {
  std::deque<std::function<void()>> tasks;
  {
    std::lock_guard<std::mutex> lock(mQueue->mutex);
    tasks.swap(mQueue->tasks);
  }
  for (std::function<void()>& task: tasks) {
    if (task) task();
  }
}

int Application::run() {
  int total = 0;
  while (mRunning.load(std::memory_order_acquire)) {
    bool anyRunning = false;
    for (const std::shared_ptr<Window>& window: mWindows) {
      if (!window->running()) continue;
      anyRunning = true;
      const int before = window->frameCount();
      window->tick(false);
      total += window->frameCount() - before;
    }

    // Tasks posted by input handlers, timer callbacks or other threads run on this thread, after the
    // windows have been pumped, so a task sees the state the event produced.
    drainTasks();

    // Closed windows leave the active list; keep each window alive through its callback, because a
    // product handler commonly reads the window (dimensions, title, last diagnostics) while closing.
    std::vector<std::shared_ptr<Window>> closed;
    for (auto it = mWindows.begin(); it != mWindows.end();) {
      if (!(*it)->running()) {
        closed.push_back(std::move(*it));
        it = mWindows.erase(it);
      } else {
        ++it;
      }
    }
    for (const std::shared_ptr<Window>& window: closed) {
      if (mOnWindowClosed) mOnWindowClosed(window.get());
    }
    if (!anyRunning) break;

    // The old loop called `tick(false)` and then `yield()`: with no frame due it spun at full CPU.
    // Waiting here, with a bounded timeout, is the first slice of the application event loop. The
    // timer thread wakes this wait for delayed work; platform input is bounded by the timeout until
    // the windows expose a shared fd/handle source.
    std::unique_lock<std::mutex> lock(mQueue->mutex);
    mQueue->wait.wait_for(lock, kIdleWait, [this] {
      return !mRunning.load(std::memory_order_acquire) || !mQueue->tasks.empty() ||
        !mQueue->alive.load(std::memory_order_acquire);
    });
  }
  return total;
}

size_t Application::openWindowCount() const noexcept {
  size_t count = 0;
  for (const std::shared_ptr<Window>& window: mWindows) {
    if (window->running()) ++count;
  }
  return count;
}

void Application::quit() {
  mRunning.store(false, std::memory_order_release);
  for (const std::shared_ptr<Window>& window: mWindows) window->quit();
  if (mQueue != nullptr) mQueue->wait.notify_all();
}

} // namespace coral::ui
