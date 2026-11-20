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

#ifndef CORAL_UI_MVVM_DISPATCHER_H
#define CORAL_UI_MVVM_DISPATCHER_H

#include <ui/Window.h>

#include <mvvm/Dispatcher.h>

#include <functional>
#include <thread>
#include <utility>

namespace coral::ui::mvvm {

/**
 * The `mvvm::Dispatcher` that marshals view-model notifications to a window's UI thread.
 *
 * Bind a worker-side `AsyncCommand` (or any `Property` created with this dispatcher) to a page that
 * lives on the window; `post` goes through `Window::post`, so callbacks run inside the UI loop with
 * the tree — not on the worker thread. The window must outlive the dispatcher, or call `detach()`
 * when the page is destroyed first.
 */
class WindowDispatcher final : public coral::mvvm::Dispatcher {
 public:
  explicit WindowDispatcher(Window& window)
    : mWindow(&window), mThread(std::this_thread::get_id()) {
  }

  void post(std::function<void()> task) override {
    if (mWindow != nullptr && task) mWindow->post(std::move(task));
  }

  [[nodiscard]] bool isCurrentThread() const noexcept override {
    return std::this_thread::get_id() == mThread;
  }

  /** Stops forwarding; safe to call after the window is gone. */
  void detach() noexcept {
    mWindow = nullptr;
  }

 private:
  Window* mWindow = nullptr;
  std::thread::id mThread;
};

} // namespace coral::ui::mvvm

#endif // CORAL_UI_MVVM_DISPATCHER_H
