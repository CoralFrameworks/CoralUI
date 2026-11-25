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

#ifndef CORAL_UI_WINDOWPLATFORM_H
#define CORAL_UI_WINDOWPLATFORM_H

#include <gpu/Surface.h>
#include <ui/Accessibility.h>
#include <ui/Window.h>

#include <backend/Types.h>
#include <graphics/Bitmap.h>
#include <gpu/FrameClock.h>

#include <chrono>
#include <functional>
#include <vector>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace backend {
class Device;
}

namespace coral::ui::detail {

#if defined(__linux__)
/** Drops process-wide Wayland/EGL state after the last window and device are gone. */
void shutdownPlatformDisplay();
#endif

/** The internal backend enum for a public API preference; `Auto` means "let the platform choose". */
inline backend::Backend toBackend(const RendererApi api) {
  switch (api) {
    case RendererApi::OpenGL: return backend::Backend::OpenGL;
    case RendererApi::Vulkan: return backend::Backend::Vulkan;
    case RendererApi::Auto:
    default: return backend::Backend::Null;
  }
}



/**
 * The UI-side callbacks a platform window uses. Keeping them abstract is what lets Win32, X11,
 * Wayland and the headless surface share one frame loop: a platform never reaches into the layer
 * tree, it only reports input, close and resize.
 */
class WindowHost {
 public:
  virtual ~WindowHost() = default;

