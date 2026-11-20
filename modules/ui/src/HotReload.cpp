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

#include <ui/HotReload.h>

#include <ui/Window.h>

#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace coral::ui {

SpecHotReloader::SpecHotReloader(std::string path, const ElementRegistry& registry,
    SpecReloadOptions options)
    : mPath(std::move(path)), mRegistry(registry), mOptions(std::move(options)) {
  // Load the file the first time so a window starts with the current spec instead of an empty tree.
  reload();
}

SpecHotReloader::~SpecHotReloader() {
  detach();
}

bool SpecHotReloader::reload() {
  std::ifstream file(mPath, std::ios::binary);
  if (!file) {
    mError = "cannot read " + mPath;
    if (mOptions.onError) mOptions.onError(mError);
    rememberFileState();
    return false;
  }
  std::ostringstream text;
  text << file.rdbuf();
  ElementPtr tree = parseElementSpec(text.str(), mRegistry, mOptions.spec);
  if (tree == nullptr) {
    mError = "invalid element spec in " + mPath;
    if (mOptions.onError) mOptions.onError(mError);
    rememberFileState();
    return false;
  }
  mTree = std::move(tree);
  mError.clear();
  ++mReloadCount;
  rememberFileState();
  if (mOptions.onReloaded) mOptions.onReloaded();
  return true;
}

ElementPtr SpecHotReloader::build() const {
  if (mTree != nullptr) return mTree;
  // Something mountable: a transparent layer that draws nothing, so a missing or broken spec leaves
  // the window empty instead of failing the reconcile.
  return std::make_shared<ColorElement>(Color::MakeRGBA8(0, 0, 0, 0), "#hot-reload-empty");
}

void SpecHotReloader::poll() {
  if (!mEnabled) return;
  const auto now = std::chrono::steady_clock::now();
  if (mLastPoll != std::chrono::steady_clock::time_point{} &&
      now - mLastPoll < mOptions.pollInterval) {
    return;
  }
  mLastPoll = now;
  if (!changedOnDisk()) return;
  reload();
  if (mWindow != nullptr) mWindow->requestFrame();
}

void SpecHotReloader::attach(Window& window) {
  detach();
  mWindow = &window;
  mFrameCallbackId = window.addFrameCallback([this] { poll(); });
}

void SpecHotReloader::detach() {
  if (mWindow != nullptr && mFrameCallbackId != 0) {
    mWindow->removeFrameCallback(mFrameCallbackId);
  }
  mWindow = nullptr;
  mFrameCallbackId = 0;
}

bool SpecHotReloader::changedOnDisk() const {
  std::error_code error;
  const auto write = std::filesystem::last_write_time(mPath, error);
  if (error) return false; // still missing: nothing to reload until it appears
  if (!mHasFileState || write != mLastWrite) return true;
  const std::uintmax_t size = std::filesystem::file_size(mPath, error);
  return !error && size != mLastSize;
}

void SpecHotReloader::rememberFileState() {
  std::error_code error;
  const auto write = std::filesystem::last_write_time(mPath, error);
  if (error) {
    mHasFileState = false;
    return;
  }
  mLastWrite = write;
  mLastSize = std::filesystem::file_size(mPath, error);
  mHasFileState = !error;
}

} // namespace coral::ui
