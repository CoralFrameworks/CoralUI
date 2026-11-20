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

#ifndef CORAL_MVVM_DISPATCHER_H
#define CORAL_MVVM_DISPATCHER_H

#include <functional>
#include <utility>

namespace coral::mvvm {

/**
 * The thread a view-model's observable callbacks should run on.
 *
 * `Property`/`Event` deliver on the thread that changed them unless they were given an executor. A
 * view-model that can be mutated from a worker thread therefore needs one dispatcher placed between
 * the model and the view; the UI adapter supplies a dispatcher that posts to the UI thread.
 *
 * This interface only names the operation, not a window or a message loop: `mvvm` deliberately does
 * not depend on `ui` or on `core/event`.
 */
class Dispatcher {
 public:
  virtual ~Dispatcher() = default;

  /** Queues @p task. May run it inline when the caller already is the target thread. */
  virtual void post(std::function<void()> task) = 0;

  /** True when the calling thread is the dispatcher's thread. */
  [[nodiscard]] virtual bool isCurrentThread() const noexcept {
    return true;
  }
};

/** A dispatcher that runs every task inline; the deterministic default for tests and tools. */
class InlineDispatcher final : public Dispatcher {
 public:
  void post(std::function<void()> task) override {
    if (task) task();
  }
};

} // namespace coral::mvvm

#endif // CORAL_MVVM_DISPATCHER_H
