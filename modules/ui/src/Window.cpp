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

#include <ui/Window.h>
#include <ui/CandidateListView.h>
#include <backend/Device.h>

#include <ui/Accessibility.h>
#include <ui/FramePlan.h>
#include <ui/Motion.h>

#include "WindowPlatform.h"
#include "TextRenderCache.h"

#include <ui/Command.h>
#include <ui/TextInput.h>
#include <ui/RenderContext.h>
#include <ui/RendererDevice.h>
#include <ui/CanvasBackend.h>
#include <ui/host/FramePacket.h>
#include <ui/host/ThreadedRenderHost.h>
#include <ui/host/ThreadAffinity.h>

#include <graphics/Canvas.h>
#include <gpu/FrameClock.h>
#include <gpu/Recorder.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <atomic>
#include <chrono>
#include <iterator>
#include <mutex>
#include <utility>
#include <vector>

namespace coral::ui {

namespace {

/** Logical pixels the pointer must travel before a press becomes a platform drag-out. */
constexpr float kDragOutThreshold = 4.0f;

Layer* topmostPopup(Layer* layer) {
  if (layer == nullptr || !layer->visible()) return nullptr;
  const auto& children = layer->children();
  for (auto child = children.rbegin(); child != children.rend(); ++child) {
    if (Layer* popup = topmostPopup(child->get()); popup != nullptr) return popup;
  }
  return layer->isPopup() ? layer : nullptr;
}

} // namespace anonymous

struct WindowImpl final : detail::WindowHost {
  ~WindowImpl() override = default;

  explicit WindowImpl(Window::Options options) : options(std::move(options)) {
    uiAffinity.bindToCurrentThread();
    windowTitle = this->options.title;
    // Undo/redo is a window-level service: a focused editor consumes Ctrl+Z itself, otherwise the
    // command table reaches the shared history.
    undoStack.setChangedCallback([this] { markFrameNeeded(); });
    commands.bind(Shortcut{ Key::Z, true, false, false, false }, "edit.undo",
      [this] { undoStack.undo(); }, [this] { return undoStack.canUndo(); });
    commands.bind(Shortcut{ Key::Z, true, true, false, false }, "edit.redo",
      [this] { undoStack.redo(); }, [this] { return undoStack.canRedo(); });
    commands.bindAlias(Shortcut{ Key::Y, true, false, false, false }, "edit.redo");
  }

  Window::Options options;
  /** Debug guard: the layer tree, layout, input and recording all belong to this thread. */
  host::ThreadAffinity uiAffinity{ "window-ui" };
  std::string windowTitle;
  int minWidth = 0;
  int minHeight = 0;
  bool fullscreen = false;
  bool maximized = false;
  bool minimized = false;
  Window::Cursor cursor = Window::Cursor::Arrow;
  Window::Decorations decorations = Window::Decorations::System;
  WindowChromeMetrics chromeMetrics;
  float opacity = 1.0f;
  bool alwaysOnTop = false;
  Window* transientParent = nullptr;
  bool modal = false;
  std::function<bool()> closeRequested;
  std::shared_ptr<const Theme> theme = std::make_shared<const Theme>(Theme::dark());
  Window::ThemeMode themeMode = Window::ThemeMode::Dark;
  /** The window this implementation belongs to, for callbacks that are a `Window` API. */
  Window* self = nullptr;
  /** What the platform reported at creation and at the last settings change. */
  SystemAppearance systemAppearance;
  /** True while the high-contrast palette is the theme in use. */
  bool highContrast = false;
  /** True while the platform reports the window hidden; automatic frames pause. */
  bool occluded = false;
  /** The theme the application installed, kept while high contrast overrides it. */
  std::shared_ptr<const Theme> baseTheme;
  std::shared_ptr<const StyleSheet> styleSheet;
  /**
   * The window command table.
   *
   * Declared before the layer tree on purpose: members are destroyed in reverse declaration order, and
   * a bound `Menu` removes its change listener in its destructor. The registry therefore has to outlive
   * `root`/`overlays`, or closing a window would call into an already destroyed command table.
   */
  CommandRegistry commands;
  std::shared_ptr<Layer> root;
  std::shared_ptr<CandidateListView> candidates;
  /** The overlay host, created on the first push; owns transient surfaces above the page. */
  std::shared_ptr<OverlayLayer> overlays;
  /**
   * The window's own overlay slot: a layer the application puts above the page and above popups.
   *
   * Layered after `overlays` so a development panel (frame stats, a debug HUD) stays readable while a
   * menu is open, and laid out at the window size so a `Flex` inside it can position children
   * absolutely. It is hit-test transparent: a diagnostic slot must not steal clicks from the product.
   */
  std::shared_ptr<Layer> userOverlay;
  /** Focus to restore when the last modal overlay closes. */
  Layer* overlayPreviousFocus = nullptr;
  /** Internal drag session: payload, preview entry, current target and accepted effect. */
  std::optional<DragPayload> dragPayloadData;
  /** The layer an external (OS) drag is currently hovering. */
  Layer* externalDragTarget = nullptr;
  std::shared_ptr<Layer> dragPreview;
  uint64_t dragPreviewOverlayId = 0;
  Layer* dragTarget = nullptr;
  Layer* hoverDragTarget = nullptr;
  DragEffect hoverDragEffect = DragEffect::None;
  DragEffect dragEffect = DragEffect::None;
  Rect damage {};
  bool hasDamage = false;
  Layer* focused = nullptr;
  Layer* hovered = nullptr;
  Layer* pointerCapture = nullptr;
  /** A capture the application asked for with Window::setPointerCapture; takes priority. */
  std::weak_ptr<Layer> publicPointerCapture;
  std::uint32_t publicPointerCaptureId = 0;
  /** A press on a drag-out provider armed a gesture; the origin is where the press landed. */
  bool dragOutPending = false;
  DragPayload dragOutPayload{};
  Point dragOutOrigin{};
  Point pointerPosition{};

  int width = 0;
  int height = 0;
  float dpiScale = 1.0f;
  float uiScale = 1.0f;
  int recoveryCount = 0;
  /** Guards the asynchronous device-lost path so one failure does not start nested recoveries. */
  bool recoveryInFlight = false;
  bool running = true;
  bool inFrame = false;
  int frameCount = 0;
  std::function<void()> frameCallback;
  std::map<uint64_t, std::function<void()>> frameCallbacks;
  uint64_t nextFrameCallbackId = 1;

  void notifyFrameCallbacks() {
    if (frameCallback) frameCallback();
    if (frameCallbacks.empty()) return;
    std::vector<std::function<void()>> callbacks;
    callbacks.reserve(frameCallbacks.size());
    for (const auto& entry: frameCallbacks) callbacks.push_back(entry.second);
    for (const auto& callback: callbacks) {
      if (callback) callback();
    }
  }
  double lastFrameMilliseconds = 0.0;
  double totalFrameMilliseconds = 0.0;
  std::string diagnostics;

  /** Retained-mode counters for the last frame; see `FrameStatistics::dirtyNodes`. */
  uint32_t lastDirtyNodes = 0;
  uint32_t lastLayoutDirtyNodes = 0;
  double lastDamageRatio = 0.0;
  uint32_t lastRepaintedBoundaries = 0;
  uint32_t lastCulledLayers = 0;
  uint32_t lastFramePlanItems = 0;
  uint32_t lastFrameCompositeItems = 0;
  uint64_t lastFrameLentBytes = 0;
  uint32_t lastFramePresentItems = 0;
  uint32_t lastFramePlanItemsShown = 0;
  uint32_t lastFrameCompositeItemsShown = 0;
  uint64_t lastFrameLentBytesShown = 0;
  uint32_t lastFramePresentItemsShown = 0;
  uint64_t lastFrameListBytes = 0;
  FramePolicy policy = FramePolicy::OnDemand;
  std::atomic<bool> frameWanted{ false };
  FrameTimings timings;
  std::shared_ptr<const RendererCapabilities> capabilities;
  std::shared_ptr<RendererDevice> deviceHandle;

  void markFrameNeeded() noexcept {
    frameWanted.store(true, std::memory_order_release);
  }

  /**
   * Samples the retained-mode counters at frame start, before layout clears the layout flags.
   *
   * A node counts as paint-dirty when its own `needsPaint` is set; the subtree flag is an aggregate
   * and would count an entire ancestor chain for one changed control.
   */
  void countDirtyNodes() noexcept {
    lastDirtyNodes = 0;
    lastLayoutDirtyNodes = 0;
    if (root != nullptr) root->collectDirtyCounts(lastDirtyNodes, lastLayoutDirtyNodes);
    if (userOverlay != nullptr) userOverlay->collectDirtyCounts(lastDirtyNodes, lastLayoutDirtyNodes);
  }

  void damageAll() noexcept {
    damage = Rect::MakeWH(static_cast<float>(width), static_cast<float>(height));
    hasDamage = true;
  }

  /**
   * Damage for this frame in device pixels, plus whether a partial repaint is worth it.
   *
   * Controls mark their bounds in `setNeedsPaint`; a scale/rotation or a dirty subtree with no usable
   * rectangle falls back to the whole window. The decision reads the platform's damage capability,
   * which is a static backend property, so the threaded path (whose surface lives on the render
   * thread) makes the same call as the immediate one instead of hard-coding full repaints.
   */
  [[nodiscard]] Rect computeFrameDamage(bool* partial) const {
    Rect frameDamage = hasDamage ? damage : Rect{};
    if (root != nullptr) {
      Rect logicalDamage;
      const bool mapped = root->collectDamage(logicalDamage);
      const bool dirtyWithoutRect = root->subtreeNeedsPaint() &&
        (logicalDamage.getWidth() <= 0.0f || logicalDamage.getHeight() <= 0.0f);
      if (!mapped || dirtyWithoutRect) {
        frameDamage = Rect::MakeLTRB(0.0f, 0.0f, static_cast<float>(width),
          static_cast<float>(height));
      } else if (logicalDamage.getWidth() > 0.0f && logicalDamage.getHeight() > 0.0f) {
        const Rect scaled = Rect::MakeLTRB(logicalDamage.getLeft() * uiScale,
          logicalDamage.getTop() * uiScale, logicalDamage.getRight() * uiScale,
          logicalDamage.getBottom() * uiScale);
        if (frameDamage.getWidth() <= 0.0f || frameDamage.getHeight() <= 0.0f) {
          frameDamage = scaled;
        } else {
          frameDamage = Rect::MakeLTRB(std::min(frameDamage.getLeft(), scaled.getLeft()),
            std::min(frameDamage.getTop(), scaled.getTop()),
            std::max(frameDamage.getRight(), scaled.getRight()),
            std::max(frameDamage.getBottom(), scaled.getBottom()));
        }
      }
    }
    if (userOverlay != nullptr) {
      // The overlay paints above the page, so its damage has to join the frame damage; otherwise a
      // damaged overlay is never repainted and successive text generations stack on the target.
      Rect overlayDamage;
      const bool mapped = userOverlay->collectDamage(overlayDamage);
      const bool dirtyWithoutRect = userOverlay->subtreeNeedsPaint() &&
        (overlayDamage.getWidth() <= 0.0f || overlayDamage.getHeight() <= 0.0f);
      if (!mapped || dirtyWithoutRect) {
        frameDamage = Rect::MakeLTRB(0.0f, 0.0f, static_cast<float>(width),
          static_cast<float>(height));
      } else if (overlayDamage.getWidth() > 0.0f && overlayDamage.getHeight() > 0.0f) {
        const Rect scaled = Rect::MakeLTRB(overlayDamage.getLeft() * uiScale,
          overlayDamage.getTop() * uiScale, overlayDamage.getRight() * uiScale,
          overlayDamage.getBottom() * uiScale);
        if (frameDamage.getWidth() <= 0.0f || frameDamage.getHeight() <= 0.0f) {
          frameDamage = scaled;
        } else {
          frameDamage = Rect::MakeLTRB(std::min(frameDamage.getLeft(), scaled.getLeft()),
            std::min(frameDamage.getTop(), scaled.getTop()),
            std::max(frameDamage.getRight(), scaled.getRight()),
            std::max(frameDamage.getBottom(), scaled.getBottom()));
        }
      }
    }
    const float fullArea = static_cast<float>(width) * static_cast<float>(height);
    const bool damaged = platform != nullptr && platform->supportsDamageRects() &&
      frameDamage.getWidth() > 0.0f && frameDamage.getHeight() > 0.0f &&
      frameDamage.getWidth() * frameDamage.getHeight() < fullArea * 0.9f;
    if (partial != nullptr) *partial = damaged;
    return frameDamage;
  }

  /** Clears the accumulated damage after a frame consumed it. */
  void clearFrameDamage() noexcept {
    damage = Rect{};
    hasDamage = false;
    if (root != nullptr) root->clearDamage();
    if (userOverlay != nullptr) userOverlay->clearDamage();
  }

  std::unique_ptr<detail::WindowPlatform> platform;
  /**
   * Non-null when this window records packets for a render thread. It owns the device and the
   * window/offscreen target; the UI thread never calls `platform->beginFrame`/`present` in this mode.
   */
  std::shared_ptr<host::ThreadedRenderHost> threadedHost;
  bool ownsThreadedHost = true;
  uint64_t renderWindowId = 0;
  uint64_t frameSequence = 0;
  std::shared_ptr<TextRenderCache> textCache = std::make_shared<TextRenderCache>();
  std::unique_ptr<graphics::gpu::VBlankSource> vblank;
  std::unique_ptr<graphics::gpu::FrameClock> clock;
  /** The window-level undo history shared by menus, editors and inspectors. */
  UndoStack undoStack;
  std::mutex taskMutex;
  std::vector<std::function<void()>> tasks;

  [[nodiscard]] Size logicalSize() const noexcept {
    return { static_cast<float>(width) / uiScale, static_cast<float>(height) / uiScale };
  }

  [[nodiscard]] static Layer* firstFocusableLayer(Layer* node) {
    if (node == nullptr || !node->visible()) return nullptr;
    if (node->focusable()) return node;
    for (const std::shared_ptr<Layer>& child: node->children()) {
      if (Layer* found = firstFocusableLayer(child.get())) return found;
    }
    return nullptr;
  }

  [[nodiscard]] bool isInTree(const Layer* layer) const {
    if (layer == nullptr || root == nullptr) return false;
    const auto contains = [layer](const Layer* node, const auto& self) -> bool {
      if (node == nullptr) return false;
      if (node == layer) return true;
      for (const std::shared_ptr<Layer>& child: node->children()) {
        if (self(child.get(), self)) return true;
      }
      return false;
    };
    return contains(root.get(), contains);
  }

