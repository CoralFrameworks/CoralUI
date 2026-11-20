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

#ifndef CORAL_UI_STATE_H
#define CORAL_UI_STATE_H

#include <core/observer/Observation.h>
#include <core/observer/Property.h>

#include <functional>
#include <memory>
#include <utility>

namespace coral::ui {

/**
 * A component-owned value with observers.
 *
 * A component keeps its state in `State` members, observes them once while building, and calls
 * `ComponentContext::invalidate` from the observer to rebuild its subtree. Writes with an equal value
 * are ignored, so an observer that writes back a clamped value cannot loop.
 *
 * The type is UI-thread only, like the layer tree. A background thread that needs to update component
 * state should marshal the write through `Window::post` (or a `mvvm::UiExecutor`), which is exactly
 * what the binding helpers do.
 */
template <typename T>
class State {
 public:
  State()
    : mProperty(std::make_shared<observer::Property<T>>(T{})) {}

  explicit State(T value)
    : mProperty(std::make_shared<observer::Property<T>>(std::move(value))) {}

  State(const State&) = delete;
  State& operator=(const State&) = delete;

  /** A snapshot of the current value (the underlying property returns a copy, never a reference). */
  [[nodiscard]] T get() const {
    return mProperty->get();
  }

  /** @return true when the value changed; an equal write is a no-op. */
  bool set(T value) {
    if (mProperty->get() == value) return false;
    mProperty->set(std::move(value));
    return true;
  }

  /**
   * Observes changes.
   *
   * @param immediate also deliver the current value once, during registration.
   */
  [[nodiscard]] observer::Observation observe(std::function<void(const T&)> handler,
      const bool immediate = false) {
    return mProperty->observe(
      [handler = std::move(handler)](const T&, const T& value) {
        if (handler) handler(value);
      },
      nullptr, immediate);
  }

  /** The underlying property, for the binding helpers that take an `observer::Property`. */
  [[nodiscard]] observer::Property<T>& property() noexcept {
    return *mProperty;
  }
  [[nodiscard]] const observer::Property<T>& property() const noexcept {
    return *mProperty;
  }

 private:
  std::shared_ptr<observer::Property<T>> mProperty;
};

} // namespace coral::ui

#endif // CORAL_UI_STATE_H