  virtual void dispatchPointer(const PointerEvent& event, int phase) = 0;
  virtual void dispatchKey(const KeyEvent& event, bool down) = 0;
  virtual void dispatchText(char32_t character) = 0;
  virtual void dispatchComposition(const std::string& text) = 0;
  virtual void dispatchCommit(const std::string& text) = 0;
  /** Publishes the input method's lookup table; empty hides the candidate popup. */
  virtual void dispatchCandidates(std::vector<std::string> candidates, size_t selected) = 0;
  virtual void dispatchDelete(int32_t index, uint32_t length) = 0;
  virtual void dispatchScroll(const ScrollEvent& event) = 0;
  /** Trackpad magnification/rotation/swipe; platforms without gestures keep the default no-op. */
  virtual bool dispatchGesture(const GestureEvent& event) {
    (void)event;
    return false;
  }
  virtual void dispatchDrop(const DropEvent& event) = 0;
  /**
   * External (OS) drag routing.
   *
   * A platform drop target translates its enter/over/leave/drop into these calls; the window walks
   * the layer tree exactly like an internal drag and returns the effect the UI accepts so the
   * platform can show the right cursor.
   */
  virtual DragEffect dispatchDragEnter(const DragEvent& event) {
    (void)event;
    return DragEffect::None;
  }
  virtual DragEffect dispatchDragOver(const DragEvent& event) {
    (void)event;
    return DragEffect::None;
  }
  virtual void dispatchDragLeave() {}
  virtual DragEffect dispatchDragDrop(const DragEvent& event) {
    (void)event;
    return DragEffect::None;
  }
  virtual void platformResized(int width, int height) = 0;
  /** macOS titlebar/toolbar safe area changed without necessarily changing the backing size. */
  virtual void platformSafeAreaChanged(const Insets& insets) { (void)insets; }
  /**
   * The OS hid the window (occluded by another app, app hidden, minimised on a platform that reports
   * it this way). An automatic loop stops producing frames until it is visible again; an explicit
   * `Window::renderFrame()` still renders, which snapshot and export code relies on.
   */
  virtual void platformOcclusionChanged(bool visible) {
    (void)visible;
  }
  /** Runs one frame from inside a platform modal loop (Win32 live resize). */
  virtual void renderFrameNow() = 0;
  /** Requests a frame for the next UI-loop iteration. */
  virtual void requestFrame() = 0;
  /** The window's command table, for native menu generation; null on hosts without one. */
  [[nodiscard]] virtual CommandRegistry* commandRegistry() {
    return nullptr;
  }
  virtual bool canClose() const = 0;
  virtual void requestClose() = 0;
  /** Closes without consulting the interceptor; only call after `canClose()` returned true. */
  virtual void closeWindow() = 0;
  /**
   * The system appearance settings changed (high contrast, theme, animation policy).
   *
   * The platform reports the event and does not decide anything: the window re-reads
   * `systemAppearance()` and applies it, which keeps the policy in one place across backends.
   */
  virtual void systemAppearanceChanged() {}
  /**
   * The native window manager changed maximize/minimize/fullscreen state.
   *
   * A platform reports what the compositor actually did; the window uses it instead of trusting the
   * value its own request optimistically stored, so a refused maximize does not leave `isMaximized()`
   * lying.
   */
  virtual void platformStateChanged(bool maximized, bool minimized, bool fullscreen) {
    (void)maximized;
    (void)minimized;
    (void)fullscreen;
  }
  [[nodiscard]] virtual Layer* focusedLayer() const = 0;
  /** The focused control's IME anchor in logical coordinates, or an empty rect. */
  [[nodiscard]] virtual Rect imeCursorRect() const = 0;
  /**
   * The focused text field's contents and caret for `set_surrounding_text`.
   *
   * Offsets are UTF-8 byte offsets; the platform converts them to the character offsets the IME
   * protocols use. Returning false means the focused layer has no text context (or none is focused).
   */
  virtual bool surroundingText(std::string* text, size_t* cursor, size_t* anchor) {
    (void)text;
    (void)cursor;
    (void)anchor;
    return false;
  }
  /**
   * Client-decoration hit test at a window-local point in **device pixels**.
   *
   * `WindowImpl` divides by `uiScale` before it runs `hitTestWindowChrome`, so `WindowChromeMetrics`
   * keeps its logical units. A backend converts its native coordinate once: Cocoa points are
   * multiplied by the backing scale, Win32 `WM_NCHITTEST` and X11 already hand over client pixels.
   */
  virtual WindowHit dispatchHitTest(const Point point) {
    (void)point;
    return WindowHit::Client;
  }
};

/**
 * One native window backend (or the offscreen/headless one).
 *
 * A platform owns the OS window, its input translation and exactly one `gpu::WindowSurface` (or
 * `gpu::Surface` for headless). Present, resize, device recovery, DPI and the native display used for
 * vblank are all behind this interface, so `Window.cpp` no longer contains a platform branch.
 */
class WindowPlatform {
 public:
  virtual ~WindowPlatform() = default;

  [[nodiscard]] virtual Window::NativePlatform kind() const = 0;
  [[nodiscard]] virtual bool headless() const = 0;
  [[nodiscard]] virtual int width() const = 0;
  [[nodiscard]] virtual int height() const = 0;
  [[nodiscard]] virtual float dpiScale() const = 0;

  virtual bool pump(WindowHost& host) = 0;
  [[nodiscard]] virtual graphics::Canvas* beginFrame() = 0;
  virtual bool present() = 0;
  /**
   * True when the platform can take the frame's repaint-boundary lists after the content was recorded
   * into the canvas.
   *
   * A platform that answers true accepts `presentBoundaries` as the end of the frame: the canvas'
   * recording holds everything outside the boundaries and the handed lists complete the picture. False
   * means the caller must record the boundaries into the canvas like any other content.
   */
  [[nodiscard]] virtual bool supportsBoundarySubmission() const {
    return false;
  }
  /** Completes the frame from the boundary lists, in paint order; the submission happens here. */
  virtual bool presentBoundaries(
      const std::vector<graphics::gpu::Surface::ListItem>& items) {
    (void)items;
    return false;
  }
  /**
   * Command buffers the last `present`/`presentBoundaries` produced.
   *
   * A plan frame submits the content plus one list per lent boundary, and each command buffer is its
   * own GPU render pass; this is the number the frame plan exists to keep at one, so it is reported
   * beside the other frame counters rather than inferred from a timing.
   */
  [[nodiscard]] virtual uint32_t lastFrameCommandBuffers() const {
    return 0;
  }
  /**
   * Where the last presented frame's CPU time went, summed over every list of the frame.
   *
   * The stages are the renderer's (coverage fields, precompile, arena, recording, execution); a frame
   * that is several lists would otherwise only report its last list. Zero when the path cannot report.
   */
  [[nodiscard]] virtual graphics::gpu::FrameStages lastFrameStages() const {
    return {};
  }
  /** Draw calls and batches the last frame produced, summed over its lists. */
  [[nodiscard]] virtual uint32_t lastFrameDrawCalls() const {
    return 0;
  }
  [[nodiscard]] virtual uint32_t lastFrameBatches() const {
    return 0;
  }

