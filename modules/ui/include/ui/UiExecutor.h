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

#ifndef CORAL_UI_UIEXECUTOR_H
#define CORAL_UI_UIEXECUTOR_H

#include <ui/Window.h>

#include <core/observer/Executor.h>

#include <functional>
#include <utility>

namespace coral::ui {

/**
 * An observer executor that delivers callbacks on a window's UI thread.
 *
 * `observer::Property::observe` invokes on the thread that set the property unless given an executor.
 * A model thread must use this one so a binding never touches the layer tree off the UI thread. The
 * executor holds a non-owning pointer to the window: it must not outlive the window.
 */
class UiExecutor final : public observer::Executor {
 public:
  explicit UiExecutor(Window& window) : mWindow(window) {}

  void post(std::function<void()> task) override {
    mWindow.post(std::move(task));
  }

 private:
  Window& mWindow;
};

} // namespace coral::ui

#endif // CORAL_UI_UIEXECUTOR_H
