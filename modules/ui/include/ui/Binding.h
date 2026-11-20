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

#ifndef CORAL_UI_BINDING_H
#define CORAL_UI_BINDING_H

#include <ui/Layer.h>
#include <ui/TextInput.h>

#include <core/observer/Observation.h>
#include <core/observer/Property.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

/**
 * Connects one observer property to a layer's visual state.
 *
 * This is the model→view seam the design asks for, not a general MVVM runtime: the caller supplies
 * `apply`, the bridge invokes it on the model's executor (normally `UiExecutor`), and invalidates the
 * layer. The observation owns the registration; it must not outlive @p layer.
 */
template <class T, class Apply>
[[nodiscard]] observer::Observation bindProperty(const observer::Property<T>& property, Layer& layer,
    Apply&& apply, std::shared_ptr<observer::Executor> executor = nullptr,
    const bool initial = true) {
  std::function<void(const T&)> callback(std::forward<Apply>(apply));
  return property.observe(
    [&layer, callback = std::move(callback)](const T&, const T& value) {
      if (callback) callback(value);
      layer.setNeedsLayout();
      layer.setNeedsPaint();
    },
    std::move(executor), initial);
}

/**
 * Two-way binding for a single-line text field.
 *
 * Model changes call `TextInput::setText`; user edits call `Property::set`. The property suppresses
 * equal writes, so the echo from `setText` does not loop. Destroying the returned handle detaches the
 * model observer and disables the widget callback; the widget must not be rebound while a previous
 * handle is alive because `TextInput` has one change callback.
 */
class TextBinding {
 public:
  TextBinding() = default;
  ~TextBinding();
  TextBinding(TextBinding&&) noexcept;
  TextBinding& operator=(TextBinding&&) noexcept;
  TextBinding(const TextBinding&) = delete;
  TextBinding& operator=(const TextBinding&) = delete;

  [[nodiscard]] bool valid() const noexcept {
    return mObservation.valid();
  }

  void reset() noexcept;

 private:
  friend TextBinding bindText(TextInput&, std::shared_ptr<observer::Property<std::string>>,
    std::shared_ptr<observer::Executor>);

  observer::Observation mObservation;
  observer::Observation mWidgetObservation;
  std::shared_ptr<observer::Property<std::string>> mProperty;
  std::shared_ptr<bool> mAlive;
};

/** Binds @p input and @p property both ways. The property must outlive the returned handle. */
[[nodiscard]] TextBinding bindText(TextInput& input,
  std::shared_ptr<observer::Property<std::string>> property,
  std::shared_ptr<observer::Executor> executor = nullptr);

} // namespace coral::ui

#endif // CORAL_UI_BINDING_H
