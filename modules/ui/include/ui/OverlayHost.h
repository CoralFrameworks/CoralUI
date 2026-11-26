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

#ifndef CORAL_UI_OVERLAYHOST_H
#define CORAL_UI_OVERLAYHOST_H

#include <ui/Element.h>
#include <ui/Overlay.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace coral::ui {

class Layer;
class Reconciler;
class Window;

/**
 * A per-window host for transient Element subtrees.
 *
 * The page tree stays a page tree: an overlay is mounted into its own `Reconciler`, wrapped in a
 * `Window::pushOverlay` entry, and dismissed through the host. That gives Dialog, Popup, Toast,
 * Tooltip and Menu one lifecycle without pretending they are normal layout nodes.
 *
 * The host is not thread safe; all calls are UI-thread calls, like the rest of the layer tree.
 */
class OverlayHost {
 public:
  explicit OverlayHost(Window& window);
  ~OverlayHost();

  OverlayHost(const OverlayHost&) = delete;
  OverlayHost& operator=(const OverlayHost&) = delete;

  /**
   * Mounts @p content and pushes it as an overlay.
   *
   * The returned id can be passed to `dismiss`. A null or unmountable tree returns 0 and does not
   * change the overlay stack.
   */
  uint64_t show(ElementPtr content, OverlayOptions options = {});

  /**
   * Rebuilds an existing overlay in place.
   *
   * Returns false when the root layer changed: the overlay layer is owned by the window's overlay
   * stack, so a different root has to be dismissed and shown again by the caller.
   */
  bool update(uint64_t id, ElementPtr content);

  bool dismiss(uint64_t id);
  void dismissAll();

  [[nodiscard]] bool contains(uint64_t id) const;
  [[nodiscard]] std::size_t size() const noexcept {
    return mEntries.size();
  }

 private:
  struct Entry {
    uint64_t overlayId = 0;
    std::shared_ptr<Layer> layer;
    std::unique_ptr<Reconciler> reconciler;
  };

  Window* mWindow;
  std::unordered_map<uint64_t, Entry> mEntries;
};

} // namespace coral::ui

#endif // CORAL_UI_OVERLAYHOST_H
