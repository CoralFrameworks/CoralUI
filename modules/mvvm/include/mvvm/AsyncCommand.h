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

#ifndef CORAL_MVVM_ASYNCCOMMAND_H
#define CORAL_MVVM_ASYNCCOMMAND_H

#include "mvvm/Dispatcher.h"

#include <core/observer/Event.h>
#include <core/observer/Property.h>

#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace coral::mvvm {

/**
 * A command whose handler finishes later.
 *
 * The handler receives a `done` callback and calls it once when the work is complete. Until then the
 * command is `running` and a second `execute()` is refused. A throw from the handler is reported as a
 * failure, so a view can show the error without every caller wrapping its work in try/catch.
 *
 * State notifications are posted through the optional `Dispatcher`, so a worker thread can complete
 * the work and have `running`/`error`/`completed` observers run on the UI thread. A command without a
 * dispatcher finishes on whichever thread called `done`; that is the deterministic test path.
 *
 * `done` is idempotent and may be called after `cancel()`; the late completion is ignored.
 */
class AsyncCommand {
 public:
  using AsyncHandler = std::function<void(std::function<void()> done)>;
  using CanExecute = std::function<bool()>;

  /** Emitted after a successful completion. */
  struct Completed {};
  /** Emitted with the failure text after a throw or an explicit error. */
  struct Error {
    std::string message;
  };

  explicit AsyncCommand(AsyncHandler handler = {}, CanExecute canExecute = {},
      std::shared_ptr<Dispatcher> dispatcher = nullptr)
    : mHandler(std::move(handler)), mCanExecute(std::move(canExecute)),
      mCanExecuteValue(evaluateCanExecute()), mState(makeState(std::move(dispatcher))) {
  }

  AsyncCommand(const AsyncCommand&) = delete;
  AsyncCommand& operator=(const AsyncCommand&) = delete;
  AsyncCommand(AsyncCommand&&) = delete;
  AsyncCommand& operator=(AsyncCommand&&) = delete;

  void setHandler(AsyncHandler handler) {
    mHandler = std::move(handler);
    refreshCanExecute();
  }

  void setCanExecute(CanExecute canExecute) {
    mCanExecute = std::move(canExecute);
    refreshCanExecute();
  }

  [[nodiscard]] bool canExecute() const {
    return evaluateCanExecute();
  }

  [[nodiscard]] const observer::Property<bool>& canExecuteProperty() const noexcept {
    return mCanExecuteValue;
  }

  void invalidateCanExecute() {
    refreshCanExecute();
  }

  [[nodiscard]] bool running() const {
    return mState->running.get();
  }

  [[nodiscard]] const observer::Property<bool>& runningProperty() const noexcept {
    return mState->running;
  }

  [[nodiscard]] const observer::Property<std::string>& errorProperty() const noexcept {
    return mState->error;
  }

  [[nodiscard]] observer::Event<Error>& failed() noexcept {
    return mState->failed;
  }

  [[nodiscard]] observer::Event<Completed>& completed() noexcept {
    return mState->completed;
  }

  /**
   * Starts the handler.
   *
   * @return false when the command is disabled, has no handler, or is already running.
   */
  bool execute() {
    if (!canExecute()) return false;
    if (mRun != nullptr && !mRun->finished.load(std::memory_order_acquire)) return false;

    auto run = std::make_shared<Run>();
    mRun = run;
    const std::shared_ptr<State> state = mState;
    state->error.set(std::string());
    state->running.set(true);

    const auto done = [state, run] {
      finish(state, run, {});
    };
    const auto fail = [state, run](std::string message) {
      finish(state, run, std::move(message));
    };

    try {
      mHandler(done);
    } catch (const std::exception& exception) {
      fail(exception.what());
    } catch (...) {
      fail("the command handler threw an unknown exception");
    }
    return true;
  }

  /** Abandons the current run. A later `done` is ignored; observers see `running == false`. */
  void cancel() {
    if (mRun == nullptr) return;
    if (mRun->finished.exchange(true, std::memory_order_acq_rel)) return;
    const std::shared_ptr<State> state = mState;
    apply(state, std::string("cancelled"));
  }

 private:
  struct Run {
    std::atomic<bool> finished{false};
  };

  struct State {
    explicit State(std::shared_ptr<Dispatcher> value) : dispatcher(std::move(value)) {}

    std::shared_ptr<Dispatcher> dispatcher;
    observer::Property<bool> running{false};
    observer::Property<std::string> error{std::string()};
    observer::Event<Completed> completed;
    observer::Event<Error> failed;
  };

  static std::shared_ptr<State> makeState(std::shared_ptr<Dispatcher> dispatcher) {
    return std::make_shared<State>(std::move(dispatcher));
  }

  [[nodiscard]] bool evaluateCanExecute() const {
    if (!mHandler) return false;
    return mCanExecute ? mCanExecute() : true;
  }

  void refreshCanExecute() {
    mCanExecuteValue.set(evaluateCanExecute());
  }

  /**
   * Publishes the terminal state once.
   *
   * The run's atomic flag is what makes a late or duplicated `done` harmless; the state's dispatcher
   * decides which thread the property notifications land on.
   */
  static void finish(const std::shared_ptr<State>& state, const std::shared_ptr<Run>& run,
      std::string error) {
    if (run->finished.exchange(true, std::memory_order_acq_rel)) return;
    apply(state, std::move(error));
  }

  static void apply(const std::shared_ptr<State>& state, std::string error) {
    const std::shared_ptr<Dispatcher> dispatcher = state->dispatcher;
    const auto update = [state, error = std::move(error)] {
      state->running.set(false);
      if (error.empty()) {
        state->completed.emit(Completed{});
      } else {
        state->error.set(error);
        state->failed.emit(Error{ error });
      }
    };
    if (dispatcher != nullptr && !dispatcher->isCurrentThread()) {
      dispatcher->post(update);
    } else {
      update();
    }
  }

  AsyncHandler mHandler;
  CanExecute mCanExecute;
  observer::Property<bool> mCanExecuteValue;
  std::shared_ptr<State> mState;
  std::shared_ptr<Run> mRun;
};

} // namespace coral::mvvm

#endif // CORAL_MVVM_ASYNCCOMMAND_H
