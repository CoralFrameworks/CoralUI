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

#ifndef CORAL_UI_HOTRELOAD_H
#define CORAL_UI_HOTRELOAD_H

#include <ui/Element.h>
#include <ui/ElementRegistry.h>
#include <ui/ElementSpec.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace coral::ui {

class Window;

/** How a `SpecHotReloader` polls and reports. */
struct SpecReloadOptions {
  /** How often `poll()` stats the file; 0 checks on every call (tests). */
  std::chrono::milliseconds pollInterval{ 200 };
  /** Inputs the spec syntax cannot express, such as the font a `Text` node needs. */
  ElementSpecOptions spec;
  /** Called with a message when a reload fails; the previous tree stays mounted. */
  std::function<void(const std::string& error)> onError;
  /** Called after a successful reload so the caller can `ElementHost::refresh()`. */
  std::function<void()> onReloaded;
};

/**
 * Re-parses an element-spec file and rebuilds the UI when the file changes.
 *
 * The workflow it exists for:
 *
 * ```cpp
 * SpecHotReloader reloader(path, ElementRegistry::shared(), options);
 * ElementHost host(window, [&] { return reloader.build(); });
 * options.onReloaded = [&] { host.refresh(); };   // set before constructing, in practice
 * host.mount();
 * window.setRoot(host.root());
 * reloader.attach(window);                        // poll() runs from the frame callbacks
 * ```
 *
 * `build()` always returns a mountable tree: before the first successful parse (a missing file, a
 * syntax error) it is an empty `ColorElement`, so a broken spec never clears the window or crashes the
 * reconciler. `reload()` keeps the last good tree when the new text does not parse, and reports why
 * through `lastError()` and `options.onError`.
 *
 * `poll()` does at most one `stat` per `pollInterval` and runs on the UI thread from the window's frame
 * callback, so a hot reload is an ordinary reconcile rather than a second thread touching the tree.
 */
class SpecHotReloader {
 public:
  explicit SpecHotReloader(std::string path,
    const ElementRegistry& registry = ElementRegistry::shared(), SpecReloadOptions options = {});
  ~SpecHotReloader();

  SpecHotReloader(const SpecHotReloader&) = delete;
  SpecHotReloader& operator=(const SpecHotReloader&) = delete;

  /** Parses the file now; @return false when it is unreadable or invalid (the old tree stays). */
  bool reload();
  /** The tree to mount: the last good parse, or an empty placeholder before the first one. */
  [[nodiscard]] ElementPtr build() const;
  /** Re-checks the file and reloads it when it changed, at most once per `pollInterval`. */
  void poll();
  /** Polls from the window's frame callbacks; call `detach()` before the reloader dies. */
  void attach(Window& window);
  void detach();

  void setEnabled(bool enabled) noexcept {
    mEnabled = enabled;
  }
  [[nodiscard]] bool enabled() const noexcept {
    return mEnabled;
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

  std::string mPath;
  const ElementRegistry& mRegistry;
  SpecReloadOptions mOptions;
  ElementPtr mTree;
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

#endif // CORAL_UI_HOTRELOAD_H
