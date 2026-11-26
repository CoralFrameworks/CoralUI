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

#ifndef CORAL_UI_OVERLAY_H
#define CORAL_UI_OVERLAY_H

#include <ui/Geometry.h>
#include <ui/Layer.h>
#include <ui/Positioner.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace coral::ui {

/** How one transient surface behaves while it is on top of the page. */
struct OverlayOptions {
  /** A modal surface draws a scrim and traps focus; the page behind it is not interactive. */
  bool modal = false;
  /** Draw the dim scrim for a modal surface (a Dialog hosted here may draw its own panel only). */
  bool drawScrim = true;
  bool dismissOnOutsideClick = true;
  bool dismissOnEscape = true;
  /** Non-interactive surfaces (toasts, passive previews) let pointer events reach the page below. */
  bool passThroughPointer = false;
  /** When set, the surface is placed next to this rectangle; otherwise it is filled/centred. */
  Rect anchor{};
  Placement preferred = Placement::Center;
  float gap = 2.0f;
  Insets margin{ 8.0f, 8.0f, 8.0f, 8.0f };
};

/**
 * The window's overlay host: a layer that owns transient surfaces above the page.
 *
 * Every popup/dialog/tooltip that used to be "a layer the caller adds to the root and gives a high
 * z-index" goes through here instead, so one place decides stacking, dismissal (outside click and
 * Escape), the modal scrim and focus trapping. Entries are dismissed by id or top-down; the window
 * listens to `setChangeCallback` to move focus into a modal surface and restore it afterwards.
 *
 * The host itself is added by `Window::pushOverlay` and covers the page, so it is hit-testable while
 * it has entries. A click that misses every entry dismisses the topmost dismissible one (light
 * dismiss) and is consumed; a click that misses a non-dismissible modal is consumed too, which is what
 * makes the page behind it non-interactive. Entries marked `passThroughPointer` (toasts and other
 * passive surfaces) leave the host hit-testable only inside the entry, so the page below still gets
 * its normal pointer events.
 */
class OverlayLayer final : public Layer {
 public:
  OverlayLayer();
  ~OverlayLayer() override;

  /** Adds @p layer on top; @return a stable id for `dismiss`. */
  uint64_t show(std::shared_ptr<Layer> layer, OverlayOptions options = {});

  bool dismiss(uint64_t id);
  /** Re-anchors an entry (a drag preview follows the pointer this way). */
  bool setAnchor(uint64_t id, const Rect& anchor);
  bool dismissTop();
  void dismissAll();

  [[nodiscard]] bool empty() const noexcept {
    return mEntries.empty();
  }
  [[nodiscard]] std::size_t count() const noexcept {
    return mEntries.size();
  }
  [[nodiscard]] Layer* topLayer() const noexcept;
  [[nodiscard]] uint64_t topId() const noexcept;
  [[nodiscard]] bool topIsModal() const noexcept;

  /** Called whenever the set of entries changes (the window moves/restores focus from this). */
  void setChangeCallback(std::function<void()> callback) {
    mChanged = std::move(callback);
  }

  [[nodiscard]] bool isOverlay() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  void onFrame(std::chrono::steady_clock::time_point now) override;
  [[nodiscard]] bool focusable() const override {
    return false;
  }

 private:
  struct Entry {
    uint64_t id = 0;
    std::shared_ptr<Layer> layer;
    OverlayOptions options;
  };

  void notifyChanged() const {
    if (mChanged) mChanged();
  }
  void updateFocusScope();
  void updateInputMode();

  std::vector<Entry> mEntries;
  std::vector<std::shared_ptr<Layer>> mPendingRemoval;
  bool mInFrame = false;
  uint64_t mNextId = 1;
  std::function<void()> mChanged;
  Color mScrim = Color::MakeRGBA8(0, 0, 0, 140);
};

} // namespace coral::ui

#endif // CORAL_UI_OVERLAY_H
