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

#ifndef CORAL_UI_THEMEHOTRELOAD_H
#define CORAL_UI_THEMEHOTRELOAD_H

#include <ui/Theme.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace coral::ui {

class Window;

/** How a `ThemeHotReloader` polls, applies and reports. */
struct ThemeReloadOptions {
  /** How often `poll()` stats the file; 0 checks on every call (tests). */
  std::chrono::milliseconds pollInterval{ 200 };
  /** The theme keys are applied on top of; the defaults match the dark skin. */
  Theme base = Theme::dark();
  /** Called with a line-numbered reason when the file does not parse; the previous theme stays. */
  std::function<void(const std::string& error)> onError;
  /** Called after a successful reload, with the theme that was applied. */
  std::function<void(const Theme&)> onChanged;
};

/**
 * Reloads a theme file while the application runs.
 *
 * The workflow is the same as `SpecHotReloader`'s: construct with the file, `attach(window)`, and edit
 * the file. A successful parse is applied with `Window::setTheme`, which repaints the controls that
 * already exist through `Layer::applyTheme`; a failed parse leaves the last good theme in place and
 * reports the line through `onError`/`lastError()`. The file is stat-ed at most once per
 * `pollInterval` and the reload runs on the UI thread from the window's frame callback.
 *
 * ```cpp
 * ThemeHotReloader reloader("coral.theme");
 * reloader.attach(window);
 * ```
 */
class ThemeHotReloader {
 public:
  explicit ThemeHotReloader(std::string path, ThemeReloadOptions options = {});
  ~ThemeHotReloader();

  ThemeHotReloader(const ThemeHotReloader&) = delete;
  ThemeHotReloader& operator=(const ThemeHotReloader&) = delete;

  /** Parses the file now and applies it when attached; @return false when it is unreadable or invalid. */
  bool reload();
  /** Re-checks the file and reloads it when it changed, at most once per `pollInterval`. */
  void poll();
  /** Applies reloads from the window's frame callbacks and applies the current theme immediately. */
  void attach(Window& window);
  void detach();

  void setEnabled(bool enabled) noexcept {
    mEnabled = enabled;
  }
  [[nodiscard]] bool enabled() const noexcept {
    return mEnabled;
  }
  /** The last theme that parsed successfully. */
  [[nodiscard]] const Theme& theme() const noexcept {
    return mTheme;
  }
  [[nodiscard]] const std::string& lastError() const noexcept {
    return mError;
  }
  /** How many times the file parsed successfully (including the load at construction). */
  [[nodiscard]] uint64_t reloadCount() const noexcept {
    return mReloadCount;
  }
  [[nodiscard]] const std::string& path() const noexcept {
    return mPath;
  }

 private:
  [[nodiscard]] bool changedOnDisk() const;
  void rememberFileState();
  void applyToWindow();

  std::string mPath;
  ThemeReloadOptions mOptions;
  Theme mTheme;
  std::string mError;
  std::filesystem::file_time_type mLastWrite{};
  std::uintmax_t mLastSize = 0;
  bool mHasFileState = false;
  std::chrono::steady_clock::time_point mLastPoll{};
  bool mEnabled = true;
  uint64_t mReloadCount = 0;
  Window* mWindow = nullptr;
  uint64_t mFrameCallbackId = 0;
};

} // namespace coral::ui

#endif // CORAL_UI_THEMEHOTRELOAD_H