  /**
   * @return true when this platform can replay its last canvas recording without a new one.
   *
   * The retained path is an optimization for a window whose subtree is clean. It is currently wired
   * for the headless surface only: the window surfaces rotate swapchain images and record through a
   * different object, so they answer false and keep the ordinary begin-frame path.
   */
  [[nodiscard]] virtual bool supportsRetainedFrames() const { return false; }

  /** Replays the retained recording; returns false when there is none or the device refused it. */
  virtual bool presentRetained() { return false; }

  /** @return true when a partial-damage submission can preserve pixels outside the damage rect. */
  [[nodiscard]] virtual bool supportsDamageRects() const { return false; }

  /** Starts a native drag; headless and unwired platforms keep the default (no drag source). */
  virtual DragEffect beginDragOut(const DragPayload&, DragEffect) {
    return DragEffect::None;
  }

  /**
   * The renderer's own per-stage timings for the last frame, in milliseconds.
   *
   * `record` is the CPU work that builds the command buffer, `execute` is the driver's submit call,
   * `gpu` is the device's own time from timestamp queries (one completed frame behind), and `total` the
   * whole `present`. Zero when the backend cannot report them (headless raster, or the threaded host,
   * whose surface lives on the render thread).
   */
  struct FrameRenderTiming {
    double record = 0.0;
    double execute = 0.0;
    double gpu = 0.0;
    double total = 0.0;
    /** Bytes of texture and buffer memory the device holds live (VRAM residency). */
    uint64_t residentBytes = 0;
  };
  [[nodiscard]] virtual FrameRenderTiming lastFrameRenderTiming() const {
    return {};
  }

  /**
   * True when this window presents PQ (HDR10) numbers rather than sRGB.
   *
   * A property of the swapchain the platform was granted, not of the request: a display without HDR10
   * keeps the SDR path, and the frame statistics report which one the user is looking at.
   */
  [[nodiscard]] virtual bool outputIsHdr() const {
    return false;
  }
  [[nodiscard]] virtual OutputColorSpace outputColorSpace() const {
    return OutputColorSpace::Srgb;
  }

  /** Submits the frame with only @p damage written; the rest is loaded from the previous frame. */
  virtual bool presentDamage(const Rect& damage) {
    (void)damage;
    return false;
  }
  [[nodiscard]] virtual bool snapshot(graphics::Bitmap* out) = 0;
  virtual bool resize(uint32_t width, uint32_t height) = 0;
  virtual bool recreate() = 0;