  /** Keeps focus inside a modal overlay and restores the interrupted focus when it closes. */
  void onOverlayChanged() {
    if (overlays == nullptr) return;
    if (!overlays->empty() && overlays->topIsModal()) {
      if (overlayPreviousFocus == nullptr) overlayPreviousFocus = focused;
      const bool focusInside = focused != nullptr && isWithin(focused, overlays.get());
      if (!focusInside) {
        if (Layer* first = firstFocusableLayer(overlays->topLayer())) setFocus(first);
      }
    } else if (overlays->empty()) {
      Layer* previous = overlayPreviousFocus;
      overlayPreviousFocus = nullptr;
      if (previous != nullptr && isInTree(previous)) setFocus(previous);
    }
    markFrameNeeded();
  }

  uint64_t pushOverlay(std::shared_ptr<Layer> layer, OverlayOptions options) {
    if (layer == nullptr || root == nullptr) return 0;
    if (overlays == nullptr) {
      overlays = std::make_shared<OverlayLayer>();
      overlays->setChangeCallback([this] { onOverlayChanged(); });
      root->addChild(overlays);
    }
    overlays->setBounds(root->bounds());
    layoutRoot();
    const uint64_t id = overlays->show(std::move(layer), std::move(options));
    markFrameNeeded();
    return id;
  }

  bool dismissOverlay(const uint64_t id) {
    if (overlays == nullptr) return false;
    const bool dismissed = overlays->dismiss(id);
    if (dismissed) markFrameNeeded();
    return dismissed;
  }

  bool dismissTopOverlay() {
    return overlays != nullptr && !overlays->empty() && dismissOverlay(overlays->topId());
  }

  void dismissAllOverlays() {
    if (overlays == nullptr) return;
    overlays->dismissAll();
    markFrameNeeded();
  }

  [[nodiscard]] bool hasOverlay() const noexcept {
    return overlays != nullptr && !overlays->empty();
  }

  bool beginDrag(DragPayload payload, std::shared_ptr<Layer> preview) {
    if (dragPayloadData.has_value() || payload.empty() || root == nullptr) return false;
    dragPayloadData = std::move(payload);
    dragTarget = nullptr;
    dragEffect = DragEffect::None;
    if (preview != nullptr) {
      const Size size = preview->measure({});
      preview->setBounds(Rect::MakeXYWH(pointerPosition.x + 12.0f, pointerPosition.y + 16.0f,
        size.width, size.height));
      OverlayOptions options;
      options.anchor = Rect::MakeXYWH(pointerPosition.x + 12.0f, pointerPosition.y + 16.0f,
        0.0f, 0.0f);
      options.preferred = Placement::BelowStart;
      options.gap = 0.0f;
      options.margin = Insets{ 0.0f, 0.0f, 0.0f, 0.0f };
      options.dismissOnOutsideClick = false;
      options.dismissOnEscape = false;
      dragPreview = std::move(preview);
      dragPreviewOverlayId = pushOverlay(dragPreview, options);
    }
    updateDrag(pointerPosition);
    return true;
  }

  void cancelDrag() {
    endDrag();
  }

  [[nodiscard]] bool isDragging() const noexcept {
    return dragPayloadData.has_value();
  }

  [[nodiscard]] const DragPayload* currentDragPayload() const noexcept {
    return dragPayloadData.has_value() ? &*dragPayloadData : nullptr;
  }

  void updateDrag(const Point point) {
    if (!dragPayloadData.has_value() || root == nullptr) return;
    Layer* hit = root->hitTest(point);
    DragEvent event;
    event.position = point;
    event.payload = *dragPayloadData;
    Layer* acceptor = nullptr;
    DragEffect effect = DragEffect::None;
    for (Layer* layer = hit; layer != nullptr; layer = layer->parent()) {
      // The overlay and the page behind it are not drop targets during an internal drag.
      if (layer == overlays.get()) break;
      const DragEffect candidate = layer == dragTarget ? layer->onDragOver(event)
                                                       : layer->onDragEnter(event);
      if (candidate != DragEffect::None) {
        acceptor = layer;
        effect = candidate;
        break;
      }
    }
    if (dragTarget != nullptr && dragTarget != acceptor) dragTarget->onDragLeave();
    dragTarget = acceptor;
    dragEffect = effect;
    if (dragPreviewOverlayId != 0 && overlays != nullptr) {
      overlays->setAnchor(dragPreviewOverlayId,
        Rect::MakeXYWH(point.x + 12.0f, point.y + 16.0f, 0.0f, 0.0f));
      if (dragPreview != nullptr) {
        const Size size = dragPreview->measuredSize();
        dragPreview->setBounds(Rect::MakeXYWH(point.x + 12.0f, point.y + 16.0f, size.width,
          size.height));
      }
    }
    markFrameNeeded();
  }

  void finishDrag(const Point point) {
    if (!dragPayloadData.has_value()) return;
    Layer* target = dragTarget;
    if (target != nullptr && root != nullptr) {
      DropEvent drop;
      drop.position = point;
      drop.payload = *dragPayloadData;
      drop.effect = dragEffect;
      Layer* hit = root->hitTest(point);
      for (Layer* layer = hit; layer != nullptr; layer = layer->parent()) {
        if (layer->onDrop(drop)) break;
        // Descendants of the accepting target get the drop first, then the target itself.
        if (layer == target) break;
      }
    }
    endDrag();
  }

  void endDrag() {
    if (dragTarget != nullptr) {
      dragTarget->onDragLeave();
      dragTarget = nullptr;
    }
    if (dragPreviewOverlayId != 0) {
      dismissOverlay(dragPreviewOverlayId);
      dragPreviewOverlayId = 0;
    }
    dragPreview.reset();
    dragPayloadData.reset();
    dragEffect = DragEffect::None;
    markFrameNeeded();
  }

  void layoutRoot() {
    if (root == nullptr) return;
    const Size size = logicalSize();
    root->setBounds(Rect::MakeWH(size.width, size.height));
    root->measure(size);
    root->layout();
    if (overlays != nullptr) overlays->setBounds(root->bounds());
    if (userOverlay != nullptr) {
      userOverlay->setBounds(root->bounds());
      userOverlay->measure(size);
      userOverlay->layout();
    }
  }

  void setUserOverlay(std::shared_ptr<Layer> overlay) {
    // The overlay is a second tree, not a child of the page: the page lays out its own children and
    // would fight the window over the overlay bounds. It is drawn after the root at both draw sites.
    userOverlay = std::move(overlay);
    if (userOverlay != nullptr) {
      // A diagnostic panel must not take pointer input away from the page underneath it.
      userOverlay->setHitTestable(false);
    }
    layoutRoot();
    markFrameNeeded();
  }

  void placeCandidates() {
    if (candidates == nullptr || !candidates->visible()) return;
    const Size pref = candidates->preferredSize();
    const Size canvas = logicalSize();
    const Rect caret = focused != nullptr ? focused->imeCursorRect() : Rect{};
    float left = caret.getLeft();
    float top = caret.getBottom() + 2.0f;
    left = std::clamp(left, 0.0f, std::max(0.0f, canvas.width - pref.width));
    if (top + pref.height > canvas.height) top = std::max(0.0f, caret.getTop() - pref.height - 2.0f);
    candidates->setBounds(Rect::MakeXYWH(left, top, pref.width, pref.height));
  }

  void showCandidates(std::vector<std::string> items, const size_t selected) {
    if (candidates == nullptr) {
      candidates = std::make_shared<CandidateListView>(theme != nullptr ? theme->body : nullptr);
    } else if (theme != nullptr) {
      candidates->setFont(theme->body);
    }
    candidates->setCandidates(std::move(items), selected);
    candidates->setVisible(!candidates->candidates().empty());
    placeCandidates();
    markFrameNeeded();
  }

  void setFocus(Layer* layer) {
    // A disabled subtree takes no input, so it must not take focus either.
    if (layer != nullptr && !layer->inputEnabled()) layer = nullptr;
    if (focused == layer) return;
    if (focused != nullptr) focused->onFocusChanged(false);
    focused = layer;
    if (focused != nullptr) focused->onFocusChanged(true);
    if (platform != nullptr) {
      platform->onFocusChanged(focused);
      // Assistive technology follows focus, not frames, so the bridge is told as soon as it moves.
      platform->raiseAccessibilityFocusEvent();
    }
  }

  /**
   * The active focus scope: highest explicit priority, then deeper, then later paint order.
   *
   * A scope fading out (or already transparent) no longer owns focus; keeping it active until its
   * layer is destroyed would let a closing modal trap focus indefinitely.
   */
  [[nodiscard]] Layer* topmostFocusScope(Layer* layer) const {
    struct Candidate {
      Layer* layer = nullptr;
      int priority = 0;
      int depth = 0;
      int order = 0;
    };
    Candidate best;
    int order = 0;
    const auto visit = [&](auto&& self, Layer* current, const int depth) -> void {
      if (current == nullptr || !current->visible() || current->opacity() <= 0.01f) return;
      if (current->isFocusScope()) {
        const Candidate candidate{ current, current->focusScopePriority(), depth, order++ };
        if (best.layer == nullptr || candidate.priority > best.priority ||
            (candidate.priority == best.priority && candidate.depth > best.depth) ||
            (candidate.priority == best.priority && candidate.depth == best.depth &&
              candidate.order > best.order)) {
          best = candidate;
        }
      }
      for (const auto& child: current->children()) {
        if (child != nullptr) self(self, child.get(), depth + 1);
      }
    };
    visit(visit, layer, 0);
    return best.layer;
  }

  [[nodiscard]] static bool isWithin(const Layer* layer, const Layer* ancestor) {
    for (const Layer* current = layer; current != nullptr; current = current->parent()) {
      if (current == ancestor) {
        return true;
      }
    }
    return false;
  }

  static void collectFocusable(Layer* layer, std::vector<Layer*>* out) {
    if (layer == nullptr || !layer->visible()) {
      return;
    }
    if (layer->focusable() && layer->inputEnabled()) {
      out->push_back(layer);
    }
    for (const auto& child: layer->children()) {
      collectFocusable(child.get(), out);
    }
  }

  /** Moves focus to the next/previous focusable layer, staying inside the topmost focus scope. */
  void focusNext(const bool backwards) {
    Layer* scope = topmostFocusScope(root.get());
    Layer* start = scope != nullptr ? scope : root.get();
    std::vector<Layer*> candidates;
    collectFocusable(start, &candidates);
    if (candidates.empty()) {
      return;
    }
    // Within the active scope, a higher-priority child scope owns Tab order first; depth and paint
    // order break ties. A stable sort preserves the old order when every rank is 0.
    struct Rank {
      int tabOrder = 0;
      int priority = 0;
      int depth = 0;
    };
    const auto rank = [](const Layer* layer) {
      Rank result;
      result.tabOrder = layer->tabOrder();
      int distance = 0;
      for (const Layer* ancestor = layer->parent(); ancestor != nullptr;
           ancestor = ancestor->parent()) {
        ++distance;
        if (ancestor->isFocusScope()) {
          result.priority = std::max(result.priority, ancestor->focusScopePriority());
          result.depth = std::max(result.depth, distance);
        }
      }
      return result;
    };
    std::stable_sort(candidates.begin(), candidates.end(), [&](Layer* left, Layer* right) {
      const Rank leftRank = rank(left);
      const Rank rightRank = rank(right);
      if (leftRank.tabOrder != rightRank.tabOrder) return leftRank.tabOrder < rightRank.tabOrder;
      if (leftRank.priority != rightRank.priority) return leftRank.priority > rightRank.priority;
      return leftRank.depth > rightRank.depth;
    });
    const auto position = std::find(candidates.begin(), candidates.end(), focused);
    if (position == candidates.end()) {
      setFocus(backwards ? candidates.back() : candidates.front());
      return;
    }
    const auto index = static_cast<std::size_t>(std::distance(candidates.begin(), position));
    const std::size_t next = backwards
                               ? (index + candidates.size() - 1) % candidates.size()
                               : (index + 1) % candidates.size();
    setFocus(candidates[next]);
  }

