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

#ifndef CORAL_MVVM_OBSERVABLEOBJECT_H
#define CORAL_MVVM_OBSERVABLEOBJECT_H

#include "mvvm/Dispatcher.h"

#include <memory>
#include <utility>

namespace coral::mvvm {

/**
 * Base class for a view-model that needs a dispatcher and an attach/detach lifetime.
 *
 * A view-model owns `observer::Property`/`Event`/`Command` members; this base only carries the
 * cross-cutting pieces:
 *
 * - `setDispatcher`: where notifications should be delivered;
 * - `post`: run a model callback on that thread (inline when no dispatcher is installed);
 * - `onAttached`/`onDetached`: a view calls them when its element tree mounts/unmounts, so a
 *   view-model can start timers or cancel subscriptions without knowing which view it is bound to.
 *
 * The base is intentionally small: no service locator, no navigation, no validation and no
 * `INotifyPropertyChanged`-style string keys. Those belong to the application or to a later layer.
 */
class ObservableObject {
 public:
  virtual ~ObservableObject() = default;

  void setDispatcher(std::shared_ptr<Dispatcher> dispatcher) {
    mDispatcher = std::move(dispatcher);
  }

  [[nodiscard]] const std::shared_ptr<Dispatcher>& dispatcher() const noexcept {
    return mDispatcher;
  }

  /**
   * Runs @p task on the dispatcher's thread.
   *
   * With no dispatcher (or when already on its thread) the task runs inline. That keeps tests and
   * single-threaded tools deterministic; a UI host installs a dispatcher that posts to its UI loop.
   */
  void post(std::function<void()> task) const {
    if (!task) return;
    if (mDispatcher == nullptr || mDispatcher->isCurrentThread()) {
      task();
      return;
    }
    mDispatcher->post(std::move(task));
  }

  /** Called when a view binds this view-model. The default does nothing. */
  virtual void onAttached() {}

  /** Called when the binding is torn down. The default does nothing. */
  virtual void onDetached() {}

 private:
  std::shared_ptr<Dispatcher> mDispatcher;
};

} // namespace coral::mvvm

#endif // CORAL_MVVM_OBSERVABLEOBJECT_H