  /** Runtime window appearance; platforms without a native title bar keep the state in `Window`. */
  virtual void setTitle(std::string_view title) { (void)title; }
  virtual void setPosition(int x, int y) {
    (void)x;
    (void)y;
  }
  virtual void setMinimumSize(int width, int height) {
    (void)width;
    (void)height;
  }
  virtual void setFullscreen(bool fullscreen) { (void)fullscreen; }
  /** Makes this window transient for @p parent (Wayland `xdg_toplevel.set_parent`, X11 WM_TRANSIENT_FOR). */
  virtual void setTransientFor(WindowPlatform* parent) { (void)parent; }
  /** Marks the window as a modal dialog (X11 `_NET_WM_STATE_MODAL`; Wayland has no xdg-shell equivalent). */
  virtual void setModal(bool modal) { (void)modal; }
  virtual void setMaximized(bool maximized) { (void)maximized; }
  virtual void setMinimized(bool minimized) { (void)minimized; }
  virtual void setCursor(Window::Cursor cursor) { (void)cursor; }
  /**
   * Grabs/releases the native pointer for a widget capture.
   *
   * Wayland's implicit grab already keeps events flowing while a button is held; X11 needs an explicit
   * XGrabPointer or a fast drag leaves the window and the widget stops receiving motion.
   */
  virtual void setPointerCapture(bool captured) { (void)captured; }
  virtual void setDecorations(Window::Decorations decorations) { (void)decorations; }
  virtual void setToolbarItems(std::vector<Window::ToolbarItem> items) { (void)items; }
  virtual void setToolbarVisible(bool visible) { (void)visible; }
  virtual void setToolbarAutoHideInFullScreen(bool enabled) { (void)enabled; }
  virtual void setToolbarActionHandler(std::function<void(std::string_view)> handler) {
    (void)handler;
  }
  virtual void setTitlebarAccessoryItems(std::vector<Window::TitlebarAccessoryItem> items) {
    (void)items;
  }
  virtual void setTitlebarAccessorySelectionHandler(std::function<void(int)> handler) {
    (void)handler;
  }
  virtual void setToolbarStyle(Window::ToolbarStyle style) {
    (void)style;
  }
  virtual void setTitlebarVisible(bool visible) {
    (void)visible;
  }
  virtual void setWindowButtonsVisible(bool visible) {
    (void)visible;
  }
  virtual void showNativePopover(void* nativeView, Rect anchor) {
    (void)nativeView;
    (void)anchor;
  }
  virtual void hideNativePopover() {}
  [[nodiscard]] virtual bool isNativePopoverVisible() const {
    return false;
  }
  [[nodiscard]] virtual bool supportsNativePopover() const {
    return false;
  }
  virtual void setNativeViewBounds(void* nativeView, Rect bounds, bool visible) {
    (void)nativeView;
    (void)bounds;
    (void)visible;
  }
  virtual void removeNativeView(void* nativeView) {
    (void)nativeView;
  }
  virtual void setTitlebarAccessoryView(void* nativeView) {
    (void)nativeView;
  }
  virtual void showNativeContextMenu(std::vector<Window::NativeMenuItem> items, Point position,
      std::function<void(std::string_view)> handler) {
    (void)items;
    (void)position;
    (void)handler;
  }
  [[nodiscard]] virtual Insets safeAreaInsets() const {
    return {};
  }
  virtual void setSidebarItems(std::vector<Window::SidebarItem> items) {
    (void)items;
  }
  virtual void setSidebarVisible(bool visible) {
    (void)visible;
  }
  virtual void setSidebarSelectionHandler(std::function<void(std::string_view)> handler) {
    (void)handler;
  }
  virtual void setSidebarEditHandler(
      std::function<void(std::string_view, std::string_view)> handler) {
    (void)handler;
  }
  virtual void setSidebarMoveHandler(
      std::function<void(std::string_view, std::string_view, int)> handler) {
    (void)handler;
  }
  virtual bool moveSidebarItem(std::string_view id, std::string_view parentId, int index) {
    (void)id;
    (void)parentId;
    (void)index;
    return false;
  }
  virtual void setSidebarContextActions(std::vector<Window::SidebarContextAction> actions) {
    (void)actions;
  }
  virtual void setSidebarContextActionHandler(
      std::function<void(std::string_view, std::string_view)> handler) {
    (void)handler;
  }
  [[nodiscard]] virtual std::string selectedSidebarItem() const {
    return {};
  }
  virtual bool captureWindow(graphics::Bitmap* out) {
    (void)out;
    return false;
  }
  [[nodiscard]] virtual bool supportsWindowCapture() const {
    return false;
  }
  virtual bool printWindow() {
    return false;
  }
  virtual bool printToFile(std::string_view path) {
    (void)path;
    return false;
  }
  [[nodiscard]] virtual bool supportsPrinting() const {
    return false;
  }
  virtual void setContentFocusRingEnabled(bool enabled) {
    (void)enabled;
  }
  virtual void setBackgroundMaterial(Window::BackgroundMaterial material) {
    (void)material;
  }
  virtual void showShareSheet(std::string_view text, Rect anchor) {
    (void)text;
    (void)anchor;
  }
  [[nodiscard]] virtual bool supportsShareSheet() const {
    return false;
  }
  virtual void setDockBadge(std::string_view text) {
    (void)text;
  }
  virtual void requestUserAttention(bool critical) {
    (void)critical;
  }
  virtual void setSecureInputEnabled(bool enabled) {
    (void)enabled;
  }
  [[nodiscard]] virtual bool isSecureInputEnabled() const {
    return false;
  }
  virtual void setOpacity(float opacity) { (void)opacity; }
  virtual void setAlwaysOnTop(bool onTop) { (void)onTop; }
  /** The OS light/dark preference; a platform without one reports light. */
  [[nodiscard]] virtual bool prefersDarkAppearance() const { return false; }
  /** EDR headroom of the window's display, 1.0 for SDR. */
  [[nodiscard]] virtual float displayHeadroom() const { return 1.0f; }
  /** True when the display advertises Display P3 / wide gamut. */
  [[nodiscard]] virtual bool supportsWideColor() const { return false; }
  /**
   * The system's current appearance: high contrast, the light/dark preference and the animation policy.
   *
   * A backend that reports nothing returns the default (invalid), and the window keeps the theme the
   * application installed. The colours are only read when high contrast is on.
   */
  [[nodiscard]] virtual SystemAppearance systemAppearance() const { return {}; }
  /** Raises the native window and asks the OS to activate it; platforms without activation ignore it. */
  virtual void raise() {}
  /** The window rectangle in screen coordinates; Wayland has no global position and uses (0,0). */
  [[nodiscard]] virtual Rect screenBounds() const {
    return Rect::MakeLTRB(0.0f, 0.0f, static_cast<float>(width()),
      static_cast<float>(height()));
  }