  void dispatchPointer(const PointerEvent& event, const int phase) override {
    markFrameNeeded();
    PointerEvent logical = event;
    logical.position = { event.position.x / uiScale, event.position.y / uiScale };
    // An internal drag owns the pointer: moves update the target, release drops, a new press
    // cancels. The page does not see the events a drag intercepts.
    if (dragPayloadData.has_value()) {
      if (phase == 0) {
        pointerPosition = logical.position;
        updateDrag(logical.position);
        return;
      }
      if (phase == 2) {
        finishDrag(logical.position);
        return;
      }
      cancelDrag();
      return;
    }
    // A platform drag-out started by the press: the platform owns the pointer until it finishes, and
    // the widget that offered the payload does not see the move that began it.
    if (phase == 0 && dragOutPending) {
      const float dx = logical.position.x - dragOutOrigin.x;
      const float dy = logical.position.y - dragOutOrigin.y;
      if (dx * dx + dy * dy >= kDragOutThreshold * kDragOutThreshold) {
        dragOutPending = false;
        DragEffect effect = DragEffect::None;
        if (platform != nullptr && !platform->headless()) {
          effect = platform->beginDragOut(dragOutPayload, DragEffect::Copy);
        }
        dragOutPayload = DragPayload{};
        if (effect != DragEffect::None) {
          pointerCapture = nullptr;
          return;
        }
        // No platform drag here (headless, or a backend without a drag-out entry point), so the
        // widget keeps its normal pointer handling and the gesture degrades to a selection.
      }
    }
    // Resolve the capture before the leave check: a slider or splitter that started a drag must keep
    // receiving moves after the pointer leaves the window, which is the whole point of the capture.
    Layer* captured = publicPointerCapture.lock().get();
    const bool capturedEvent = captured != nullptr &&
      (publicPointerCaptureId == 0 || publicPointerCaptureId == event.pointerId);
    if (!event.inside) {
      // The pointer left the window: drop hover so no control stays highlighted while the pointer is
      // elsewhere. A captured control still receives the event through the platform's implicit grab.
      if (hovered != nullptr) {
        hovered->onPointerLeave();
        hovered = nullptr;
      }
      if (!capturedEvent && pointerCapture == nullptr) return;
    }
    Layer* target = capturedEvent
                      ? captured
                      : (pointerCapture != nullptr
                           ? pointerCapture
                           : (event.inside && root != nullptr ? root->hitTest(logical.position) : nullptr));
    // A disabled control and everything inside it are inert: not hovered, not clicked, not captured.
    if (target != nullptr && !target->inputEnabled()) {
      if (hovered != nullptr) {
        hovered->onPointerLeave();
        hovered = nullptr;
      }
      return;
    }
    if (phase == 0) {
      if (hovered != target) {
        if (hovered != nullptr) hovered->onPointerLeave();
        hovered = target;
      }
    }
    // The overlay host is the top of the transient stack: the page below it must not see the event.
    Layer* boundary = overlays != nullptr && overlays->visible() ? overlays.get() : nullptr;
    Layer* handledBy = nullptr;
    for (Layer* layer = target; layer != nullptr; layer = layer->parent()) {
      bool handled = false;
      if (phase == 0) handled = layer->onPointerMove(logical);
      else if (phase == 1) handled = layer->onPointerDown(logical);
      else handled = layer->onPointerUp(logical);
      if (handled) {
        handledBy = layer;
        break;
      }
      if (layer == boundary) break;
    }
    if (phase == 1 && handledBy != nullptr && handledBy->capturesPointerOnPress()) {
      pointerCapture = handledBy;
      if (platform != nullptr) platform->setPointerCapture(true);
    }
    // A press on a layer that offers a drag-out payload arms the gesture; a later move starts the
    // platform drag. Only the nearest provider is asked: it defines the gesture for its subtree.
    if (phase == 1) {
      dragOutPending = false;
      dragOutPayload = DragPayload{};
      for (Layer* layer = target; layer != nullptr; layer = layer->parent()) {
        if (!layer->dragOutProvider()) {
          continue;
        }
        if (std::optional<DragPayload> payload = layer->dragOutProvider()();
            payload.has_value() && !payload->empty()) {
          dragOutPayload = std::move(*payload);
          dragOutOrigin = logical.position;
          dragOutPending = true;
        }
        break;
      }
    }
    if (phase == 2) {
      dragOutPending = false;
      dragOutPayload = DragPayload{};
      if (pointerCapture != nullptr && platform != nullptr) platform->setPointerCapture(false);
      pointerCapture = nullptr;
      if (capturedEvent) {
        publicPointerCapture.reset();
        publicPointerCaptureId = 0;
      }
    }
    if (phase == 1) {
      Layer* scope = topmostFocusScope(root.get());
      Layer* focusTarget = nullptr;
      for (Layer* layer = target; layer != nullptr; layer = layer->parent()) {
        if (!layer->focusable()) {
          continue;
        }
        if (scope != nullptr && !isWithin(layer, scope)) {
          continue;
        }
        focusTarget = layer;
        break;
      }
      if (scope != nullptr) {
        if (focusTarget != nullptr) {
          setFocus(focusTarget);
        } else if (focused == nullptr || !isWithin(focused, scope)) {
          // A click on the modal backdrop does not take focus out of the modal: it moves it to the
          // first focusable control of the scope.
          focusNext(false);
        }
      } else {
        setFocus(focusTarget);
      }
    }
  }

  void dispatchKey(const KeyEvent& event, const bool down) override {
    markFrameNeeded();
    if (dragPayloadData.has_value() && down && event.key == Key::Escape) {
      cancelDrag();
      return;
    }
    if (down && candidates != nullptr && candidates->visible()) {
      const auto& items = candidates->candidates();
      if ((event.key == Key::Down || event.key == Key::Up || event.key == Key::Enter ||
           event.key == Key::Escape) && platform != nullptr &&
          platform->dispatchImeKey(static_cast<uint32_t>(event.key))) {
        // IBus owns selection state; its update/commit signal will refresh or hide the list.
        if (event.key == Key::Escape) candidates->setVisible(false);
        return;
      }
      if (event.key == Key::Down && !items.empty()) {
        candidates->setSelected((candidates->selected() + 1) % items.size());
        return;
      }
      if (event.key == Key::Up && !items.empty()) {
        const size_t count = items.size();
        candidates->setSelected((candidates->selected() + count - 1) % count);
        return;
      }
      if (event.key == Key::Enter && !items.empty()) {
        const std::string chosen = items[candidates->selected()];
        candidates->setCandidates({});
        candidates->setVisible(false);
        dispatchCommit(chosen);
        return;
      }
      if (event.key == Key::Escape) {
        candidates->setCandidates({});
        candidates->setVisible(false);
        return;
      }
    }
    if (down && event.key == Key::Tab) {
      focusNext(event.shift);
      return;
    }
    // A popup menu is a transient overlay: it owns navigation keys even when the control that
    // opened it still holds focus.
    if (down) {
      if (Layer* popup = topmostPopup(root.get()); popup != nullptr && popup != focused) {
        if (popup->onKeyDown(event)) return;
      }
    }
    bool handled = false;
    if (focused != nullptr && focused->inputEnabled()) {
      for (Layer* layer = focused; layer != nullptr; layer = layer->parent()) {
        if (down ? layer->onKeyDown(event) : layer->onKeyUp(event)) {
          handled = true;
          break;
        }
      }
    }
    // An overlay nobody else consumed (a dialog whose focused button declined Escape) closes
    // top-down before the command table sees the key.
    if (!handled && down && overlays != nullptr && overlays->onKeyDown(event)) handled = true;
    // The focused control owns its editing shortcuts; the window table is the fallback for keys the
    // control did not consume.
    if (!handled && down) {
      commands.dispatch(event);
    }
  }

  void dispatchText(const char32_t character) override {
    markFrameNeeded();
    if (focused == nullptr) return;
    for (Layer* layer = focused; layer != nullptr; layer = layer->parent()) {
      if (layer->onTextInput(character)) break;
    }
  }

  void dispatchComposition(const std::string& text) override {
    markFrameNeeded();
    if (focused == nullptr) return;
    for (Layer* layer = focused; layer != nullptr; layer = layer->parent()) {
      if (layer->onTextComposition(text)) break;
    }
  }

  void dispatchCommit(const std::string& text) override {
    markFrameNeeded();
    if (focused == nullptr) return;
    for (Layer* layer = focused; layer != nullptr; layer = layer->parent()) {
      if (layer->onTextCommit(text)) break;
    }
  }

  void dispatchCandidates(std::vector<std::string> items, const size_t selected) override {
    showCandidates(std::move(items), selected);
  }

  void dispatchDelete(const int32_t index, const uint32_t length) override {
    markFrameNeeded();
    if (focused == nullptr) return;
    for (Layer* layer = focused; layer != nullptr; layer = layer->parent()) {
      if (layer->onTextDelete(index, length)) break;
    }
  }

  void dispatchScroll(const ScrollEvent& event) override {
    markFrameNeeded();
    ScrollEvent logical = event;
    logical.position = { event.position.x / uiScale, event.position.y / uiScale };
    // Deltas are device pixels too. Dividing only the position made a 200% window scroll twice as far
    // per wheel notch as the same window at 100%.
    logical.deltaX = event.deltaX / uiScale;
    logical.deltaY = event.deltaY / uiScale;
    Layer* const target = root != nullptr ? root->hitTest(logical.position) : nullptr;
    if (target != nullptr && !target->inputEnabled()) return;
    for (Layer* layer = target; layer != nullptr; layer = layer->parent()) {
      if (layer->onScroll(logical)) break;
    }
  }

  bool dispatchGesture(const GestureEvent& event) override {
    markFrameNeeded();
    GestureEvent logical = event;
    logical.position = { event.position.x / uiScale, event.position.y / uiScale };
    Layer* const target = root != nullptr ? root->hitTest(logical.position) : nullptr;
    if (target != nullptr && !target->inputEnabled()) return false;
    for (Layer* layer = target; layer != nullptr; layer = layer->parent()) {
      if (layer->onGesture(logical)) return true;
    }
    return false;
  }

  DragEffect routeExternalDrag(const DragEvent& event, const int phase) {
    markFrameNeeded();
    Layer* hit = root != nullptr ? root->hitTest(event.position) : nullptr;
    Layer* acceptor = nullptr;
    DragEffect effect = DragEffect::None;
    for (Layer* layer = hit; layer != nullptr; layer = layer->parent()) {
      if (layer == overlays.get()) break;
      const DragEffect candidate = (layer == externalDragTarget && phase != 0)
        ? layer->onDragOver(event) : layer->onDragEnter(event);
      if (candidate != DragEffect::None) {
        acceptor = layer;
        effect = candidate;
        break;
      }
    }
    if (phase == 2) {
      if (acceptor != nullptr) {
        DropEvent drop;
        drop.position = event.position;
        drop.payload = event.payload;
        drop.effect = effect;
        drop.external = true;
        for (Layer* layer = hit; layer != nullptr; layer = layer->parent()) {
          if (layer->onDrop(drop)) break;
          if (layer == acceptor) break;
        }
      }
      if (externalDragTarget != nullptr && externalDragTarget != acceptor) {
        externalDragTarget->onDragLeave();
      }
      externalDragTarget = nullptr;
      return effect;
    }
    if (externalDragTarget != nullptr && externalDragTarget != acceptor) {
      externalDragTarget->onDragLeave();
    }
    externalDragTarget = acceptor;
    return effect;
  }

  DragEffect dispatchDragEnter(const DragEvent& event) override {
    DragEvent copy = event;
    copy.external = true;
    return routeExternalDrag(copy, 0);
  }

  DragEffect dispatchDragOver(const DragEvent& event) override {
    DragEvent copy = event;
    copy.external = true;
    return routeExternalDrag(copy, 1);
  }

  void dispatchDragLeave() override {
    if (externalDragTarget != nullptr) {
      externalDragTarget->onDragLeave();
      externalDragTarget = nullptr;
    }
    markFrameNeeded();
  }

  DragEffect dispatchDragDrop(const DragEvent& event) override {
    DragEvent copy = event;
    copy.external = true;
    return routeExternalDrag(copy, 2);
  }

  void dispatchDrop(const DropEvent& event) override {
    markFrameNeeded();
    DropEvent logical = event;
    logical.position = { event.position.x / uiScale, event.position.y / uiScale };
    if (logical.payload.empty() && !logical.paths.empty()) {
      // A platform file drop reaches the framework as paths; expose it through the same payload
      // model an internal drag uses so a target only implements one path.
      logical.payload = DragPayload::files(logical.paths);
      logical.external = true;
    }
    Layer* target = root != nullptr ? root->hitTest(logical.position) : nullptr;
    for (Layer* layer = target != nullptr ? target : root.get(); layer != nullptr;
         layer = layer->parent()) {
      if (layer->onDrop(logical)) break;
    }
  }

  void platformStateChanged(const bool maximizedValue, const bool minimizedValue,
      const bool fullscreenValue) override {
    const bool changed = maximized != maximizedValue || minimized != minimizedValue ||
      fullscreen != fullscreenValue;
    maximized = maximizedValue;
    minimized = minimizedValue;
    fullscreen = fullscreenValue;
    if (changed) {
      damageAll();
      markFrameNeeded();
    }
  }

  void systemAppearanceChanged() override {
    // The platform reports the event; the policy is Window::applySystemAppearance, so a backend
    // cannot diverge on what high contrast or reduced motion means.
    if (self == nullptr) return;
    self->applySystemAppearance(platform != nullptr ? platform->systemAppearance() : SystemAppearance{});
  }

  void platformOcclusionChanged(const bool visible) override {
    const bool nextOccluded = !visible;
    if (occluded == nextOccluded) return;
    occluded = nextOccluded;
    if (!occluded) {
      // The last presented frame may be arbitrarily old; a hidden window that comes back redraws.
      damageAll();
      markFrameNeeded();
      if (platform != nullptr) platform->wake();
    }
  }

  void platformResized(const int newWidth, const int newHeight) override {
    if (newWidth <= 0 || newHeight <= 0) return;
    markFrameNeeded();
    width = newWidth;
    height = newHeight;
    if (options.uiScale <= 0.0f && options.useDpiScale && platform != nullptr) {
      dpiScale = platform->dpiScale();
      uiScale = options.headless ? 1.0f : dpiScale;
    }
    damageAll();
    if (threadedHost != nullptr) {
      threadedHost->resize(renderWindowId, static_cast<uint32_t>(newWidth),
        static_cast<uint32_t>(newHeight));
    }
    layoutRoot();
  }

  void requestClose() override {
    if (closeRequested && !closeRequested()) return;
    running = false;
  }

  void renderFrameNow() override {
    if (platform == nullptr || !running) return;
    if (inFrame) {
      markFrameNeeded();
      return;
    }
    markFrameNeeded();
    // The caller is a platform loop the user is interacting with (Win32's resize timer, AppKit's
    // live resize): draw immediately rather than sleeping for the display deadline.
    renderFrame(false);
  }

  void requestFrame() override {
    if (platform == nullptr || !running) return;
    // Platform callbacks must not re-enter renderFrame: mark the request and wake the loop. The
    // OnDemand loop is the single place that decides to produce a frame.
    markFrameNeeded();
    platform->wake();
  }

  [[nodiscard]] Layer* focusedLayer() const override {
    return focused;
  }

  WindowHit dispatchHitTest(const Point point) override {
    // The platform boundary is device pixels, the same unit as `width`/`height`: a backend converts
    // its native coordinates once (Cocoa points * dpiScale, X11/Win32 raw client pixels) and every
    // backend then agrees with the metric-free geometry here. The chrome metrics are scaled with
    // `uiScale`, so the hit test runs in logical space and converts the point back.
    const float ui = uiScale > 0.0f ? uiScale : 1.0f;
    const float logicalWidth = static_cast<float>(width) / ui;
    const float logicalHeight = static_cast<float>(height) / ui;
    const Point logical{ point.x / ui, point.y / ui };
    if (decorations == Window::Decorations::System) {
      return logical.x >= 0.0f && logical.y >= 0.0f && logical.x < logicalWidth &&
               logical.y < logicalHeight
               ? WindowHit::Client
               : WindowHit::None;
    }
    return hitTestWindowChrome(Size{ logicalWidth, logicalHeight }, logical, chromeMetrics);
  }

