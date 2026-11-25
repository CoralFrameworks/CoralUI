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

#include <ui/WindowRestoration.h>

#include <ui/Window.h>

#include <mutex>
#include <utility>
#include <vector>

namespace coral::ui {

namespace {

std::mutex gRestorationMutex;
WindowRestorationFactory gRestorationFactory;
std::vector<std::shared_ptr<Window>> gRestoredWindows;

} // namespace

void setWindowRestorationFactory(WindowRestorationFactory factory) {
  const std::lock_guard<std::mutex> lock(gRestorationMutex);
  gRestorationFactory = std::move(factory);
}

bool supportsWindowRestoration() noexcept {
  return true;
}

/** Internal: called by the AppKit restoration class. */
std::shared_ptr<Window> restoreWindowForIdentifier(const std::string_view identifier) {
  WindowRestorationFactory factory;
  {
    const std::lock_guard<std::mutex> lock(gRestorationMutex);
    factory = gRestorationFactory;
  }
  return factory ? factory(identifier) : nullptr;
}

/** Internal: keep a restored CoralUI window alive after AppKit takes its NSWindow. */
void retainRestoredWindow(std::shared_ptr<Window> window) {
  if (window == nullptr) return;
  const std::lock_guard<std::mutex> lock(gRestorationMutex);
  gRestoredWindows.push_back(std::move(window));
}

} // namespace coral::ui
