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

#include <ui/Binding.h>

#include <utility>

namespace coral::ui {

TextBinding::~TextBinding() {
  reset();
}

TextBinding::TextBinding(TextBinding&& other) noexcept
    : mObservation(std::move(other.mObservation)),
      mWidgetObservation(std::move(other.mWidgetObservation)),
      mProperty(std::move(other.mProperty)), mAlive(std::move(other.mAlive)) {}

TextBinding& TextBinding::operator=(TextBinding&& other) noexcept {
  if (this == &other) return *this;
  reset();
  mObservation = std::move(other.mObservation);
  mWidgetObservation = std::move(other.mWidgetObservation);
  mProperty = std::move(other.mProperty);
  mAlive = std::move(other.mAlive);
  return *this;
}

void TextBinding::reset() noexcept {
  if (mAlive != nullptr) *mAlive = false;
  mObservation.reset();
  mWidgetObservation.reset();
  mProperty.reset();
  mAlive.reset();
}

TextBinding bindText(TextInput& input, std::shared_ptr<observer::Property<std::string>> property,
    std::shared_ptr<observer::Executor> executor) {
  TextBinding binding;
  if (property == nullptr) return binding;

  auto alive = std::make_shared<bool>(true);
  const std::weak_ptr<bool> weakAlive = alive;
  const std::weak_ptr<observer::Property<std::string>> weakProperty = property;

  // Start from the model, then subscribe. The widget's echo is suppressed by Property's equal-value
  // check, so this does not need a reentrancy flag.
  input.setText(property->get());
  binding.mObservation = property->observe(
    [&input, weakAlive](const std::string&, const std::string& value) {
      const auto aliveFlag = weakAlive.lock();
      if (aliveFlag == nullptr || !*aliveFlag) return;
      input.setText(value);
    },
    std::move(executor), false);
  binding.mWidgetObservation = input.observeChanged(
    [weakProperty, weakAlive](const std::string& value) {
      const auto aliveFlag = weakAlive.lock();
      const auto model = weakProperty.lock();
      if (aliveFlag == nullptr || !*aliveFlag || model == nullptr) return;
      model->set(value);
    });
  binding.mProperty = std::move(property);
  binding.mAlive = std::move(alive);
  return binding;
}

} // namespace coral::ui