  bool surroundingText(std::string* text, size_t* cursor, size_t* anchor) override {
    if (text == nullptr || cursor == nullptr || anchor == nullptr) return false;
    for (Layer* layer = focused; layer != nullptr; layer = layer->parent()) {
      if (auto* input = dynamic_cast<TextInput*>(layer)) {
        *text = input->text();
        *cursor = input->cursor();
        *anchor = input->selectionAnchor();
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] Rect imeCursorRect() const override {
    return focused != nullptr ? focused->imeCursorRect() : Rect{};
  }

  /**
   * Records one frame on this thread and hands it to the render host.
   *
   * The recorder deliberately has no device context: the `DrawList` it produces is the packet, and
   * the render thread owns the device that will later upload and submit it. `Canvas::isRecording()`
   * was separated from `gpuContext()` so the per-layer cache and `drawExternalImage` still work on
   * this path.
   */
  /** Queues a task for the UI loop; used by the render thread's presentation callback. */
  void queueUiTask(std::function<void()> task) {
    if (!task) return;
    {
      const std::lock_guard<std::mutex> lock(taskMutex);
      tasks.push_back(std::move(task));
    }
    if (platform != nullptr) platform->wake();
  }

  /** The host options for this window's native/offscreen target. */
  host::ThreadedRenderHost::Options threadedOptions() const {
    host::ThreadedRenderHost::Options hostOptions;
    hostOptions.width = static_cast<uint32_t>(width);
    hostOptions.height = static_cast<uint32_t>(height);
    hostOptions.rendererApi = options.rendererApi;
    hostOptions.nativeWindow = platform != nullptr ? platform->nativeWindow() : nullptr;
    if (platform != nullptr) {
      const graphics::gpu::NativeDisplay native = platform->nativeDisplay();
      hostOptions.nativeDisplay = native.display;
      switch (native.kind) {
        case graphics::gpu::NativeDisplayKind::Windows:
          hostOptions.nativeWindowKind = host::ThreadedRenderHost::NativeWindowKind::Windows;
          break;
        case graphics::gpu::NativeDisplayKind::X11:
          hostOptions.nativeWindowKind = host::ThreadedRenderHost::NativeWindowKind::X11;
          break;
        case graphics::gpu::NativeDisplayKind::Wayland:
          hostOptions.nativeWindowKind = host::ThreadedRenderHost::NativeWindowKind::Wayland;
          break;
        case graphics::gpu::NativeDisplayKind::MacOS:
          // The Metal WindowSurface accepts an unknown/portable window system and uses the
          // CAMetalLayer handle from `nativeWindow`.
          hostOptions.nativeWindowKind = host::ThreadedRenderHost::NativeWindowKind::Unknown;
          break;
        case graphics::gpu::NativeDisplayKind::Unknown:
        default:
          hostOptions.nativeWindowKind = host::ThreadedRenderHost::NativeWindowKind::Unknown;
          break;
      }
    }
    hostOptions.label = options.headless ? "ui-headless-threaded" : "ui-window-threaded";
    return hostOptions;
  }

  /**
   * Recreates the render target after a device/surface loss.
   *
   * Owned host: full host teardown/rebuild, because the render thread owns the device and surface, so
   * stopping it first guarantees no resource is used after the old device is gone. Shared host: the
   * host rebuild is a session-wide operation (`Application::recoverRenderDevice`); a single window
   * only asks the host to recover and then refreshes its own capabilities.
   */
  bool recoverDevice() {
    if (platform == nullptr) return false;
    if (threadedHost != nullptr) {
      if (!ownsThreadedHost) {
        diagnostics = "shared render host recovery is a session-wide operation, not a window one";
        return false;
      }
      threadedHost.reset();
      auto replacement = std::make_unique<host::ThreadedRenderHost>(threadedOptions());
      if (!replacement->waitForReady()) {
        diagnostics = replacement->diagnostics();
        return false;
      }
      threadedHost = std::move(replacement);
      renderWindowId = threadedHost->primaryTarget();
      threadedHost->setPresentationCallback(
        [impl = this](const FrameCompletion& completion) {
          impl->onPacketPresented(completion);
        });
      capabilities = std::make_shared<const RendererCapabilities>(
        threadedHost->capabilitiesFor(renderWindowId));
      deviceHandle.reset();
      ++recoveryCount;
      running = true;
      diagnostics.clear();
      if (root != nullptr) root->onDeviceRecovered();
      layoutRoot();
      return true;
    }
    if (!platform->recreate()) {
      diagnostics = platform->diagnostics();
      return false;
    }
    ++recoveryCount;
    running = true;
    diagnostics.clear();
    deviceHandle.reset();
    // A new device may have different limits, so the snapshot is re-probed with it rather than kept
    // from the old one.
    capabilities = std::make_shared<const RendererCapabilities>(
      platform->device() != nullptr
        ? queryRendererCapabilities(platform->device(), true, platform->sampleCount())
        : RendererCapabilities::raster());
    if (root != nullptr) root->onDeviceRecovered();
    layoutRoot();
    return true;
  }

  /** Refreshes this window after the shared host rebuilt its device. */
  bool recoverFromSharedHost() {
    if (threadedHost == nullptr) return false;
    capabilities = std::make_shared<const RendererCapabilities>(
      threadedHost->capabilitiesFor(renderWindowId));
    deviceHandle.reset();
    ++recoveryCount;
    running = true;
    diagnostics.clear();
    if (root != nullptr) root->onDeviceRecovered();
    layoutRoot();
    return true;
  }

  /** Called on the render thread after a packet is presented (or fails). */
  void onPacketPresented(const FrameCompletion& completion) {
    struct Presented {
      uint64_t windowId = 0;
      uint64_t sequence = 0;
      double presentMilliseconds = 0.0;
      std::chrono::steady_clock::time_point submittedAt{};
      std::chrono::steady_clock::time_point presentedAt{};
          bool presented = false;
      bool failed = false;
    } frame;
    frame.windowId = completion.windowId;
    frame.sequence = completion.sequence;
    frame.presentMilliseconds = completion.presentMilliseconds;
    frame.submittedAt = completion.submittedAt;
    frame.presentedAt = completion.presentedAt;
    frame.presented = completion.presented.load(std::memory_order_acquire);
    frame.failed = completion.failed.load(std::memory_order_acquire);
    queueUiTask([this, frame] {
      if (frame.windowId != 0 && frame.windowId != renderWindowId) return;
      if (frame.failed) {
        diagnostics = threadedHost != nullptr ? threadedHost->diagnostics()
                                              : "the render host failed a frame";
        // Unlike the immediate path, an asynchronous submit never makes `renderFrame()` return false,
        // so a failed present is the only device-lost signal this path gets. Recover once, on the UI
        // thread, instead of leaving the window stuck on a dead device.
        if (!recoveryInFlight && threadedHost != nullptr) {
          recoveryInFlight = true;
          bool recovered = false;
          if (ownsThreadedHost) {
            recovered = recoverDevice();
          } else if (threadedHost->recover()) {
            recovered = recoverFromSharedHost();
          }
          recoveryInFlight = false;
          if (recovered) {
            diagnostics.clear();
            markFrameNeeded();
          }
        }
        return;
      }
      if (clock != nullptr && frame.presented) {
        clock->onFramePresented(frame.presentedAt);
        auto milliseconds = [](const std::chrono::steady_clock::duration duration) {
          return std::chrono::duration<double, std::milli>(duration).count();
        };
        timings.present = frame.presentMilliseconds;
        if (frame.submittedAt != std::chrono::steady_clock::time_point{}) {
          timings.total = milliseconds(frame.presentedAt - frame.submittedAt);
          lastFrameMilliseconds = timings.total;
          totalFrameMilliseconds += timings.total;
        }
        notifyFrameCallbacks();
      }
    });
  }

  bool renderThreadedFrame() {
    uiAffinity.assertCurrentThread("Window::renderFrame (render thread)");
    if (threadedHost == nullptr || !running || inFrame) return false;
    struct FrameGuard {
      bool& flag;
      ~FrameGuard() { flag = false; }
    } guard{ inFrame };
    inFrame = true;
    frameWanted.store(false, std::memory_order_release);
    Layer::resetRepaintedBoundaries();
    Layer::resetCulledLayers();
    Layer::resetFrameListBytes();
    const auto frameStart = std::chrono::steady_clock::now();
    clock->requestFrame(std::max(clock->expectedPresentTime(), frameStart));
    if (!clock->waitForFrame()) return false;
    clock->onFrameStarted();
    countDirtyNodes();

    graphics::gpu::Recorder recorder(static_cast<int>(width), static_cast<int>(height), nullptr,
      "ui-thread-frame");
    graphics::Canvas& recorderCanvas = recorder.canvas();
    std::vector<ContentFrame> contentFrames;
    std::vector<ContentHost::FrameFence> contentFences;
    const auto updateStart = std::chrono::steady_clock::now();
    if (root != nullptr) {
      root->onFrame(clock->expectedPresentTime());
    }
    const auto layoutStart = std::chrono::steady_clock::now();
    if (root != nullptr) {
      if (root->needsLayout()) {
        root->measureWithConstraints(Constraints::tight(logicalSize()));
        root->layout();
      } else if (root->subtreeNeedsLayout()) {
        root->layout();
      }
    }
    // The overlay is a second tree: the page layout never touches it, so its own dirty flag is what
    // decides when it needs a new layout pass.
    if (userOverlay != nullptr && userOverlay->needsLayout()) {
      userOverlay->measureWithConstraints(Constraints::tight(logicalSize()));
      userOverlay->layout();
    }
    const auto layoutEnd = std::chrono::steady_clock::now();
    const auto drawStart = layoutEnd;
    bool damaged = false;
    const Rect frameDamage = computeFrameDamage(&damaged);
    if (damaged) {
      // The packet carries the same damage the immediate path would present with; the render thread
      // loads the persistent target and scissors to it instead of clearing the page.
      graphics::Paint clear;
      clear.color = options.clearColor;
      clear.style = graphics::Paint::Style::kFill;
      clear.blendMode = graphics::BlendMode::kSrc;
      recorderCanvas.drawRect(frameDamage, clear);
    } else {
      recorderCanvas.clear(options.clearColor);
    }
    const auto paintStart = std::chrono::steady_clock::now();
    Layer::resetRepaintedBoundaries();
    Layer::resetCulledLayers();
    Layer::resetFrameListBytes();
    std::vector<RenderContext::PlannedItem> plannedItems;
    if (root != nullptr) {
      recorderCanvas.save();
      recorderCanvas.scale(uiScale);
      Canvas uiCanvas = detail::makeCanvas(recorderCanvas);
      RenderContext context(uiCanvas, uiScale, textCache,
        Size{ static_cast<float>(width), static_cast<float>(height) }, capabilities);
      context.setContentSink(&contentFrames, &contentFences);
      // ## The render thread can take the lists instead of a flattened recording
      //
      // Same rule as the immediate path: only when the tree has boundaries, the platform can take lists
      // and the plan is enabled. Everything else records the whole tree as before, which is the packet an
      // ordinary frame has always carried.
      const bool planMode = Window::framePlanEnabled() && platform->supportsBoundarySubmission() &&
        root != nullptr && root->subtreeHasBoundary();
      if (planMode) context.setPlanSink(&plannedItems);
      Layer::beginAnimationFrame();
      root->draw(context);
      if (planMode) {
        const auto collectOverlay = [&](Layer* overlay) {
          if (overlay == nullptr || !overlay->visible()) return;
          if (overlay->borrowRecording().empty() || overlay->subtreeNeedsPaint()) {
            const Size size{ static_cast<float>(width), static_cast<float>(height) };
            overlay->adoptRecording(overlay->recordInto(size, uiScale));
          }
        };
        if (candidates != nullptr && candidates->visible()) {
          placeCandidates();
          collectOverlay(candidates.get());
        }
        collectOverlay(userOverlay.get());
      } else {
        if (candidates != nullptr && candidates->visible()) {
          placeCandidates();
          candidates->draw(context);
        }
        // The overlay is a second tree above the page and above popups, so it never joins the page layout.
        if (userOverlay != nullptr) userOverlay->draw(context);
      }
      recorderCanvas.restore();
    }

    const auto paintEnd = std::chrono::steady_clock::now();
    auto recorded = recorder.finish();
    auto commands = std::make_shared<const graphics::gpu::DrawList>(std::move(recorded));
    FramePacket packet;
    packet.sequence = ++frameSequence;
    packet.windowId = renderWindowId;
    packet.commands = std::move(commands);
    // The lent lists travel in paint order after the content, and the packet presents them as one frame.
    for (const RenderContext::PlannedItem& planned: plannedItems) {
      for (const std::shared_ptr<const graphics::gpu::DrawList>& list: planned.layer->borrowRecording()) {
        if (list == nullptr) continue;
        FrameBoundaryList boundary;
        boundary.list = list;
        boundary.clip = planned.clip;
        boundary.translation = planned.translation;
        packet.boundaries.push_back(std::move(boundary));
      }
    }
    lastFramePlanItems = static_cast<uint32_t>(packet.boundaries.size());
    lastFrameCompositeItems = static_cast<uint32_t>(plannedItems.size());
    lastFrameLentBytes = Layer::lentListBytes();
    lastFramePresentItems = lastFramePlanItems == 0 ? 1u : lastFramePlanItems + 1u;
    packet.damage = damaged ? frameDamage : Rect{};
    packet.pixelSize = Size{ static_cast<float>(width), static_cast<float>(height) };
    packet.uiScale = uiScale;
    packet.contentFrames = std::move(contentFrames);
    packet.fences = std::move(contentFences);
    packet.completion = std::make_shared<FrameCompletion>();
    packet.completion->submittedAt = frameStart;
    if (!threadedHost->submit(std::move(packet))) {
      diagnostics = threadedHost->diagnostics();
      return false;
    }
    // The packet owns what the frame needed; the accumulated damage has been consumed.
    clearFrameDamage();

    const auto frameEnd = std::chrono::steady_clock::now();
    auto milliseconds = [](const std::chrono::steady_clock::duration duration) {
      return std::chrono::duration<double, std::milli>(duration).count();
    };
    timings.update = milliseconds(drawStart - updateStart);
    timings.layout = milliseconds(layoutEnd - layoutStart);
    timings.draw = milliseconds(frameEnd - drawStart);
    timings.paint = milliseconds(paintEnd - paintStart);
    timings.present = 0.0;
    timings.total = milliseconds(frameEnd - frameStart);
    timings.valid = true;
    lastRepaintedBoundaries = Layer::repaintedBoundaries();
    lastCulledLayers = Layer::culledLayers();
    lastFrameListBytes = Layer::frameListBytes();
    lastFramePlanItemsShown = lastFramePlanItems;
    lastFrameCompositeItemsShown = lastFrameCompositeItems;
    lastFrameLentBytesShown = lastFrameLentBytes;
    lastFramePresentItemsShown = lastFramePresentItems;
    lastFramePlanItems = 0;
    lastFrameCompositeItems = 0;
    lastFrameLentBytes = 0;
    lastFramePresentItems = 0;
    lastDamageRatio = damaged ? (frameDamage.getWidth() * frameDamage.getHeight()) /
        std::max(1.0f, static_cast<float>(width) * static_cast<float>(height)) : 0.0;
    ++frameCount;
    lastFrameMilliseconds = timings.total;
    totalFrameMilliseconds += timings.total;
    // Presentation timing and `FrameClock::onFramePresented` arrive from the render thread's
    // completion callback, which `Window::post` marshals onto this thread.
    return true;
  }

  /**
   * Records and presents one frame.
   *
   * @param pace false for a frame the user is actively waiting on (a live-resize tick). The frame
   *        clock's normal job is to start a frame just before its display deadline; during a resize
   *        that wait serialises the window server's own loop, so an interactive frame is drawn as
   *        soon as it is asked for. `renderFrameNow` is the only caller allowed to skip it.
   */
  bool renderFrame(const bool pace = true) {
    uiAffinity.assertCurrentThread("Window::renderFrame");
    if (platform == nullptr || !running || inFrame) return false;
    if (threadedHost != nullptr) return renderThreadedFrame();
    struct FrameGuard {
      bool& flag;
      ~FrameGuard() { flag = false; }
    } guard{ inFrame };
    inFrame = true;
    frameWanted.store(false, std::memory_order_release);
    Layer::resetRepaintedBoundaries();
    Layer::resetCulledLayers();
    Layer::resetFrameListBytes();
    const auto frameStart = std::chrono::steady_clock::now();
    const auto deadline = std::max(clock->expectedPresentTime(), frameStart);
    clock->requestFrame(deadline);
    bool waitForDisplay = pace;
    // Diagnostic switch for A/B profiling: `CORAL_UI_NO_FRAME_PACING=1` removes the display-deadline
    // wait so a frame's own cost can be measured without the clock's pacing.
    if (const char* noPacing = std::getenv("CORAL_UI_NO_FRAME_PACING");
        noPacing != nullptr && std::strcmp(noPacing, "0") != 0) {
      waitForDisplay = false;
    }
    if (waitForDisplay) {
      if (!clock->waitForFrame()) return false;
    }
    clock->onFrameStarted();
    countDirtyNodes();

    // A clean subtree can replay the last canvas recording. The timing split is honest: no update and
    // no draw work happened, only the present.
    if (platform->supportsRetainedFrames() && root != nullptr && !root->subtreeNeedsPaint() &&
        (userOverlay == nullptr || !userOverlay->subtreeNeedsPaint()) &&
        !hasDamage && !root->hasRunningAnimation()) {
      const auto retainedStart = std::chrono::steady_clock::now();
      if (platform->presentRetained()) {
        const auto retainedEnd = std::chrono::steady_clock::now();
        timings.update = 0.0;
        timings.layout = 0.0;
        timings.draw = 0.0;
        timings.paint = 0.0;
        timings.present = std::chrono::duration<double, std::milli>(retainedEnd - retainedStart).count();
        timings.total = std::chrono::duration<double, std::milli>(retainedEnd - frameStart).count();
        timings.valid = true;
        // A retained replay re-recorded nothing, so the counters describe this frame, not the last one
        // that happened to record.
        lastRepaintedBoundaries = 0;
        lastCulledLayers = 0;
        lastFrameListBytes = 0;
        lastDamageRatio = 0.0;
        clock->onFramePresented(retainedEnd);
        ++frameCount;
        lastFrameMilliseconds = timings.total;
        totalFrameMilliseconds += timings.total;
        notifyFrameCallbacks();
        return true;
      }
    }

    graphics::Canvas* canvas = platform->beginFrame();
    if (canvas == nullptr) {
      diagnostics = platform->diagnostics();
      running = false;
      return false;
    }

    const auto updateStart = std::chrono::steady_clock::now();
    if (root != nullptr) {
      // Animations sample the frame's expected present time, not the wall clock at which the UI
      // thread happened to wake: a late vblank then advances the animation once rather than
      // accumulating the scheduling jitter into the next frame.
      root->onFrame(clock->expectedPresentTime());
    }
    const auto layoutStart = std::chrono::steady_clock::now();
    if (root != nullptr) {
      // A layout boundary can absorb a change below it: the root then only needs a layout pass, not a
      // fresh measurement of the whole tree.
      if (root->needsLayout()) {
        root->measureWithConstraints(Constraints::tight(logicalSize()));
        root->layout();
      } else if (root->subtreeNeedsLayout()) {
        root->layout();
      }
    }
    if (userOverlay != nullptr) {
      if (userOverlay->needsLayout()) {
        userOverlay->measureWithConstraints(Constraints::tight(logicalSize()));
        userOverlay->layout();
      } else if (userOverlay->subtreeNeedsLayout()) {
        userOverlay->layout();
      }
    }
    const auto layoutEnd = std::chrono::steady_clock::now();

    const auto drawStart = std::chrono::steady_clock::now();

    bool damaged = false;
    const Rect frameDamage = computeFrameDamage(&damaged);
    // R3's frame: a platform that can take boundary lists gets the tree recorded without them, and the
    // lists are handed over afterwards. It is only worth taking when the tree actually has boundaries --
    // without them the frame is one list and the root keeps its recording, which is the cheaper shape.
    // A plan frame repaints the whole target even when only part of it changed: the lists it lends are
    // whole-list submissions, so a scissored present would cut them.
    const bool planMode = Window::framePlanEnabled() && platform->supportsBoundarySubmission() &&
      root != nullptr && root->subtreeHasBoundary();

    if (damaged && !planMode) {
      // Load the old target outside the damage; the clear covers only the dirty rectangle and the
      // renderer scissor keeps every draw and composite inside it.
      graphics::Paint clear;
      clear.color = options.clearColor;
      clear.style = graphics::Paint::Style::kFill;
      clear.blendMode = graphics::BlendMode::kSrc;
      canvas->drawRect(frameDamage, clear);
    } else {
      canvas->clear(options.clearColor);
    }
    const auto paintStart = std::chrono::steady_clock::now();
    Layer::resetRepaintedBoundaries();
    Layer::resetCulledLayers();
    Layer::resetFrameListBytes();
    std::vector<RenderContext::PlannedItem> plannedItems;
    if (root != nullptr) {
      canvas->save();
      canvas->scale(uiScale);
      Canvas uiCanvas = detail::makeCanvas(*canvas);
      RenderContext context(uiCanvas, uiScale, textCache,
        Size{ static_cast<float>(width), static_cast<float>(height) }, capabilities);
      if (planMode) context.setPlanSink(&plannedItems);
      Layer::beginAnimationFrame();
      root->draw(context);
      if (planMode) {
        // ## The overlays are items of their own, and the last ones
        //
        // They paint above every boundary, and a plan frame submits the boundary lists *after* the content
        // list -- so an overlay flattened into the content would be covered by the panels it hangs over.
        // Recording each one into its own list and appending it keeps the paint order the window means:
        // page content, the boundaries in paint order, then the overlays.

        const auto collectOverlay = [&](Layer* overlay) {
          if (overlay == nullptr || !overlay->visible()) return;
          // Recorded only when it has no current list -- exactly the rule the scroll viewport uses. An
          // overlay that is recorded every frame would re-record whatever it hosts, which on the editor
          // scene is the page itself.
          if (overlay->borrowRecording().empty() || overlay->subtreeNeedsPaint()) {
            const Size size{ static_cast<float>(width), static_cast<float>(height) };
            overlay->adoptRecording(overlay->recordInto(size, uiScale));
          }
          if (overlay->borrowRecording().empty()) return;
          RenderContext::PlannedItem item;
          item.layer = overlay;
          item.translation = Point{ overlay->bounds().getLeft() * uiScale,
            overlay->bounds().getTop() * uiScale };
          plannedItems.push_back(item);
        };
        if (candidates != nullptr && candidates->visible()) {
          placeCandidates();
          collectOverlay(candidates.get());
        }
        if (userOverlay != nullptr) collectOverlay(userOverlay.get());
      } else {
        if (candidates != nullptr && candidates->visible()) {
          placeCandidates();
          candidates->draw(context);
        }
        if (userOverlay != nullptr) userOverlay->draw(context);
      }
      canvas->restore();
    }
    const auto paintEnd = std::chrono::steady_clock::now();

    const auto presentStart = std::chrono::steady_clock::now();
        bool presented = false;
    if (planMode && !plannedItems.empty()) {
      std::vector<graphics::gpu::Surface::ListItem> items;
      for (const RenderContext::PlannedItem& planned: plannedItems) {
        if (planned.list != nullptr) {
          // A container's own paint travels as its own list (the viewport's scrollbar, which must end up
          // above the content it lends).
          graphics::gpu::Surface::ListItem item;
          item.list = planned.list;
          item.translation = planned.translation;
          item.clip = planned.clip;
          items.push_back(std::move(item));
          continue;
        }
        // Otherwise the list is the layer's own: a composited container records its child elsewhere and
        // hands the result back through `Layer::adoptRecording`, so the borrow returns it unchanged.
        for (const std::shared_ptr<const graphics::gpu::DrawList>& list: planned.layer->borrowRecording()) {
          if (list == nullptr) continue;
          graphics::gpu::Surface::ListItem item;
          item.list = list;
          item.translation = planned.translation;
          item.clip = planned.clip;
          items.push_back(std::move(item));
        }
      }
      if (!items.empty() && platform->presentBoundaries(items)) {
        presented = true;
        lastFramePlanItems = static_cast<uint32_t>(items.size());
        lastFrameCompositeItems = static_cast<uint32_t>(plannedItems.size());
        lastFrameLentBytes = Layer::lentListBytes();
        lastFramePresentItems = lastFramePlanItems + 1u;

      } else {
        // The platform declined after the content was already recorded without the boundaries. Fold
        // them back into the canvas rather than presenting a frame with holes in it.
        Canvas foldCanvas = detail::makeCanvas(*canvas);
        for (const graphics::gpu::Surface::ListItem& item: items) {
          detail::nativeCanvas(foldCanvas).appendRecording(*item.list);
        }
      }
    }
    if (!presented) {
      // A frame that was not assembled from lists is one list on the device, and it lent nothing.
      if (!planMode) {
        lastFrameCompositeItems = 0;
        lastFrameLentBytes = 0;
        lastFramePresentItems = 1;
      }
  
    if (damaged && !planMode) {
        presented = platform->presentDamage(frameDamage);
        if (!presented) presented = platform->present();
      } else {
        presented = platform->present();
      }
    }
    clearFrameDamage();
    if (!presented) {      diagnostics = platform->diagnostics();
      running = false;
      return false;
    }
    const auto frameEnd = std::chrono::steady_clock::now();
    auto milliseconds = [](const std::chrono::steady_clock::duration duration) {
      return std::chrono::duration<double, std::milli>(duration).count();
    };
    timings.update = milliseconds(drawStart - updateStart);
    timings.layout = milliseconds(layoutEnd - layoutStart);
    timings.draw = milliseconds(presentStart - drawStart);
    timings.paint = milliseconds(paintEnd - paintStart);
    timings.present = milliseconds(frameEnd - presentStart);
    timings.total = milliseconds(frameEnd - frameStart);
    timings.valid = true;
    lastRepaintedBoundaries = Layer::repaintedBoundaries();
    lastCulledLayers = Layer::culledLayers();
    lastFrameListBytes = Layer::frameListBytes();
    lastFramePlanItemsShown = lastFramePlanItems;
    lastFrameCompositeItemsShown = lastFrameCompositeItems;
    lastFrameLentBytesShown = lastFrameLentBytes;
    lastFramePresentItemsShown = lastFramePresentItems;
    lastFramePlanItems = 0;
    lastFrameCompositeItems = 0;
    lastFrameLentBytes = 0;
    lastFramePresentItems = 0;
    lastDamageRatio = damaged ? (frameDamage.getWidth() * frameDamage.getHeight()) /
        std::max(1.0f, static_cast<float>(width) * static_cast<float>(height)) : 0.0;

    clock->onFramePresented(frameEnd);
    ++frameCount;
    lastFrameMilliseconds = timings.total;
    totalFrameMilliseconds += timings.total;
    notifyFrameCallbacks();
    return true;
  }
};

namespace {
/** -1 follows the environment, 0 forces off, 1 forces on (tests and the benchmark A/B need forcing). */
std::atomic<int> gFramePlanOverride{ -1 };
} // namespace

void Window::setFramePlanEnabled(const bool enabled) noexcept {
  gFramePlanOverride.store(enabled ? 1 : 0, std::memory_order_release);
}

int Window::framePlanOverride() noexcept {
  return gFramePlanOverride.load(std::memory_order_acquire);
}

void Window::setFramePlanOverride(const int value) noexcept {
  gFramePlanOverride.store(value, std::memory_order_release);
}

Window::FramePlanScope::FramePlanScope(const bool enabled) noexcept
  : mPrevious(Window::framePlanOverride()) {
  Window::setFramePlanOverride(enabled ? 1 : 0);
}

Window::FramePlanScope::~FramePlanScope() {
  Window::setFramePlanOverride(mPrevious);
}

bool Window::framePlanEnabled() noexcept {
  // On unless asked off. R11 turned the viewport's content into per-band lists: a frame now submits the
  // one or two bands it overlaps instead of the whole content, and a scrolled viewport no longer asks
  // for a layout, so the plan frame is both the cheapest and the most local shape. `CORAL_UI_FRAME_PLAN=0`
  // still restores the merged frame, and the window-platform path keeps its own opt-in
  // (`CORAL_UI_FRAME_PLAN_WINDOW`) until the three platforms have been smoke-tested.
  const int overrideValue = gFramePlanOverride.load(std::memory_order_acquire);
  if (overrideValue >= 0) return overrideValue != 0;
  static const bool initial = [] {
    const char* value = std::getenv("CORAL_UI_FRAME_PLAN");
    return value == nullptr || std::strcmp(value, "0") != 0;
  }();
  return initial;
}

Window::Window(std::unique_ptr<WindowImpl> impl) : mImpl(std::move(impl)) {
  if (mImpl == nullptr) return;
  mImpl->self = this;
  // A window starts with whatever the system says: high contrast, the light/dark preference and the
  // animation policy are all read here, and the platform reports a change when they move.
  applySystemAppearance(mImpl->platform != nullptr ? mImpl->platform->systemAppearance()
                                                   : SystemAppearance{});
  // Route this window's accessibility events to its own platform. The notifier used to be one
  // per-thread slot, so a thread with several windows sent every event to the newest window, carrying
  // a node path that belonged to another tree.
  coral::ui::setAccessibilityNotifier(this,
    [impl = mImpl.get()](Layer& layer, const AccessibilityEvent event) {
      if (impl->platform == nullptr || impl->root == nullptr) return;
      const std::string path = coral::ui::accessibilityPathOf(*impl->root, &layer);
      if (path.empty()) return;
      impl->platform->raiseAccessibilityPropertyEvent(path, event);
    });
}

Window::~Window() {
  if (mImpl == nullptr) return;
  if (mImpl->threadedHost != nullptr && !mImpl->ownsThreadedHost &&
      mImpl->renderWindowId != 0) {
    // The shared host outlives this window; detach the target and its callback before the native
    // window handle is destroyed.
    mImpl->threadedHost->removeTarget(mImpl->renderWindowId);
    mImpl->renderWindowId = 0;
  }
  if (mImpl->threadedHost != nullptr && mImpl->ownsThreadedHost) {
    // Stop the render thread while the native window still exists; the thread owns the surface that
    // presents into it. Destroying the platform window first leaves the render thread with a dead
    // HWND/wl_surface.
    mImpl->threadedHost->shutdown();
    mImpl->threadedHost.reset();
    mImpl->renderWindowId = 0;
  }
  mImpl->clock.reset();
  mImpl->vblank.reset();
  mImpl->platform.reset();
  mImpl->root.reset();
  // The notifier captures this window's impl; leaving it registered would let a control notify a
  // window that no longer exists.
  coral::ui::setAccessibilityNotifier(this, {});
}

namespace {
thread_local std::string gLastCreationError;
}

std::string Window::lastCreationError() { return gLastCreationError; }

std::unique_ptr<Window> Window::create(const Options& options) {
  gLastCreationError.clear();
  if (options.width <= 0 || options.height <= 0) {
    gLastCreationError = "the requested window size is empty";
    return nullptr;
  }

  auto impl = std::make_unique<WindowImpl>(options);
  impl->policy = options.framePolicy;
  std::string platformDiagnostics;
  backend::Device* sharedDevice = options.sharedDevice != nullptr
    ? static_cast<backend::Device*>(options.sharedDevice->nativeHandle())
    : nullptr;
  impl->platform = detail::createWindowPlatform(options, sharedDevice, &platformDiagnostics);
  if (impl->platform == nullptr) {
    impl->diagnostics = platformDiagnostics.empty() ? "no UI platform is available" : platformDiagnostics;
    gLastCreationError = impl->diagnostics;
    return nullptr;
  }

  impl->platform->setAccessibilityTreeProvider(
    [impl = impl.get()] {
      return coral::ui::accessibilityTree(*impl->root, impl->focused);
    },
    [impl = impl.get()](const std::string_view path, const AccessibilityAction action,
        const std::string_view value) {
      // UIA calls this on the client thread, so the action is queued as a window task: performing it
      // inline would mutate layers outside the frame and race the renderer.
      std::string pathCopy(path);
      std::string valueCopy(value);
      {
        std::lock_guard<std::mutex> lock(impl->taskMutex);
        impl->tasks.push_back([impl, pathCopy = std::move(pathCopy), valueCopy = std::move(valueCopy),
            action] {
          if (Layer* layer = coral::ui::accessibilityLayerAtPath(*impl->root, pathCopy)) {
            (void)layer->performAccessibilityAction(action, valueCopy);
          }
        });
      }
      impl->markFrameNeeded();
      return true;
    });
  impl->width = impl->platform->width();
  impl->height = impl->platform->height();
  impl->dpiScale = impl->platform->dpiScale();
  impl->uiScale = options.uiScale > 0.0f
                    ? options.uiScale
                    : (options.headless || !options.useDpiScale ? 1.0f : impl->dpiScale);
  if (options.renderHost == RenderHostKind::RenderThread) {
    if (options.sharedRenderHost != nullptr) {
      impl->threadedHost = options.sharedRenderHost;
      impl->ownsThreadedHost = false;
      impl->renderWindowId = impl->threadedHost->addTarget(impl->threadedOptions());
      if (impl->renderWindowId == 0) {
        impl->diagnostics = impl->threadedHost->diagnostics();
        gLastCreationError = impl->diagnostics.empty()
          ? "the shared render host rejected the window target" : impl->diagnostics;
        return nullptr;
      }
    } else {
      impl->threadedHost = std::make_shared<host::ThreadedRenderHost>(impl->threadedOptions());
      impl->renderWindowId = impl->threadedHost->primaryTarget();
      if (impl->renderWindowId == 0 || !impl->threadedHost->waitForReady()) {
        impl->diagnostics = impl->threadedHost->diagnostics();
        gLastCreationError = impl->diagnostics.empty()
          ? "the threaded render host could not start" : impl->diagnostics;
        return nullptr;
      }
    }
    impl->capabilities = std::make_shared<const RendererCapabilities>(
      impl->threadedHost->capabilitiesFor(impl->renderWindowId));
  } else {
    impl->capabilities = std::make_shared<const RendererCapabilities>(
      impl->platform->device() != nullptr
        ? queryRendererCapabilities(impl->platform->device(), true, impl->platform->sampleCount())
        : RendererCapabilities::raster());
  }

  if (impl->platform->headless()) {
    impl->clock = std::make_unique<graphics::gpu::ManualFrameClock>(
      std::chrono::steady_clock::now());
    impl->clock->setMode(graphics::gpu::FrameMode::OnDemand);
  } else {
    const graphics::gpu::NativeDisplay native = impl->platform->nativeDisplay();
    impl->vblank = graphics::gpu::makePlatformVBlankSource(native,
      options.renderHost != RenderHostKind::RenderThread);
    if (impl->vblank != nullptr) {
      impl->clock = std::make_unique<graphics::gpu::PlatformFrameClock>(*impl->vblank);
    } else {
      impl->clock = std::make_unique<graphics::gpu::PresentFrameClock>(
        std::chrono::steady_clock::now());
    }
    impl->clock->setMode(impl->policy == FramePolicy::Continuous
                           ? graphics::gpu::FrameMode::Continuous
                           : graphics::gpu::FrameMode::OnDemand);
  }
  if (impl->threadedHost != nullptr) {
    // The host calls this on the render thread; `onPacketPresented` only copies the completion and
    // posts a task, so it is safe for the render thread to run.
    impl->threadedHost->setPresentationCallback(impl->renderWindowId,
      [impl = impl.get()](const FrameCompletion& completion) {
        impl->onPacketPresented(completion);
      });
  }
  // A fresh window has never rendered, so the first frame is always needed.
  impl->markFrameNeeded();
  return std::unique_ptr<Window>(new Window(std::move(impl)));
}

void Window::setRoot(std::shared_ptr<Layer> root) {
  if (mImpl == nullptr) return;
  mImpl->setFocus(nullptr);
  mImpl->root = std::move(root);
  mImpl->hovered = nullptr;
  mImpl->pointerCapture = nullptr;
  if (mImpl->root != nullptr) {
    mImpl->root->setTheme(mImpl->theme);
    // The window's sheet has to reach the new tree, exactly like the theme does.
    mImpl->root->inheritStyleSheet(mImpl->styleSheet);
    mImpl->root->attachToWindow(this);
    // The overlay host is a child of the page root, so replacing the root orphans it: everything
    // pushed through `pushOverlay` would keep living in a tree nobody draws, and the next push would
    // add to that invisible host. Re-attach it instead of dropping live overlays.
    if (mImpl->overlays != nullptr) {
      mImpl->root->addChild(mImpl->overlays);
      mImpl->overlays->attachToWindow(this);
    }
  }
  mImpl->layoutRoot();
  mImpl->markFrameNeeded();
}

void Window::setOverlay(std::shared_ptr<Layer> overlay) {
  if (mImpl == nullptr) return;
  mImpl->setUserOverlay(std::move(overlay));
}

void Window::clearOverlay() {
  if (mImpl == nullptr) return;
  mImpl->setUserOverlay(nullptr);
}

const std::shared_ptr<Layer>& Window::overlay() const noexcept {
  static const std::shared_ptr<Layer> empty;
  if (mImpl == nullptr) return empty;
  return mImpl->userOverlay;
}

AccessibilityNode Window::accessibilityTree() const {
  if (mImpl == nullptr || mImpl->root == nullptr) return {};
  return coral::ui::accessibilityTree(*mImpl->root, mImpl->focused);
}

std::vector<AccessibilityNode> Window::accessibilityFocusNodes() const {
  const AccessibilityNode tree = accessibilityTree();
  const std::vector<const AccessibilityNode*> order = accessibilityFocusOrder(tree);
  std::vector<AccessibilityNode> result;
  result.reserve(order.size());
  for (const AccessibilityNode* node: order) {
    if (node != nullptr) result.push_back(*node);
  }
  return result;
}

const std::shared_ptr<Layer>& Window::root() const noexcept {
  static const std::shared_ptr<Layer> empty;
  return mImpl != nullptr ? mImpl->root : empty;
}

Layer* Window::focusedLayer() const noexcept {
  return mImpl != nullptr ? mImpl->focused : nullptr;
}

Layer* Window::activeFocusScope() const noexcept {
  if (mImpl == nullptr || mImpl->root == nullptr) return nullptr;
  return mImpl->topmostFocusScope(mImpl->root.get());
}

int Window::width() const noexcept {
  return mImpl != nullptr ? mImpl->width : 0;
}

int Window::height() const noexcept {
  return mImpl != nullptr ? mImpl->height : 0;
}

void Window::setTitle(std::string title) {
  if (mImpl == nullptr) return;
  mImpl->windowTitle = std::move(title);
  if (mImpl->platform != nullptr) mImpl->platform->setTitle(mImpl->windowTitle);
}

const std::string& Window::title() const noexcept {
  static const std::string empty;
  return mImpl != nullptr ? mImpl->windowTitle : empty;
}


DragEffect Window::beginPlatformDrag(const DragPayload& payload, const DragEffect allowed) {
  if (mImpl == nullptr || mImpl->platform == nullptr || mImpl->platform->headless()) {
    return DragEffect::None;
  }
  return mImpl->platform->beginDragOut(payload, allowed);
}
void Window::setPosition(const int x, const int y) {
  if (mImpl == nullptr) return;
  if (mImpl->platform != nullptr) mImpl->platform->setPosition(x, y);
}

void Window::setMinimumSize(const int width, const int height) {
  if (mImpl == nullptr) return;
  mImpl->minWidth = std::max(0, width);
  mImpl->minHeight = std::max(0, height);
  if (mImpl->platform != nullptr) mImpl->platform->setMinimumSize(mImpl->minWidth, mImpl->minHeight);
}

int Window::minimumWidth() const noexcept {
  return mImpl != nullptr ? mImpl->minWidth : 0;
}

int Window::minimumHeight() const noexcept {
  return mImpl != nullptr ? mImpl->minHeight : 0;
}

void Window::setDecorations(const Decorations decorations) {
  if (mImpl == nullptr || mImpl->decorations == decorations) return;
  mImpl->decorations = decorations;
  if (mImpl->platform != nullptr) mImpl->platform->setDecorations(decorations);
  mImpl->damageAll();
  mImpl->markFrameNeeded();
}

Window::Decorations Window::decorations() const noexcept {
  return mImpl != nullptr ? mImpl->decorations : Decorations::System;
}

void Window::setChromeMetrics(WindowChromeMetrics metrics) {
  if (mImpl == nullptr) return;
  mImpl->chromeMetrics = metrics;
  if (mImpl->decorations == Decorations::Client) {
    mImpl->damageAll();
    mImpl->markFrameNeeded();
  }
}

const WindowChromeMetrics& Window::chromeMetrics() const noexcept {
  static const WindowChromeMetrics defaults;
  return mImpl != nullptr ? mImpl->chromeMetrics : defaults;
}

WindowHit Window::hitTest(const Point point) const {
  if (mImpl == nullptr) return WindowHit::None;
  const float scale = mImpl->uiScale > 0.0f ? mImpl->uiScale : 1.0f;
  const float logicalWidth = static_cast<float>(mImpl->width) / scale;
  const float logicalHeight = static_cast<float>(mImpl->height) / scale;
  if (mImpl->decorations == Decorations::System) {
    return point.x >= 0.0f && point.y >= 0.0f && point.x < logicalWidth &&
             point.y < logicalHeight
             ? WindowHit::Client
             : WindowHit::None;
  }
  return hitTestWindowChrome(Size{ logicalWidth, logicalHeight }, point, mImpl->chromeMetrics);
}
void Window::setCursor(const Cursor cursor) {
  if (mImpl == nullptr || mImpl->cursor == cursor) return;
  mImpl->cursor = cursor;
  if (mImpl->platform != nullptr) mImpl->platform->setCursor(cursor);
}

Window::Cursor Window::cursor() const noexcept {
  return mImpl != nullptr ? mImpl->cursor : Cursor::Arrow;
}

void Window::maximize() {
  if (mImpl == nullptr || mImpl->fullscreen || mImpl->maximized) return;
  mImpl->maximized = true;
  mImpl->minimized = false;
  if (mImpl->platform != nullptr) mImpl->platform->setMaximized(true);
  mImpl->damageAll();
  mImpl->markFrameNeeded();
}

void Window::restore() {
  if (mImpl == nullptr) return;
  const bool wasMaximized = mImpl->maximized;
  const bool wasMinimized = mImpl->minimized;
  mImpl->maximized = false;
  mImpl->minimized = false;
  if (mImpl->platform != nullptr) {
    if (wasMaximized) mImpl->platform->setMaximized(false);
    if (wasMinimized) mImpl->platform->setMinimized(false);
  }
  if (wasMaximized || wasMinimized) {
    mImpl->damageAll();
    mImpl->markFrameNeeded();
  }
}

void Window::minimize() {
  if (mImpl == nullptr || mImpl->minimized) return;
  mImpl->minimized = true;
  if (mImpl->platform != nullptr) mImpl->platform->setMinimized(true);
  mImpl->markFrameNeeded();
}

bool Window::isMaximized() const noexcept {
  return mImpl != nullptr && mImpl->maximized;
}

bool Window::isMinimized() const noexcept {
  return mImpl != nullptr && mImpl->minimized;
}

void Window::setOpacity(const float opacity) {
  if (mImpl == nullptr) return;
  const float clamped = std::clamp(opacity, 0.0f, 1.0f);
  if (mImpl->opacity == clamped) return;
  mImpl->opacity = clamped;
  if (mImpl->platform != nullptr) mImpl->platform->setOpacity(clamped);
  mImpl->markFrameNeeded();
}

float Window::opacity() const noexcept {
  return mImpl != nullptr ? mImpl->opacity : 1.0f;
}

void Window::setAlwaysOnTop(const bool onTop) {
  if (mImpl == nullptr || mImpl->alwaysOnTop == onTop) return;
  mImpl->alwaysOnTop = onTop;
  if (mImpl->platform != nullptr) mImpl->platform->setAlwaysOnTop(onTop);
}

bool Window::isAlwaysOnTop() const noexcept {
  return mImpl != nullptr && mImpl->alwaysOnTop;
}

void Window::setTransientFor(Window* parent) {
  if (mImpl == nullptr) return;
  mImpl->transientParent = parent;
  if (mImpl->platform != nullptr) {
    mImpl->platform->setTransientFor(parent != nullptr && parent->mImpl != nullptr
      ? parent->mImpl->platform.get() : nullptr);
  }
}

Window* Window::transientFor() const noexcept {
  return mImpl != nullptr ? mImpl->transientParent : nullptr;
}

void Window::setModal(const bool modal) {
  if (mImpl == nullptr) return;
  mImpl->modal = modal;
  if (mImpl->platform != nullptr) mImpl->platform->setModal(modal);
}

bool Window::isModal() const noexcept {
  return mImpl != nullptr && mImpl->modal;
}
void Window::setFullscreen(const bool fullscreen) {
  if (mImpl == nullptr || mImpl->fullscreen == fullscreen) return;
  mImpl->fullscreen = fullscreen;
  if (mImpl->platform != nullptr) mImpl->platform->setFullscreen(fullscreen);
  mImpl->damageAll();
  mImpl->markFrameNeeded();
}

bool Window::isFullscreen() const noexcept {
  return mImpl != nullptr && mImpl->fullscreen;
}

Rect Window::bounds() const {
  if (mImpl == nullptr || mImpl->platform == nullptr) return Rect{};
  return mImpl->platform->screenBounds();
}

void Window::setCloseRequestedCallback(std::function<bool()> callback) {
  if (mImpl != nullptr) mImpl->closeRequested = std::move(callback);
}

void Window::requestClose() {
  if (mImpl == nullptr) return;
  mImpl->requestClose();
  if (mImpl->platform != nullptr) mImpl->platform->wake();
}

void Window::restoreGeometry(const Rect screenBounds) {
  if (mImpl == nullptr) return;
  setPosition(static_cast<int>(screenBounds.getLeft()), static_cast<int>(screenBounds.getTop()));
  const int width = static_cast<int>(screenBounds.getWidth());
  const int height = static_cast<int>(screenBounds.getHeight());
  if (width > 0 && height > 0) resize(width, height);
}

void Window::raise() {
  if (mImpl == nullptr || mImpl->platform == nullptr) return;
  mImpl->platform->raise();
  mImpl->markFrameNeeded();
}

bool Window::pump() {
  if (mImpl == nullptr || !mImpl->running || mImpl->platform == nullptr) return false;
  std::vector<std::function<void()>> tasks;
  {
    std::lock_guard<std::mutex> lock(mImpl->taskMutex);
    tasks.swap(mImpl->tasks);
  }
  for (const std::function<void()>& task: tasks) {
    if (task) task();
  }
  const bool alive = mImpl->platform->pump(*mImpl);
  if (alive) {
    mImpl->platform->updateImeCursorRect(mImpl->imeCursorRect());
  }
  if (mImpl->options.headless && mImpl->options.frameLimit > 0 &&
      mImpl->frameCount >= mImpl->options.frameLimit) {
    mImpl->running = false;
  }
  return alive && mImpl->running;
}

bool Window::renderFrame() {
  return mImpl != nullptr && mImpl->renderFrame();
}

int Window::run() {
  while (mImpl != nullptr && mImpl->running) {
    if (!tick()) break;
  }
  return mImpl != nullptr ? mImpl->frameCount : 0;
}

bool Window::tick(const bool blockOnIdle) {
  if (mImpl == nullptr || !mImpl->running) return false;
  if (!pump()) return false;


  const bool animating = mImpl->root != nullptr && mImpl->root->hasRunningAnimation();
  // `frameLimit` is a run-loop bound, not an input-driven request: a headless/native demo asking for
  // N frames must produce N without an external `requestFrame` per frame. Production windows leave
  // the limit at 0, so OnDemand still renders only when something asks.
  const bool limitPending = mImpl->options.frameLimit > 0 &&
    mImpl->frameCount < mImpl->options.frameLimit;
  // An occluded window keeps its pending request (do not exchange it away): the first visible tick
  // then renders, while the idle loop can stay parked without a full repaint per animation tick.
  bool wanted = mImpl->policy == FramePolicy::Continuous || animating || limitPending;
  if (!wanted && !mImpl->occluded) {
    wanted = mImpl->frameWanted.exchange(false, std::memory_order_acq_rel);
  }
  if (wanted && !mImpl->occluded) {
    if (!mImpl->renderFrame()) {
      if (mImpl->recoveryCount < 1 && recoverDevice()) return true;
      return false;
    }
  } else if (mImpl->platform != nullptr) {
    // `run` waits indefinitely on the display/self-pipe; `Application` polls each window instead.
    mImpl->platform->waitForEvents(blockOnIdle ? -1 : 0);
  }
  if (mImpl->options.frameLimit > 0 && mImpl->frameCount >= mImpl->options.frameLimit) {
    mImpl->running = false;
  }
  return mImpl->running;
}

bool Window::running() const noexcept {
  return mImpl != nullptr && mImpl->running;
}

void Window::requestFrame() {
  if (mImpl == nullptr) return;
  mImpl->markFrameNeeded();
  if (mImpl->platform != nullptr) mImpl->platform->wake();
}

bool Window::frameRequested() const noexcept {
  return mImpl != nullptr && mImpl->frameWanted.load(std::memory_order_acquire);
}

void Window::invalidate(const Rect& deviceRect) {
  if (mImpl == nullptr || deviceRect.getWidth() <= 0.0f || deviceRect.getHeight() <= 0.0f) return;
  if (!mImpl->hasDamage) {
    mImpl->damage = deviceRect;
    mImpl->hasDamage = true;
  } else {
    mImpl->damage = Rect::MakeLTRB(std::min(mImpl->damage.getLeft(), deviceRect.getLeft()),
      std::min(mImpl->damage.getTop(), deviceRect.getTop()),
      std::max(mImpl->damage.getRight(), deviceRect.getRight()),
      std::max(mImpl->damage.getBottom(), deviceRect.getBottom()));
  }
  mImpl->hasDamage = true;
  requestFrame();
}

Rect Window::damage() const noexcept {
  return mImpl != nullptr && mImpl->hasDamage ? mImpl->damage : Rect{};
}

void Window::clearDamage() noexcept {
  if (mImpl == nullptr) return;
  mImpl->damage = Rect{};
  mImpl->hasDamage = false;
}

void Window::setFramePolicy(const FramePolicy policy) {
  if (mImpl == nullptr) return;
  mImpl->policy = policy;
  if (mImpl->clock != nullptr) {
    mImpl->clock->setMode(policy == FramePolicy::Continuous
                            ? graphics::gpu::FrameMode::Continuous
                            : graphics::gpu::FrameMode::OnDemand);
  }
}

FramePolicy Window::framePolicy() const noexcept {
  return mImpl != nullptr ? mImpl->policy : FramePolicy::Continuous;
}

void Window::quit() {
  if (mImpl != nullptr) {
    mImpl->running = false;
    if (mImpl->platform != nullptr) mImpl->platform->wake();
  }
}

FrameTimings Window::lastFrameTimings() const noexcept {
  return mImpl != nullptr ? mImpl->timings : FrameTimings{};
}

FrameStatistics Window::frameStatistics() const noexcept {
  FrameStatistics stats;
  if (mImpl == nullptr) return stats;
  stats.frames = static_cast<uint64_t>(mImpl->frameCount);
  stats.recoveries = static_cast<uint32_t>(mImpl->recoveryCount);
  stats.lastTotalMilliseconds = mImpl->lastFrameMilliseconds;
  stats.averageTotalMilliseconds = mImpl->frameCount > 0
    ? mImpl->totalFrameMilliseconds / mImpl->frameCount : 0.0;
  stats.lastPresentMilliseconds = mImpl->timings.present;
  stats.dirtyNodes = mImpl->lastDirtyNodes;
  stats.layoutDirtyNodes = mImpl->lastLayoutDirtyNodes;
  stats.damagedAreaRatio = mImpl->lastDamageRatio;
  stats.repaintedBoundaries = mImpl->lastRepaintedBoundaries;
  stats.culledLayers = mImpl->lastCulledLayers;
  stats.frameListBytes = mImpl->lastFrameListBytes;
  stats.framePlanItems = mImpl->lastFramePlanItemsShown;
  stats.compositeItems = mImpl->lastFrameCompositeItemsShown;
  stats.lentListBytes = mImpl->lastFrameLentBytesShown;
  stats.presentItems = mImpl->lastFramePresentItemsShown;
  stats.commandBuffers = mImpl->platform != nullptr ? mImpl->platform->lastFrameCommandBuffers() : 0;
  stats.drawCalls = mImpl->platform != nullptr ? mImpl->platform->lastFrameDrawCalls() : 0;
  stats.batches = mImpl->platform != nullptr ? mImpl->platform->lastFrameBatches() : 0;
  if (mImpl->platform != nullptr) {
    const graphics::gpu::FrameStages stages = mImpl->platform->lastFrameStages();
    stats.renderStages = { stages.fields, stages.placeImages, stages.precompile, stages.targets,
      stages.arena, stages.vertexUpload, stages.record, stages.execute, stages.total,
      stages.rect, stages.gradient, stages.image, stages.path, stages.field, stages.composite };
  }
  if (mImpl->platform != nullptr) {
    const detail::WindowPlatform::FrameRenderTiming render = mImpl->platform->lastFrameRenderTiming();
    stats.lastRenderMilliseconds = render.record;
    stats.lastExecuteMilliseconds = render.execute;
    stats.lastGpuMilliseconds = render.gpu;
    stats.residentBytes = render.residentBytes;
    stats.hdrOutput = mImpl->platform->outputIsHdr();
  }
  stats.hostKind = mImpl->threadedHost != nullptr ? RenderHostKind::RenderThread
                                                 : RenderHostKind::SameThread;
  if (mImpl->clock != nullptr) {
    const graphics::gpu::RefreshInfo info = mImpl->clock->refresh();
    stats.refreshHertz = info.valid ? info.hertz : 0.0;
  }
  if (mImpl->threadedHost != nullptr) {
    // Queue depth and drops are what the latest-wins packet queue can report: one pending packet
    // per target, and the count of packets replaced before the render thread reached them.
    const host::ThreadedRenderHost::TargetStats target =
      mImpl->threadedHost->targetStats(mImpl->renderWindowId);
    stats.queueDepth = target.queued;
    stats.droppedFrames = target.dropped;
    stats.failedFrames = target.failed;
  }
  if (mImpl->root != nullptr) {
    // O(tree) per call; a diagnostics overlay samples this once per frame, which is cheap next to
    // recording and drawing the same tree.
    std::vector<const Layer*> pending{ mImpl->root.get() };
    while (!pending.empty()) {
      const Layer* node = pending.back();
      pending.pop_back();
      ++stats.layers;
      if (node->visible()) ++stats.visibleLayers;
      for (const std::shared_ptr<Layer>& child: node->children()) {
        if (child != nullptr) pending.push_back(child.get());
      }
    }
  }
  return stats;
}

bool Window::snapshot(graphics::Bitmap* out) {
  if (mImpl == nullptr || out == nullptr) return false;
  if (mImpl->threadedHost != nullptr) return mImpl->threadedHost->snapshot(mImpl->renderWindowId, out);
  return mImpl->platform != nullptr && mImpl->platform->snapshot(out);
}

bool Window::resize(const int width, const int height) {
  if (mImpl == nullptr || mImpl->platform == nullptr || width <= 0 || height <= 0) return false;
  if (!mImpl->platform->resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height))) {
    mImpl->diagnostics = mImpl->platform->diagnostics();
    return false;
  }
  mImpl->width = mImpl->platform->width();
  mImpl->height = mImpl->platform->height();
  if (mImpl->threadedHost != nullptr) {
    mImpl->threadedHost->resize(mImpl->renderWindowId, static_cast<uint32_t>(mImpl->width),
      static_cast<uint32_t>(mImpl->height));
  }
  mImpl->layoutRoot();
  mImpl->damageAll();
  mImpl->markFrameNeeded();
  return true;
}

float Window::dpiScale() const noexcept {
  return mImpl != nullptr && mImpl->platform != nullptr ? mImpl->platform->dpiScale() : 1.0f;
}

std::string Window::portalParentHandle() const {
  if (mImpl == nullptr || mImpl->platform == nullptr) return {};
  return mImpl->platform->portalParentHandle();
}

void* Window::nativeHandle() const noexcept {
  return mImpl != nullptr && mImpl->platform != nullptr ? mImpl->platform->nativeWindow() : nullptr;
}

float Window::uiScale() const noexcept {
  return mImpl != nullptr ? mImpl->uiScale : 1.0f;
}

float Window::devicePixelRatio() const noexcept {
  return uiScale();
}

Window::NativePlatform Window::nativePlatform() const noexcept {
  return mImpl != nullptr && mImpl->platform != nullptr
           ? mImpl->platform->kind()
           : NativePlatform::Auto;
}

std::shared_ptr<RendererDevice> Window::rendererDevice() const {
  if (mImpl == nullptr || mImpl->platform == nullptr) return nullptr;
  if (mImpl->deviceHandle == nullptr) {
    mImpl->deviceHandle = RendererDevice::borrow(mImpl->platform->device());
  }
  return mImpl->deviceHandle;
}

std::shared_ptr<RendererDevice> Window::releaseRendererDevice() {
  if (mImpl == nullptr || mImpl->platform == nullptr) return nullptr;
  std::shared_ptr<RendererDevice> released = RendererDevice::own(mImpl->platform->releaseDevice());
  if (released != nullptr) mImpl->deviceHandle = released;
  return released;
}

const RendererCapabilities& Window::rendererCapabilities() const noexcept {
  static const RendererCapabilities raster = RendererCapabilities::raster();
  return mImpl != nullptr && mImpl->capabilities != nullptr ? *mImpl->capabilities : raster;
}

RendererKind Window::rendererKind() const noexcept {
  if (mImpl == nullptr) return RendererKind::Unknown;
  return rendererCapabilities().backend == "software" ? RendererKind::Raster : RendererKind::GPU;
}

const RendererCapabilities& Window::capabilities() const noexcept {
  return rendererCapabilities();
}

bool Window::recover() {
  return recoverDevice();
}

bool Window::recoverFromSharedHost() {
  if (mImpl == nullptr) return false;
  return mImpl->recoverFromSharedHost();
}

bool Window::recoverDevice() {
  if (mImpl == nullptr) return false;
  return mImpl->recoverDevice();
}

std::string Window::diagnostics() const {
  if (mImpl == nullptr) return {};
  if (!mImpl->diagnostics.empty()) return mImpl->diagnostics;
  return mImpl->platform != nullptr ? mImpl->platform->diagnostics() : std::string();
}

bool Window::isRenderTargetOnScreen() const {
  if (mImpl == nullptr) return false;
  if (mImpl->threadedHost != nullptr) {
    return mImpl->threadedHost->targetIsWindow(mImpl->renderWindowId);
  }
  return mImpl->platform != nullptr && mImpl->platform->nativeWindow() != nullptr;
}

double Window::refreshHertz() const noexcept {
  if (mImpl == nullptr || mImpl->clock == nullptr) return 0.0;
  const graphics::gpu::RefreshInfo info = mImpl->clock->refresh();
  return info.valid ? info.hertz : 0.0;
}

double Window::lastFrameMilliseconds() const noexcept {
  return mImpl != nullptr ? mImpl->lastFrameMilliseconds : 0.0;
}

int Window::frameCount() const noexcept {
  return mImpl != nullptr ? mImpl->frameCount : 0;
}

void Window::setFrameCallback(std::function<void()> callback) {
  if (mImpl != nullptr) mImpl->frameCallback = std::move(callback);
}

uint64_t Window::addFrameCallback(std::function<void()> callback) {
  if (mImpl == nullptr || !callback) return 0;
  const uint64_t id = mImpl->nextFrameCallbackId++;
  mImpl->frameCallbacks.emplace(id, std::move(callback));
  return id;
}

void Window::removeFrameCallback(const uint64_t id) {
  if (mImpl == nullptr || id == 0) return;
  mImpl->frameCallbacks.erase(id);
}

int Window::recoveryCount() const noexcept {
  return mImpl != nullptr ? mImpl->recoveryCount : 0;
}

void Window::focusLayer(Layer* layer) {
  if (mImpl != nullptr) mImpl->setFocus(layer);
}

CommandRegistry& Window::commands() {
  static CommandRegistry empty;
  return mImpl != nullptr ? mImpl->commands : empty;
}

void Window::post(std::function<void()> task) {
  if (mImpl == nullptr || !task) return;
  {
    std::lock_guard<std::mutex> lock(mImpl->taskMutex);
    mImpl->tasks.push_back(std::move(task));
  }
  // A posted task usually changes the tree. The UI may be idle in `OnDemand` mode, so a task is a
  // frame request as well as a wake-up.
  mImpl->markFrameNeeded();
  if (mImpl->platform != nullptr) mImpl->platform->wake();
}

bool Window::pointerDown(const PointerEvent& event) {
  if (mImpl == nullptr) return false;
  mImpl->dispatchPointer(event, 1);
  return true;
}

bool Window::pointerUp(const PointerEvent& event) {
  if (mImpl == nullptr) return false;
  mImpl->dispatchPointer(event, 2);
  return true;
}

bool Window::pointerMove(const PointerEvent& event) {
  if (mImpl == nullptr) return false;
  mImpl->dispatchPointer(event, 0);
  return true;
}

void Window::setPointerCapture(const std::shared_ptr<Layer>& layer, const std::uint32_t pointerId) {
  if (mImpl == nullptr) return;
  mImpl->publicPointerCapture = layer;
  mImpl->publicPointerCaptureId = pointerId;
  if (mImpl->platform != nullptr) mImpl->platform->setPointerCapture(layer != nullptr);
}

void Window::releasePointerCapture() {
  if (mImpl == nullptr) return;
  mImpl->publicPointerCapture.reset();
  mImpl->publicPointerCaptureId = 0;
  if (mImpl->platform != nullptr) mImpl->platform->setPointerCapture(false);
}

Layer* Window::pointerCapture() const noexcept {
  return mImpl != nullptr ? mImpl->publicPointerCapture.lock().get() : nullptr;
}

std::uint32_t Window::pointerCaptureId() const noexcept {
  return mImpl != nullptr ? mImpl->publicPointerCaptureId : 0;
}

bool Window::keyDown(const KeyEvent& event) {
  if (mImpl == nullptr) return false;
  mImpl->dispatchKey(event, true);
  return true;
}

bool Window::keyUp(const KeyEvent& event) {
  if (mImpl == nullptr) return false;
  mImpl->dispatchKey(event, false);
  return true;
}

bool Window::textInput(const char32_t character) {
  if (mImpl == nullptr) return false;
  mImpl->dispatchText(character);
  return true;
}

bool Window::textComposition(const std::string& text) {
  if (mImpl == nullptr) return false;
  mImpl->dispatchComposition(text);
  return true;
}

bool Window::textCommit(const std::string& text) {
  if (mImpl == nullptr) return false;
  mImpl->dispatchCommit(text);
  return true;
}

void Window::setImeCandidates(std::vector<std::string> candidates, const size_t selected) {
  if (mImpl == nullptr) return;
  mImpl->showCandidates(std::move(candidates), selected);
}

void Window::clearImeCandidates() {
  if (mImpl == nullptr || mImpl->candidates == nullptr) return;
  mImpl->candidates->setCandidates({});
  mImpl->candidates->setVisible(false);
  mImpl->markFrameNeeded();
}

bool Window::imeCandidatesVisible() const noexcept {
  return mImpl != nullptr && mImpl->candidates != nullptr && mImpl->candidates->visible();
}

size_t Window::imeCandidateCount() const noexcept {
  return mImpl != nullptr && mImpl->candidates != nullptr ? mImpl->candidates->candidates().size() : 0;
}

size_t Window::imeCandidateSelected() const noexcept {
  return mImpl != nullptr && mImpl->candidates != nullptr ? mImpl->candidates->selected() : 0;
}

bool Window::scroll(const ScrollEvent& event) {
  if (mImpl == nullptr) return false;
  mImpl->dispatchScroll(event);
  return true;
}

bool Window::gesture(const GestureEvent& event) {
  if (mImpl == nullptr) return false;
  return mImpl->dispatchGesture(event);
}

void Window::drop(const DropEvent& event) {
  if (mImpl != nullptr) mImpl->dispatchDrop(event);
}

uint64_t Window::pushOverlay(std::shared_ptr<Layer> layer, OverlayOptions options) {
  if (mImpl == nullptr) return 0;
  return mImpl->pushOverlay(std::move(layer), std::move(options));
}

bool Window::dismissOverlay(const uint64_t id) {
  return mImpl != nullptr && mImpl->dismissOverlay(id);
}

bool Window::dismissTopOverlay() {
  return mImpl != nullptr && mImpl->dismissTopOverlay();
}

void Window::dismissAllOverlays() {
  if (mImpl != nullptr) mImpl->dismissAllOverlays();
}

bool Window::hasOverlay() const noexcept {
  return mImpl != nullptr && mImpl->hasOverlay();
}

UndoStack& Window::undoStack() {
  static UndoStack unavailable;
  return mImpl != nullptr ? mImpl->undoStack : unavailable;
}

const UndoStack& Window::undoStack() const {
  return const_cast<Window*>(this)->undoStack();
}

bool Window::beginDrag(DragPayload payload, std::shared_ptr<Layer> preview) {
  return mImpl != nullptr && mImpl->beginDrag(std::move(payload), std::move(preview));
}

DragEffect Window::dragHover(const DragPayload& payload, const Point position) {
  if (mImpl == nullptr || mImpl->root == nullptr) return DragEffect::None;
  DragEvent event;
  event.position = position;
  event.payload = payload;
  Layer* hit = mImpl->root->hitTest(position);
  Layer* acceptor = nullptr;
  DragEffect effect = DragEffect::None;
  for (Layer* layer = hit; layer != nullptr; layer = layer->parent()) {
    const DragEffect candidate = layer == mImpl->hoverDragTarget ? layer->onDragOver(event)
                                                                 : layer->onDragEnter(event);
    if (candidate != DragEffect::None) {
      acceptor = layer;
      effect = candidate;
      break;
    }
  }
  if (mImpl->hoverDragTarget != nullptr && mImpl->hoverDragTarget != acceptor) {
    mImpl->hoverDragTarget->onDragLeave();
  }
  mImpl->hoverDragTarget = acceptor;
  mImpl->hoverDragEffect = effect;
  mImpl->markFrameNeeded();
  return effect;
}

void Window::dragLeave() {
  if (mImpl == nullptr || mImpl->hoverDragTarget == nullptr) return;
  mImpl->hoverDragTarget->onDragLeave();
  mImpl->hoverDragTarget = nullptr;
  mImpl->hoverDragEffect = DragEffect::None;
  mImpl->markFrameNeeded();
}

bool Window::dragDrop(const DragPayload& payload, const Point position) {
  if (mImpl == nullptr || mImpl->root == nullptr) return false;
  DropEvent drop;
  drop.position = position;
  drop.payload = payload;
  drop.effect = mImpl->hoverDragEffect;
  bool accepted = false;
  Layer* hit = mImpl->root->hitTest(position);
  for (Layer* layer = hit; layer != nullptr; layer = layer->parent()) {
    if (layer->onDrop(drop)) {
      accepted = true;
      break;
    }
    if (layer == mImpl->hoverDragTarget) break;
  }
  dragLeave();
  return accepted;
}
void Window::cancelDrag() {
  if (mImpl != nullptr) mImpl->cancelDrag();
}

bool Window::isDragging() const noexcept {
  return mImpl != nullptr && mImpl->isDragging();
}

const DragPayload* Window::dragPayload() const noexcept {
  return mImpl != nullptr ? mImpl->currentDragPayload() : nullptr;
}

void Window::setThemeMode(const ThemeMode mode) {
  if (mImpl == nullptr || mImpl->themeMode == mode) return;
  mImpl->themeMode = mode;
  const bool dark = mode == ThemeMode::Dark ||
    (mode == ThemeMode::System && mImpl->platform != nullptr &&
      mImpl->platform->prefersDarkAppearance());
  setTheme(dark ? Theme::dark() : Theme::light());
}

void Window::setStyleSheet(std::shared_ptr<const StyleSheet> sheet) {
  if (mImpl == nullptr) return;
  mImpl->styleSheet = std::move(sheet);
  if (mImpl->root != nullptr) {
    mImpl->root->setStyleSheet(mImpl->styleSheet);
  }
  mImpl->markFrameNeeded();
}

std::shared_ptr<const StyleSheet> Window::styleSheet() const noexcept {
  return mImpl != nullptr ? mImpl->styleSheet : nullptr;
}

void Window::refreshThemeMode() {
  if (mImpl == nullptr || mImpl->themeMode != ThemeMode::System) return;
  const bool dark = mImpl->platform != nullptr && mImpl->platform->prefersDarkAppearance();
  auto next = std::make_shared<const Theme>(dark ? Theme::dark() : Theme::light());
  // While high contrast is on, the resolved light/dark theme is what the app gets back when the mode
  // turns off; applying it now would replace the system palette.
  if (mImpl->highContrast) {
    mImpl->baseTheme = std::move(next);
    return;
  }
  applyResolvedTheme(std::move(next));
}

void Window::applySystemAppearance(const SystemAppearance& appearance) {
  if (mImpl == nullptr) return;
  mImpl->systemAppearance = appearance;
  // The policy is process-wide because animations start from layers that do not know their window.
  setReduceMotion(appearance.reduceMotion);
  const bool wantHighContrast = appearance.highContrast && isUsable(appearance.colors);
  if (wantHighContrast) {
    if (!mImpl->highContrast) {
      if (mImpl->theme != nullptr) mImpl->baseTheme = mImpl->theme;
      mImpl->highContrast = true;
    }
    applyResolvedTheme(std::make_shared<const Theme>(highContrastTheme(appearance.colors)));
    return;
  }
  if (!mImpl->highContrast) return;
  mImpl->highContrast = false;
  if (mImpl->baseTheme != nullptr) {
    applyResolvedTheme(std::move(mImpl->baseTheme));
    mImpl->baseTheme.reset();
  } else {
    refreshThemeMode();
  }
}

bool Window::highContrast() const noexcept {
  return mImpl != nullptr && mImpl->highContrast;
}

SystemAppearance Window::systemAppearance() const {
  return mImpl != nullptr ? mImpl->systemAppearance : SystemAppearance{};
}

Window::ThemeMode Window::themeMode() const noexcept {
  return mImpl != nullptr ? mImpl->themeMode : ThemeMode::Dark;
}

void Window::applyResolvedTheme(std::shared_ptr<const Theme> theme) {
  if (mImpl == nullptr || theme == nullptr) return;
  auto next = std::move(theme);
  // A window that still uses the theme background should follow it; an application that supplied a
  // custom clear colour keeps that colour across theme switches.
  if (mImpl->theme != nullptr && mImpl->options.clearColor == mImpl->theme->background) {
    mImpl->options.clearColor = next->background;
  }
  mImpl->theme = std::move(next);
  if (mImpl->root != nullptr) {
    mImpl->root->setTheme(mImpl->theme);
    // The window's sheet has to reach the new tree, exactly like the theme does.
    mImpl->root->inheritStyleSheet(mImpl->styleSheet);
    mImpl->root->attachToWindow(this);
  }
  mImpl->markFrameNeeded();
}

void Window::setTheme(Theme theme) {
  if (mImpl == nullptr) return;
  auto next = std::make_shared<const Theme>(std::move(theme));
  // While high contrast is on, what the application installs is remembered rather than applied: the
  // system palette wins until the mode turns off, and the app then gets the theme it asked for.
  if (mImpl->highContrast) {
    mImpl->baseTheme = std::move(next);
    return;
  }
  applyResolvedTheme(std::move(next));
}

void Window::setClearColor(const Color color) {
  if (mImpl == nullptr) return;
  mImpl->options.clearColor = color;
  mImpl->damageAll();
  mImpl->markFrameNeeded();
}

Color Window::clearColor() const noexcept {
  return mImpl != nullptr ? mImpl->options.clearColor : Color{};
}

const Theme& Window::theme() const noexcept {
  static const Theme fallback = Theme::dark();
  return mImpl != nullptr && mImpl->theme != nullptr ? *mImpl->theme : fallback;
}

} // namespace coral::ui


