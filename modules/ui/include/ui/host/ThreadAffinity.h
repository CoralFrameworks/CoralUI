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

#ifndef CORAL_UI_HOST_THREADAFFINITY_H
#define CORAL_UI_HOST_THREADAFFINITY_H

#include <cassert>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace coral::ui::host {

/**
 * A debug guard for "this object is used from the thread that bound it".
 *
 * The phase-2 render host is split into a UI thread that records frames and a render thread that
 * owns the device. Both sides need a cheap way to catch a mis-routed call in development: the UI
 * thread calling `backend::Device`, or a producer touching a window surface. `assertCurrentThread`
 * is a no-op in release builds, so it costs nothing in a shipping frame.
 *
 * The object is not itself a thread manager: `Window`/`RenderThread` bind it at creation and assert
 * at their public entry points. An unbound guard accepts the first caller and binds to it, which
 * makes a test or a single-threaded fallback work without a separate setup path.
 */
class ThreadAffinity {
 public:
  explicit ThreadAffinity(std::string name = "ui") : mName(std::move(name)) {}

  ThreadAffinity(const ThreadAffinity&) = delete;
  ThreadAffinity& operator=(const ThreadAffinity&) = delete;

  /** Binds to the calling thread, or rebinds when @p allowRebind is true. */
  void bindToCurrentThread(const bool allowRebind = false) {
    const std::lock_guard<std::mutex> lock(mMutex);
    if (mBound && !allowRebind) return;
    mThread = std::this_thread::get_id();
    mBound = true;
  }

  /** Binds on the first caller, then keeps that thread. */
  void bindIfUnbound() {
    const std::lock_guard<std::mutex> lock(mMutex);
    if (mBound) return;
    mThread = std::this_thread::get_id();
    mBound = true;
  }

  [[nodiscard]] bool isBound() const {
    const std::lock_guard<std::mutex> lock(mMutex);
    return mBound;
  }

  [[nodiscard]] bool isCurrentThread() const {
    const std::lock_guard<std::mutex> lock(mMutex);
    return !mBound || mThread == std::this_thread::get_id();
  }

  [[nodiscard]] const std::string& name() const noexcept {
    return mName;
  }

  /**
   * Asserts the calling thread in debug builds.
   *
   * A violation is a programming error, not a recoverable condition: continuing would race the
   * device or the layer tree. Release builds compile the check away.
   */
  void assertCurrentThread(const char* what = nullptr) const {
#ifndef NDEBUG
    const bool ok = isCurrentThread();
    if (!ok) {
      const char* label = what != nullptr ? what : "object";
      (void)label;
      assert(false && "UI thread affinity violated");
    }
#else
    (void)what;
#endif
  }

 private:
  std::string mName;
  mutable std::mutex mMutex;
  std::thread::id mThread{};
  bool mBound = false;
};

} // namespace coral::ui::host

#endif // CORAL_UI_HOST_THREADAFFINITY_H
