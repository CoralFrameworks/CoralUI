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

#include <ui/ThemeHotReload.h>

#include <ui/ThemeFile.h>
#include <ui/Window.h>

#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace coral::ui {

ThemeHotReloader::ThemeHotReloader(std::string path, ThemeReloadOptions options)
    : mPath(std::move(path)), mOptions(std::move(options)), mTheme(mOptions.base) {
  // Load the file the first time so a window that attaches later starts with the current theme.
  reload();
}

ThemeHotReloader::~ThemeHotReloader() {
  detach();
}

bool ThemeHotReloader::reload() {
  std::ifstream file(mPath, std::ios::binary);
  if (!file) {
    mError = "cannot read " + mPath;
    if (mOptions.onError) mOptions.onError(mError);
    rememberFileState();
    return false;
  }
  std::ostringstream text;
  text << file.rdbuf();
  std::string error;
  const std::optional<Theme> parsed = parseTheme(text.str(), mOptions.base, &error);
  if (!parsed.has_value()) {
    mError = mPath + ": " + error;
    if (mOptions.onError) mOptions.onError(mError);
    rememberFileState();
    return false;
  }
  mTheme = *parsed;
  mError.clear();
  ++mReloadCount;
  rememberFileState();
  if (mWindow != nullptr) applyToWindow();
  if (mOptions.onChanged) mOptions.onChanged(mTheme);
  return true;
}

void ThemeHotReloader::poll() {
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

void ThemeHotReloader::attach(Window& window) {
  detach();
  mWindow = &window;
  applyToWindow();
  mFrameCallbackId = window.addFrameCallback([this] { poll(); });
}

void ThemeHotReloader::detach() {
  if (mWindow != nullptr && mFrameCallbackId != 0) {
    mWindow->removeFrameCallback(mFrameCallbackId);
  }
  mWindow = nullptr;
  mFrameCallbackId = 0;
}

void ThemeHotReloader::applyToWindow() {
  if (mWindow != nullptr) mWindow->setTheme(mTheme);
}

bool ThemeHotReloader::changedOnDisk() const {
  std::error_code error;
  const auto write = std::filesystem::last_write_time(mPath, error);
  if (error) return false; // still missing: nothing to reload until it appears
  if (!mHasFileState || write != mLastWrite) return true;
  const std::uintmax_t size = std::filesystem::file_size(mPath, error);
  return !error && size != mLastSize;
}

void ThemeHotReloader::rememberFileState() {
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
