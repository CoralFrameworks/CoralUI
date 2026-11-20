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

#ifndef CORAL_UI_MVVM_BINDING_H
#define CORAL_UI_MVVM_BINDING_H

#include <mvvm/Command.h>
#include <mvvm/ObservableList.h>

#include <ui/Binding.h>
#include <ui/Button.h>
#include <ui/Command.h>

#include <functional>
#include <utility>

namespace coral::ui::mvvm {

/**
 * MVVM-specific adapters on top of `ui/Binding.h`.
 *
 * The property/text seam stays in `ui/Binding.h`: it is useful without the `mvvm` library and does
 * not depend on it. This header adds the two things a view model has that a raw `Property` does not:
 * command availability and coalesced observable-list changes.
 */

/**
 * Binds a command's enabled state to a view setter.
 *
 * `invoke()` is what a button/menu item calls on activation; the command itself still owns the
 * handler and the `canExecute` predicate.
 */
class CommandBinding {
 public:
  CommandBinding() = default;
  CommandBinding(coral::mvvm::Command& command, std::function<void(bool)> enabledSetter)
    : mCommand(&command), mEnabledSetter(std::move(enabledSetter)) {
    if (!mEnabledSetter) return;
    mObservation = command.canExecuteProperty().observe(
      [this](const bool&, const bool& value) {
        if (mEnabledSetter) mEnabledSetter(value);
      },
      nullptr, true);
  }

  CommandBinding(const CommandBinding&) = delete;
  CommandBinding& operator=(const CommandBinding&) = delete;
  CommandBinding(CommandBinding&& other) noexcept
    : mCommand(other.mCommand), mEnabledSetter(std::move(other.mEnabledSetter)),
      mObservation(std::move(other.mObservation)) {
    other.mCommand = nullptr;
  }
  CommandBinding& operator=(CommandBinding&& other) noexcept {
    if (this != &other) {
      mCommand = other.mCommand;
      mEnabledSetter = std::move(other.mEnabledSetter);
      mObservation = std::move(other.mObservation);
      other.mCommand = nullptr;
    }
    return *this;
  }
  ~CommandBinding() = default;

  [[nodiscard]] bool invoke() const {
    return mCommand != nullptr && mCommand->execute();
  }

  [[nodiscard]] bool canExecute() const {
    return mCommand != nullptr && mCommand->canExecute();
  }

 private:
  coral::mvvm::Command* mCommand = nullptr;
  std::function<void(bool)> mEnabledSetter;
  observer::Observation mObservation;
};

/**
 * Binds a `Button` to a command: the enabled state follows `canExecute`, and a click invokes the
 * command only while it is executable. The binding owns the button's click handler; destroying it
 * clears that handler, so the `Button` must outlive the binding.
 */
class ButtonCommandBinding {
 public:
  ButtonCommandBinding(Button& button, coral::mvvm::Command& command)
    : mButton(&button), mCommand(&command),
      mBinding(command, [&button](const bool enabled) { button.setEnabled(enabled); }) {
    button.setOnClick([this] { invoke(); });
  }

  ButtonCommandBinding(const ButtonCommandBinding&) = delete;
  ButtonCommandBinding& operator=(const ButtonCommandBinding&) = delete;
  ButtonCommandBinding(ButtonCommandBinding&&) = delete;
  ButtonCommandBinding& operator=(ButtonCommandBinding&&) = delete;

  ~ButtonCommandBinding() {
    if (mButton != nullptr) mButton->setOnClick({});
  }

  [[nodiscard]] bool invoke() const { return mBinding.invoke(); }
  [[nodiscard]] bool canExecute() const { return mBinding.canExecute(); }

 private:
  Button* mButton = nullptr;
  coral::mvvm::Command* mCommand = nullptr;
  CommandBinding mBinding;
};

/**
 * Binds a window command to a view-model command.
 *
 * The shortcut and the menu entry live in the `CommandRegistry`; the handler and the availability
 * predicate stay on the `mvvm::Command`. This is the join between "what the application can do" and
 * "where the user triggers it": a menu item bound to `name` greys out exactly when `canExecute()`
 * is false, without a second copy of the state. The binding unbinds on destruction.
 */
class RegistryCommandBinding {
 public:
  RegistryCommandBinding(CommandRegistry& registry, Shortcut shortcut, std::string name,
    coral::mvvm::Command& command)
    : mRegistry(&registry), mName(std::move(name)), mCommand(&command), mBinding(command, {}) {
    mRegistry->bind(shortcut, mName, [this] { mBinding.invoke(); },
      [this] { return mBinding.canExecute(); });
  }

  RegistryCommandBinding(const RegistryCommandBinding&) = delete;
  RegistryCommandBinding& operator=(const RegistryCommandBinding&) = delete;
  RegistryCommandBinding(RegistryCommandBinding&&) = delete;
  RegistryCommandBinding& operator=(RegistryCommandBinding&&) = delete;

  ~RegistryCommandBinding() {
    if (mRegistry != nullptr) mRegistry->unbind(mName);
  }

  [[nodiscard]] bool invoke() const { return mBinding.invoke(); }
  [[nodiscard]] bool canExecute() const { return mBinding.canExecute(); }

 private:
  CommandRegistry* mRegistry = nullptr;
  std::string mName;
  coral::mvvm::Command* mCommand = nullptr;
  CommandBinding mBinding;
};

/**
 * Coalesced structural-change adapter for an `ObservableList`.
 *
 * A view that rebuilds a region on every change uses `onChanged`; a keyed reconciler can subscribe
 * to the individual observations instead. The initial callback runs once so the view can populate.
 */
template <class T>
class ObservableListBinding {
 public:
  ObservableListBinding() = default;
  ObservableListBinding(coral::mvvm::ObservableList<T>& list, std::function<void()> onChanged)
    : mOnChanged(std::move(onChanged)) {
    if (!mOnChanged) return;
    mInserted = list.observeInserted([this](const auto&) { notify(); });
    mRemoved = list.observeRemoved([this](const auto&) { notify(); });
    mMoved = list.observeMoved([this](const auto&) { notify(); });
    mReplaced = list.observeReplaced([this](const auto&) { notify(); });
    mReset = list.observeReset([this](const auto&) { notify(); });
    mOnChanged();
  }

  ObservableListBinding(const ObservableListBinding&) = delete;
  ObservableListBinding& operator=(const ObservableListBinding&) = delete;
  ObservableListBinding(ObservableListBinding&& other) noexcept
    : mOnChanged(std::move(other.mOnChanged)), mInserted(std::move(other.mInserted)),
      mRemoved(std::move(other.mRemoved)), mMoved(std::move(other.mMoved)),
      mReplaced(std::move(other.mReplaced)), mReset(std::move(other.mReset)) {
  }
  ObservableListBinding& operator=(ObservableListBinding&& other) noexcept {
    if (this != &other) {
      mOnChanged = std::move(other.mOnChanged);
      mInserted = std::move(other.mInserted);
      mRemoved = std::move(other.mRemoved);
      mMoved = std::move(other.mMoved);
      mReplaced = std::move(other.mReplaced);
      mReset = std::move(other.mReset);
    }
    return *this;
  }
  ~ObservableListBinding() = default;

 private:
  void notify() {
    if (mOnChanged) mOnChanged();
  }

  std::function<void()> mOnChanged;
  observer::Observation mInserted;
  observer::Observation mRemoved;
  observer::Observation mMoved;
  observer::Observation mReplaced;
  observer::Observation mReset;
};

} // namespace coral::ui::mvvm

#endif // CORAL_UI_MVVM_BINDING_H
