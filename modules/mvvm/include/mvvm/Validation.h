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

#ifndef CORAL_MVVM_VALIDATION_H
#define CORAL_MVVM_VALIDATION_H

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace coral::mvvm {

/**
 * An ordered list of rules for one value.
 *
 * A rule returns a human-readable error, or null when the value is acceptable. `validate` stops at
 * the first error, which is the order a form shows one message at a time. Rules are ordinary
 * functions rather than a rule DSL: a product can write a lambda, a free function or a reusable
 * named rule without the framework owning the wording.
 */
template <class T>
class Validation {
 public:
  using Rule = std::function<std::optional<std::string>(const T&)>;

  Validation() = default;
  explicit Validation(std::vector<Rule> rules) : mRules(std::move(rules)) {}

  Validation& add(Rule rule) {
    if (rule) mRules.push_back(std::move(rule));
    return *this;
  }

  [[nodiscard]] std::optional<std::string> validate(const T& value) const {
    for (const Rule& rule: mRules) {
      if (!rule) continue;
      if (std::optional<std::string> error = rule(value); error.has_value()) {
        return error;
      }
    }
    return std::nullopt;
  }

  [[nodiscard]] bool empty() const noexcept {
    return mRules.empty();
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return mRules.size();
  }

 private:
  std::vector<Rule> mRules;
};

/** A rule that rejects an empty string. */
[[nodiscard]] inline Validation<std::string>::Rule required(std::string message) {
  return [message = std::move(message)](const std::string& value) -> std::optional<std::string> {
    if (value.empty()) return message;
    return std::nullopt;
  };
}

/** A rule that rejects a string shorter than @p minimum bytes. */
[[nodiscard]] inline Validation<std::string>::Rule minLength(std::size_t minimum, std::string message) {
  return [minimum, message = std::move(message)](
           const std::string& value) -> std::optional<std::string> {
    if (value.size() < minimum) return message;
    return std::nullopt;
  };
}

} // namespace coral::mvvm

#endif // CORAL_MVVM_VALIDATION_H
