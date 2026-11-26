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

#ifndef CORAL_UI_DSLOVERLAY_H
#define CORAL_UI_DSLOVERLAY_H

#include <ui/Overlay.h>
#include <ui/OverlayHost.h>
#include <ui/Theme.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

class Layer;
class Window;

namespace dsl {

struct DialogButton {
  std::string label;
  std::function<void()> action;
};

/**
 * Thin DSL-shaped façade over OverlayHost.
 *
 * The overlays are still transient Window surfaces, not page nodes; this type only removes the
 * repeated Toast/Popup/Menu/Dialog construction and dismissal bookkeeping from call sites.
 */
class Overlay {
 public:
  Overlay(Window& window, const Theme& theme);
  ~Overlay();

  Overlay(const Overlay&) = delete;
  Overlay& operator=(const Overlay&) = delete;

  uint64_t Toast(std::string text,
    std::chrono::milliseconds duration = std::chrono::milliseconds(2200));
  uint64_t Tooltip(std::string text, Point anchor);
  uint64_t Dialog(std::string title, std::string message = {},
    std::shared_ptr<Layer> content = {}, std::vector<DialogButton> buttons = {});
  uint64_t Popup(std::shared_ptr<Layer> content, OverlayOptions options = {});
  uint64_t Menu(std::vector<std::string> items, Point anchor,
    std::function<void(int)> onSelected = {});

  bool close(uint64_t id);
  void closeAll();
  [[nodiscard]] std::size_t openCount() const noexcept {
    return mHost.size();
  }

 private:
  Window* mWindow;
  Theme mTheme;
  OverlayHost mHost;
};

} // namespace dsl
} // namespace coral::ui

#endif // CORAL_UI_DSLOVERLAY_H
