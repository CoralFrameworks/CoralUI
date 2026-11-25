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

#ifndef CORAL_UI_APPLICATION_H
#define CORAL_UI_APPLICATION_H

#include <ui/Window.h>
#include <ui/Theme.h>

#include <core/event/Timer.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace coral {

class EventHandler;
class EventThread;

} // namespace coral

namespace coral::ui {

namespace host {
class ThreadedRenderHost;
}

/** The UI task queue shared with timer callbacks; defined in Application.cpp. */
struct ApplicationTaskQueue;

/**
 * A restorable window placement.
 *
 * `title` is informational and for product-side matching; `bounds` is in screen coordinates (device
 * pixels). Wayland reports the client size at the origin because it has no global placement.
 */
struct WindowLayout {
  std::string title;
  Rect bounds;
  bool fullscreen = false;
};

/**
 * Owns a set of windows and drives them in one loop.
 *
 * Windows borrow an opaque renderer device owned by the application; both native and headless
 * transfer the first window's device into `mDevice` with `releaseRendererDevice()`.
 *
 * ## Threading
 *
 * `run()` owns the calling (UI) thread. `post()`, `postDelayed()` and `createTimer()` may be called
 * from any thread; their callbacks run on the UI thread. Delayed work is scheduled on an internal
 * `event::EventThread`, so a timer fires even when no window is producing frames.
 */
class Application {
 public:
  Application();
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  [[nodiscard]] static std::unique_ptr<Application> create();

  /** Creates a window owned by the application. Returns null when the platform refused it. */
  std::shared_ptr<Window> createWindow(Window::Options options);

  /** Applies @p theme to every current window and to windows created later. */
  void setTheme(Theme theme);
  [[nodiscard]] const std::optional<Theme>& theme() const noexcept { return mTheme; }

  /** Applies a uniform GPU clear colour to every current window and to windows created later. */
  void setClearColor(Color color);
  [[nodiscard]] const std::optional<Color>& clearColor() const noexcept { return mClearColor; }

  /**
   * Called on the UI thread when a window is removed after it closed.
   *
   * The pointer is valid for the duration of the call: the application keeps the window alive until
   * the callback returns, so a handler may read its title, size or diagnostics.
   */
  void setOnWindowClosed(std::function<void(Window*)> callback);

  [[nodiscard]] const std::vector<std::shared_ptr<Window>>& windows() const noexcept {
    return mWindows;
  }

  /**
   * The render host shared by threaded windows, or null until the first one is created.
   *
   * Immediate windows do not use it. A threaded window added to this application borrows this host
   * and shares its device/render thread with every other threaded window.
   */
  [[nodiscard]] const std::shared_ptr<host::ThreadedRenderHost>& renderHost() const noexcept {
    return mRenderHost;
  }

  /**
   * Queues @p task to run on the UI thread.
   *
   * Safe from any thread, including before `run()` starts. Returns false after the application is
   * shutting down.
   */
  bool post(std::function<void()> task);

  /**
   * Queues @p task to run on the UI thread after @p delay.
   *
   * A non-positive delay is a plain `post()`. The timer keeps running while the UI is idle.
   */
  bool postDelayed(std::function<void()> task, std::chrono::milliseconds delay);

  /**
   * Creates a one-shot or repeating timer whose callback runs on the UI thread.
   *
   * The returned timer may be started/stopped from any thread; the caller keeps it alive. This is the
   * UI-flavoured wrapper over `event::Timer`: the core timer fires on the timer thread and this
   * adapter hops to the UI thread before invoking @p callback.
   */
  std::shared_ptr<Timer> createTimer(std::chrono::milliseconds interval,
    std::function<void()> callback, bool repeating = false);

  /**
   * Captures the current window order, titles, bounds and fullscreen state.
   *
   * A product persists this (its own settings file) and restores it with `applyWindowLayout` before
   * the first frame. Order matches `windows()`.
   */
  [[nodiscard]] std::vector<WindowLayout> captureWindowLayout() const;

  /**
   * Applies @p layout to the current windows positionally; false when the count does not match.
   *
   * Restoring by order (rather than by an unstable runtime id) keeps the format stable across runs.
   * Bounds are applied before fullscreen so the restored size is the one used on exit.
   */
  bool applyWindowLayout(const std::vector<WindowLayout>& layout);

  /** Writes the current window layout to a small text file; empty layouts are valid. */
  bool saveWindowLayout(std::string_view path) const;
  /** Loads and applies a previously saved layout; false when the file is invalid or window counts differ. */
  bool loadWindowLayout(std::string_view path);

  /** Raises and activates @p window; no-op when it is headless or the platform has no activation. */
  void activateWindow(Window& window);

  /**
   * Requests a close on every window through its interceptor, like clicking each close button.
   *
   * A window that vetoes stays open. @return how many closes were requested (not how many closed).
   */
  size_t closeAllWindows();

  /**
   * Recovers the shared render host/device once for every window in this application.
   * Threaded windows share one host, so per-window recovery is rejected; this is the session-wide
   * entry point. Immediate windows are recovered individually.
   */
  bool recoverRenderDevice();

  /** Pumps and renders every live window until all close. @return the total frames produced. */
  int run();

  /** Number of windows still running. */
  [[nodiscard]] size_t openWindowCount() const noexcept;

  /** Asks every window to close after the current iteration. */
  void quit();

 private:
  /** Creates the timer thread and its handler on first use. */
  void ensureTimerThread();

  /** Swaps the queued UI tasks out and runs them. */
  void drainTasks();

  std::shared_ptr<RendererDevice> mDevice;
  /** Declared before `mWindows` so windows are destroyed before the host in the implicit destructor. */
  std::shared_ptr<host::ThreadedRenderHost> mRenderHost;
  std::vector<std::shared_ptr<Window>> mWindows;
  std::shared_ptr<RendererDevice> mSharedDevice;
  std::optional<Theme> mTheme;
  std::optional<Color> mClearColor;
  std::function<void(Window*)> mOnWindowClosed;

  std::atomic<bool> mRunning{ true };
  std::shared_ptr<ApplicationTaskQueue> mQueue;

  std::mutex mTimerMutex;
  std::shared_ptr<EventThread> mTimerThread;
  std::shared_ptr<EventHandler> mTimerHandler;
};

} // namespace coral::ui

#endif // CORAL_UI_APPLICATION_H

