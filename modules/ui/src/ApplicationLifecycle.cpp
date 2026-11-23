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

#include <ui/ApplicationLifecycle.h>

#include "ApplicationLifecycleInternal.h"

#include <mutex>
#include <utility>

namespace coral::ui {

namespace {

std::mutex gLifecycleMutex;
std::function<void(std::vector<std::string>)> gOpenFiles;
std::function<void()> gReopen;
std::function<bool()> gShouldTerminate;

} // namespace

void setOpenFilesHandler(std::function<void(std::vector<std::string>)> handler) {
  const std::lock_guard<std::mutex> lock(gLifecycleMutex);
  gOpenFiles = std::move(handler);
}

void setReopenHandler(std::function<void()> handler) {
  const std::lock_guard<std::mutex> lock(gLifecycleMutex);
  gReopen = std::move(handler);
}

void setShouldTerminateHandler(std::function<bool()> handler) {
  const std::lock_guard<std::mutex> lock(gLifecycleMutex);
  gShouldTerminate = std::move(handler);
}

namespace detail {

bool dispatchOpenFiles(std::vector<std::string> files) {
  std::function<void(std::vector<std::string>)> handler;
  {
    const std::lock_guard<std::mutex> lock(gLifecycleMutex);
    handler = gOpenFiles;
  }
  if (!handler) return false;
  handler(std::move(files));
  return true;
}

bool dispatchReopen() {
  std::function<void()> handler;
  {
    const std::lock_guard<std::mutex> lock(gLifecycleMutex);
    handler = gReopen;
  }
  if (!handler) return false;
  handler();
  return true;
}

bool dispatchShouldTerminate() {
  std::function<bool()> handler;
  {
    const std::lock_guard<std::mutex> lock(gLifecycleMutex);
    handler = gShouldTerminate;
  }
  return handler ? handler() : true;
}

} // namespace detail

} // namespace coral::ui
