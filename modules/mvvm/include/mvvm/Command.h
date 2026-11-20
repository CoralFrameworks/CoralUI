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

#ifndef CORAL_MVVM_COMMAND_H
#define CORAL_MVVM_COMMAND_H

#include <core/observer/Event.h>
#include <core/observer/Property.h>

#include <functional>
#include <utility>

namespace coral::mvvm {

/**
 * An action a view can bind to a button, menu item or shortcut.
 *
 * A command owns its handler and an optional `canExecute` predicate. `canExecute()` is evaluated on
 * demand, and the observable `canExecuteProperty()` is refreshed after every execution and by
 * `invalidateCanExecute()`. A view binds the property to an enabled state; a command that changes
 * its own availability from the handler is therefore reflected without the view polling.
 *
 * Commands are UI-thread objects: the handler and the predicate normally touch view-model state.
 * A command run from another thread must marshal itself through `ObservableObject::post` or a
 * `Dispatcher`; `mvvm` does not hide that from the caller.
 */
class Command {
 public:
  /** The payload of `executed()`; commands have no arguments. */
  struct Executed {};

  using Handler = std::function<void()>;
  using CanExecute = std::function<bool()>;

  explicit Command(Handler handler = {}, CanExecute canExecute = {})
    : mHandler(std::move(handler)), mCanExecute(std::move(canExecute)),
      mCanExecuteValue(evaluateCanExecute()) {
  }

  Command(const Command&) = delete;
  Command& operator=(const Command&) = delete;
  Command(Command&&) = delete;
  Command& operator=(Command&&) = delete;

  void setHandler(Handler handler) {
    mHandler = std::move(handler);
    refreshCanExecute();
  }

  [[nodiscard]] bool hasHandler() const noexcept {
    return static_cast<bool>(mHandler);
  }

  void setCanExecute(CanExecute canExecute) {
    mCanExecute = std::move(canExecute);
    refreshCanExecute();
  }

  /** The most recently evaluated availability. Bind this to a control's enabled state. */
  [[nodiscard]] const observer::Property<bool>& canExecuteProperty() const noexcept {
    return mCanExecuteValue;
  }

  /** Evaluates the predicate now; `false` also covers a command with no handler. */
  [[nodiscard]] bool canExecute() const {
    return evaluateCanExecute();
  }

  /** Re-evaluates the predicate and publishes it through `canExecuteProperty()`. */
  void invalidateCanExecute() {
    refreshCanExecute();
  }

  /**
   * Runs the handler when allowed.
   *
   * @return false when the command is disabled or has no handler; the handler is not run.
   */
  bool execute() {
    if (!canExecute()) return false;
    mHandler();
    refreshCanExecute();
    mExecuted.emit(Executed{});
    return true;
  }

  /** Emits after a successful `execute()`. Useful for logging and composite commands. */
  [[nodiscard]] observer::Event<Executed>& executed() noexcept {
    return mExecuted;
  }

  [[nodiscard]] observer::Observation observeExecuted(std::function<void()> observer,
      std::shared_ptr<observer::Executor> executor = nullptr) {
    return mExecuted.observe(
      [callback = std::move(observer)](const Executed&) {
        if (callback) callback();
      },
      std::move(executor));
  }

 private:
  [[nodiscard]] bool evaluateCanExecute() const {
    if (!mHandler) return false;
    return mCanExecute ? mCanExecute() : true;
  }

  void refreshCanExecute() {
    mCanExecuteValue.set(evaluateCanExecute());
  }

  Handler mHandler;
  CanExecute mCanExecute;
  observer::Property<bool> mCanExecuteValue;
  observer::Event<Executed> mExecuted;
};

} // namespace coral::mvvm

#endif // CORAL_MVVM_COMMAND_H
