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

#ifndef CORAL_UI_WINDOWRESTORATION_H
#define CORAL_UI_WINDOWRESTORATION_H

#include <functional>
#include <memory>
#include <string_view>

namespace coral::ui {

class Window;

/**
 * Recreates the CoralUI window for an AppKit restoration identifier.
 *
 * The handler runs on the main thread when AppKit restores a window at launch. It should create a
 * Window with the same restorationIdentifier and return it. CoralUI keeps the returned window alive
 * for the lifetime of the process.
 */
using WindowRestorationFactory =
  std::function<std::shared_ptr<Window>(std::string_view identifier)>;

void setWindowRestorationFactory(WindowRestorationFactory factory);

[[nodiscard]] bool supportsWindowRestoration() noexcept;

} // namespace coral::ui

#endif // CORAL_UI_WINDOWRESTORATION_H