  /**
   * Blocks for at most @p timeoutMs waiting for native input.
   *
   * `run` calls this instead of rendering when it is idle in `OnDemand` mode, so an editor with no
   * animation and no input costs a wake-up every @p timeoutMs rather than a full frame at 60 Hz. The
   * default sleeps, which is correct but not wakeable; a platform with a message queue overrides it.
   */
  virtual void waitForEvents(int timeoutMs) {
    if (timeoutMs > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
    }
  }

  /**
   * Wakes a call blocked in `waitForEvents`, for a frame requested from another thread.
   *
   * The default does nothing, and the pending timeout bounds the latency. Win32 posts a message;
   * X11 and Wayland write to their `WakePipe`, which is part of the wait's poll set.
   */
  virtual void wake() {}

  [[nodiscard]] virtual std::string diagnostics() const = 0;
  [[nodiscard]] virtual graphics::gpu::NativeDisplay nativeDisplay() const = 0;

  /** The platform window handle (HWND, X11 Window, wl_surface), or null for headless. */
  [[nodiscard]] virtual void* nativeWindowHandle() const {
    return nativeWindow();
  }
  [[nodiscard]] virtual void* nativeWindow() const { return nullptr; }

  /** A portal `parent_window` handle (`x11:<id>` or `wayland:<handle>`), or empty. */
  [[nodiscard]] virtual std::string portalParentHandle() const { return {}; }

