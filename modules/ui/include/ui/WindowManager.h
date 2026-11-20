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

#ifndef CORAL_UI_WINDOWMANAGER_H
#define CORAL_UI_WINDOWMANAGER_H

#include <ui/DragDrop.h>
#include <ui/Geometry.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace coral::ui {

class Window;

/** Predefined layouts for `WindowManager::arrange`. */
enum class WindowArrange : uint8_t {
  /** Offset every window down/right so the title bars stay visible. */
  Cascade,
  /** Side-by-side columns. */
  TileHorizontal,
  /** Stacked rows. */
  TileVertical,
};

/** A persisted window rectangle: what an application saves between sessions. */
struct WindowGeometry {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  bool maximized = false;
};

/**
 * Tracks the application's top-level windows.
 *
 * The manager owns activation, arrangement and geometry persistence, which are application-level
 * concerns that a single `Window` cannot answer: which window is active, which modal blocks which
 * parent, and where the windows were last time. It never creates windows; `add` takes an existing
 * `Window` and the manager keeps it alive.
 *
 * Geometry is stored under a caller-chosen stable id (a document path, a panel name). `save`/`load`
 * use a line-oriented text format that is deliberately dependency-free and easy to diff:
 * `id|x|y|width|height|maximized`. Malformed lines are skipped on load.
 */
class WindowManager {
 public:
  void add(std::shared_ptr<Window> window);
  void remove(Window* window);

  [[nodiscard]] const std::vector<std::shared_ptr<Window>>& windows() const noexcept {
    return mWindows;
  }
  [[nodiscard]] Window* active() const noexcept {
    return mActive;
  }

  /** Activates @p window unless a modal transient of it is open. */
  bool activate(Window* window);

  /**
   * The window whose screen rectangle contains @p screenPoint, or nullptr.
   *
   * Screen coordinates are physical pixels as reported by the platform; the manager compares them
   * with `Window::bounds()`. Front-to-back order is used, so a window stacked on top wins a tie. This
   * is the hit test a cross-window drag needs before it can route a hover or a drop.
   */
  [[nodiscard]] Window* windowAt(Point screenPoint) const;

  /**
   * Starts a cross-window drag owned by the manager.
   *
   * The manager only routes: hover events go to the window under the screen point, and the drop is
   * delivered to that window's layer tree. A drag ends via `drop()` or `cancelDrag()`; starting a
   * second drag while one is running fails.
   */
  bool beginDrag(std::shared_ptr<Window> source, DragPayload payload);
  /** Moves the drag to @p screenPoint; returns the effect accepted by the window there. */
  bool dragTo(Point screenPoint);
  /** Drops at the last hovered point; returns whether a layer accepted the payload. */
  bool drop();
  void cancelDrag();
  [[nodiscard]] bool dragging() const noexcept {
    return mDragPayload.has_value();
  }
  [[nodiscard]] Window* dragTarget() const noexcept {
    return mDragTarget;
  }
  [[nodiscard]] const DragPayload* dragPayload() const noexcept {
    return mDragPayload.has_value() ? &*mDragPayload : nullptr;
  }

  /** True when a modal window that is transient for @p window is still open. */
  [[nodiscard]] bool isBlocked(const Window& window) const;

  /** Arranges the registered windows inside @p area; returns the assigned rectangles. */
  std::vector<Rect> arrange(WindowArrange mode, Rect area, float gap = 8.0f);

  void setGeometry(const std::string& id, const WindowGeometry& geometry);
  [[nodiscard]] std::optional<WindowGeometry> geometry(const std::string& id) const;

  [[nodiscard]] std::string save() const;
  bool load(const std::string& data);

  void clear();

 private:
  std::vector<std::shared_ptr<Window>> mWindows;
  Window* mActive = nullptr;
  std::optional<DragPayload> mDragPayload;
  Window* mDragSource = nullptr;
  Window* mDragTarget = nullptr;
  Point mDragPosition{};
  DragEffect mDragEffect = DragEffect::None;
  std::map<std::string, WindowGeometry> mGeometry;
};

} // namespace coral::ui

#endif // CORAL_UI_WINDOWMANAGER_H
