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

#ifndef CORAL_MVVM_FIELD_H
#define CORAL_MVVM_FIELD_H

#include "mvvm/Validation.h"

#include <core/observer/Property.h>

#include <optional>
#include <string>
#include <utility>

namespace coral::mvvm {

/**
 * One editable value with validation state.
 *
 * A form binds `value` to a text control, `error` to an inline message and `touched` to whether the
 * error should be shown yet. `setValue` marks the field touched and validates; `validate()` is the
 * explicit path for a submit button. Validation does not block the value from changing: a form that
 * rejects a keystroke is worse than one that explains it.
 */
template <class T>
class Field {
 public:
  Field(T initial, Validation<T> validation = {})
    : mValue(std::move(initial)), mValidation(std::move(validation)) {
    refresh();
  }

  Field(const Field&) = delete;
  Field& operator=(const Field&) = delete;
  Field(Field&&) = delete;
  Field& operator=(Field&&) = delete;

  [[nodiscard]] const observer::Property<T>& value() const noexcept {
    return mValue;
  }

  [[nodiscard]] observer::Property<T>& value() noexcept {
    return mValue;
  }

  [[nodiscard]] const observer::Property<std::string>& error() const noexcept {
    return mError;
  }

  [[nodiscard]] const observer::Property<bool>& touched() const noexcept {
    return mTouched;
  }

  [[nodiscard]] observer::Property<bool>& touched() noexcept {
    return mTouched;
  }

  void setValue(T value) {
    mValue.set(std::move(value));
    mTouched.set(true);
    refresh();
  }

  /** Re-runs validation without marking the field touched. @return true when valid. */
  [[nodiscard]] bool validate() {
    refresh();
    return mError.get().empty();
  }

  void reset(T value) {
    mValue.set(std::move(value));
    mTouched.set(false);
    mError.set(std::string());
  }

  [[nodiscard]] bool valid() const {
    return mError.get().empty();
  }

 private:
  void refresh() {
    std::optional<std::string> error = mValidation.validate(mValue.get());
    mError.set(error.value_or(std::string()));
  }

  observer::Property<T> mValue;
  observer::Property<std::string> mError{std::string()};
  observer::Property<bool> mTouched{false};
  Validation<T> mValidation;
};

} // namespace coral::mvvm

#endif // CORAL_MVVM_FIELD_H
