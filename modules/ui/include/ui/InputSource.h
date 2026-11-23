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

#ifndef CORAL_UI_INPUTSOURCE_H
#define CORAL_UI_INPUTSOURCE_H

#include <functional>
#include <string>

namespace coral::ui {

/** The active keyboard input source. */
struct KeyboardInputSource {
  std::string id;
  std::string name;
  [[nodiscard]] bool valid() const noexcept {
    return !id.empty();
  }
};

/** @return the current selected keyboard input source (TIS on macOS). */
[[nodiscard]] KeyboardInputSource currentKeyboardInputSource();

/**
 * Registers a process-wide callback fired when the selected keyboard input source changes.
 *
 * macOS observes `kTISNotifySelectedKeyboardInputSourceChanged`; the callback is dispatched to the
 * main queue. Other platforms keep the handler but never fire it.
 */
void setInputSourceChangedHandler(std::function<void()> handler);

[[nodiscard]] bool supportsInputSourceMonitoring() noexcept;

} // namespace coral::ui

#endif // CORAL_UI_INPUTSOURCE_H