  /**
   * Installs the accessibility snapshot the platform bridge publishes.
   *
   * The platform never walks the layer tree itself: it asks for a snapshot when its accessibility
   * protocol requests one (Windows: `WM_GETOBJECT`), so the tree is read on the UI thread and only
   * while a client is actually asking. A platform without a bridge ignores the callback.
   */
  virtual void setAccessibilityTreeProvider(std::function<AccessibilityNode()> provider,
    AccessibilityActionInvoker invoker = {}) {
    (void)provider;
    (void)invoker;
  }

  /**
   * Tells the platform bridge that keyboard focus moved, so it can raise its focus-changed event
   * (Windows: `UiaRaiseAutomationEvent` with the automation focus id, plus `NotifyWinEvent`).
   */
  virtual void raiseAccessibilityFocusEvent() {}

  /** Announces a value/selection change so a client can react without polling the tree. */
  virtual void raiseAccessibilityPropertyEvent(std::string_view path, AccessibilityEvent event) {
    (void)path;
    (void)event;
  }

  /** The frame target's MSAA sample count, which the capability snapshot reports. */
  [[nodiscard]] virtual uint32_t sampleCount() const {
    return 1;
  }

  /**
   * The device this window renders through, or null before a backend is up.
   *
   * An application uses it to create a producer on the **same** device -- a preview surface, a
   * `TextureOutputNode`, a capture surface -- which is what makes sampling that producer zero-copy.
   */
  [[nodiscard]] virtual backend::Device* device() const {
    return nullptr;
  }

  /** Transfers ownership of the platform-created device to the application, if it has one. */
  [[nodiscard]] virtual backend::Device* releaseDevice() {
    return nullptr;
  }

  /** Called whenever the UI focus changes; the Wayland backend uses it to toggle text-input/IME. */
  virtual void onFocusChanged(Layer* focused) {
    (void)focused;
  }

  /** Updates the IME candidate anchor; platforms that place a candidate window use it. */
  virtual void updateImeCursorRect(const Rect& rect) {
    (void)rect;
  }

  /** @return true when the input method consumed a candidate-navigation key. */
  virtual bool dispatchImeKey(uint32_t key) {
    (void)key;
    return false;
  }
};

/** Creates the platform selected by the options, including Linux's Wayland鈫扻11 fallback. */
[[nodiscard]] std::unique_ptr<WindowPlatform> createWindowPlatform(const Window::Options& options,
  backend::Device* sharedDevice, std::string* diagnostics);

[[nodiscard]] std::unique_ptr<WindowPlatform> createHeadlessPlatform(const Window::Options& options,
  backend::Device* sharedDevice, std::string* diagnostics);

[[nodiscard]] std::unique_ptr<WindowPlatform> createRasterPlatform(const Window::Options& options,
  backend::Device* sharedDevice, std::string* diagnostics);

#if defined(_WIN32)
[[nodiscard]] std::unique_ptr<WindowPlatform> createWin32Platform(const Window::Options& options,
  backend::Device* sharedDevice, std::string* diagnostics);
#elif defined(CORAL_UI_COCOA)
[[nodiscard]] std::unique_ptr<WindowPlatform> createCocoaPlatform(const Window::Options& options,
  backend::Device* sharedDevice, std::string* diagnostics);
#elif defined(CORAL_UI_X11)
[[nodiscard]] std::unique_ptr<WindowPlatform> createX11Platform(const Window::Options& options,
  backend::Device* sharedDevice, std::string* diagnostics);
#endif

#if defined(CORAL_UI_WAYLAND)
[[nodiscard]] std::unique_ptr<WindowPlatform> createWaylandPlatform(const Window::Options& options,
  backend::Device* sharedDevice, std::string* diagnostics);
#endif

} // namespace coral::ui::detail

#endif // CORAL_UI_WINDOWPLATFORM_H

