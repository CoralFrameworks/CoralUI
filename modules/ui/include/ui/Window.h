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

#ifndef CORAL_UI_WINDOW_H
#define CORAL_UI_WINDOW_H

#include <ui/Types.h>

#include <ui/Event.h>
#include <ui/Geometry.h>
#include <ui/Layer.h>
#include <ui/Renderer.h>
#include <ui/RendererCapabilities.h>
#include <ui/Overlay.h>
#include <ui/SystemAppearance.h>
#include <ui/Theme.h>
#include <ui/Undo.h>
#include <ui/WindowChrome.h>


#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace coral::ui {

class CommandRegistry;
class RendererDevice;
struct AccessibilityNode;
struct WindowImpl;

namespace host {
class ThreadedRenderHost;
}

/**
 * When a window produces frames.
 *
 * `Continuous` renders whenever the loop wakes, which is the first-milestone behaviour. `OnDemand`
 * renders only after `requestFrame` (input, a posted task, an animation, a content frame), which is
 * what keeps an idle editor from recording and submitting 60 frames a second.
 */
enum class FramePolicy {
  Continuous,
  OnDemand,
};

/**
 * Which side owns the device and presents the frame.
 *
 * `SameThread` (the default): the UI thread records and presents on the same thread.
 * `RenderThread` records a `FramePacket` on the UI thread and submits it to a render thread that owns
 * the device. Headless, Windows native and X11 native targets are supported; Wayland is refused
 * because its surface shares the wl_display event queue with UI input dispatch until that queue is
 * moved to the render thread. Presentation completion, async resize and multi-window sharing one
 * render host are wired; `Application` installs the shared host for threaded windows.
 */
enum class RenderHostKind {
  SameThread,
  RenderThread,
};

/** Where the last frame's wall clock went; all values are milliseconds. */
struct FrameTimings {
  /** Animation advance plus the layout pass (the phases before any drawing). */
  double update = 0.0;
  /** The measure + layout pass, a subset of `update`. Zero when the frame relaid nothing out. */
  double layout = 0.0;
  /** Recording the tree (and any overlay) into the frame's command stream. */
  double draw = 0.0;
  /** The tree-recording part of `draw`, excluding the content-frame and overlay bookkeeping. */
  double paint = 0.0;
  double present = 0.0;
  double total = 0.0;
  bool valid = false;
};

/** Cumulative frame counters for a diagnostics overlay or a product status line. */
struct FrameStatistics {
  uint64_t frames = 0;
  uint32_t recoveries = 0;
  double lastTotalMilliseconds = 0.0;
  double averageTotalMilliseconds = 0.0;
  double lastPresentMilliseconds = 0.0;
  /** The render host serving this window; the render-thread path also fills the queue/drop counters. */
  RenderHostKind hostKind = RenderHostKind::SameThread;
  /** The window's refresh rate in Hz, or 0 when the platform does not report one. */
  double refreshHertz = 0.0;
  /**
   * Render-thread host: packets waiting for the render thread (0 or 1, because it keeps only the newest).
   *
   * The same-thread path presents on the UI thread and always reports 0.
   */
  uint32_t queueDepth = 0;
  /** Render-thread host: packets superseded by a newer one before the render thread picked them up. */
  uint64_t droppedFrames = 0;
  /** Render-thread host: frames the render thread could not present (device loss and similar). */
  uint64_t failedFrames = 0;
  /** The renderer's record stage and driver submit for the last frame, in milliseconds (0 if unknown). */
  double lastRenderMilliseconds = 0.0;
  double lastExecuteMilliseconds = 0.0;
  /** The device's own time for the last completed frame, from timestamp queries (0 if unavailable). */
  double lastGpuMilliseconds = 0.0;
  /** Bytes of texture and buffer memory the device holds live (VRAM residency). */
  uint64_t residentBytes = 0;
  /** Content counters for the current layer tree. */
  uint32_t layers = 0;
  uint32_t visibleLayers = 0;
  /**
   * Layers that asked for a repaint / a layout pass when this frame started.
   *
   * These are the retained-mode counters: one control changing colour should move `dirtyNodes` by
   * one and leave `layoutDirtyNodes` at zero; a text or size change moves both. A frame that moves
   * neither is a retained replay with no work.
   */
  uint32_t dirtyNodes = 0;
  uint32_t layoutDirtyNodes = 0;
  /** The frame's damage rectangle as a fraction of the window; 0 when the frame was a full repaint. */
  double damagedAreaRatio = 0.0;
  /**
   * How many repaint boundaries rebuilt their display list in this frame.
   *
   * A static frame must report 0 (nothing re-recorded), and a change confined to one boundary must
   * report 1. This is the §9 gate for "only the changed boundary is rebuilt"; it is the counter R3/R4
   * decisions are supposed to be based on, so it exists before those stages do.
   */
  uint32_t repaintedBoundaries = 0;
  /**
   * Layers the draw pass skipped because their paint rectangle missed the clip.
   *
   * This is the counter that says a scrolled page costs what its viewport shows: a frame that draws a
   * 2400-row list should report most rows here and only a viewport's worth of drawing work.
   */
  uint32_t culledLayers = 0;
  /**
   * Boundary lists the last frame was assembled from, or 0 when it recorded everything into one list.
   *
   * A frame that reports a non-zero count went through the frame plan: content recorded once, boundaries
   * lent rather than re-recorded, which is R3's frame shape.
   */
  uint32_t framePlanItems = 0;
  /** Composite items the frame was assembled from: boundaries + composited viewports + overlays. */
  uint32_t compositeItems = 0;
  /** Bytes of recorded lists the frame lent instead of copying; the complement of `frameListBytes`. */
  uint64_t lentListBytes = 0;
  /** Lists the device actually received: 1 for a flattened frame, 1 + `framePlanItems` for a planned one. */
  uint32_t presentItems = 0;
  /**
   * Command buffers (GPU render passes) the last frame took to present those lists.
   *
   * Every list opens its own pass unless it fits the frame that is already open, so this is the number
   * the plan path minimizes: a plan frame that lends three lists in one pass reports 1.
   */
  uint32_t commandBuffers = 0;
  /** Draw calls and batches the last frame's lists produced, summed over the frame. */
  uint32_t drawCalls = 0;
  uint32_t batches = 0;
  /**
   * Where the last frame's CPU time went, in milliseconds, summed over every list it was made of.
   *
   * The renderer's own stages: a plan frame is several lists and each is precompiled, assembled and
   * recorded, so only the totals across the frame say where its milliseconds are.
   */
  struct RenderStages {
    double fields = 0.0;
    double placeImages = 0.0;
    double precompile = 0.0;
    double targets = 0.0;
    double arena = 0.0;
    double vertexUpload = 0.0;
    double record = 0.0;
    double execute = 0.0;
    double total = 0.0;
    /** The parts of record by draw source; they add up to at most record. */
    double rect = 0.0;
    double gradient = 0.0;
    double image = 0.0;
    double path = 0.0;
    double field = 0.0;
    double composite = 0.0;
  } renderStages;
  /**
   * Bytes of recorded lists copied into the frame.
   *
   * Static frames must report 0; a frame that redraws copies every replayed list, which is the cost
   * R3 would remove.
   */
  uint64_t frameListBytes = 0;
  /** True when the window presented PQ numbers (HDR10); false when it fell back to sRGB. */
  bool hdrOutput = false;
};

/**
 * A native window with a layer tree, or the headless surface the tests use.
 *
 * The window owns one `gpu::WindowSurface` and one layer tree. Its run loop is the M0/M1 frame loop:
 * pump platform events, ask the `FrameClock` for the next frame, draw the tree, present. In headless
 * mode the same loop draws into an offscreen `gpu::Surface`, which is how layout and drawing are
 * tested without a display (and how a screenshot is produced).
 *
 * ## Threading
 *
 * The window and its layers are thread-affine: create, render and destroy them on one thread. The
 * platform message loop also runs there, so no lock is needed around the tree.
 */
class Window final : public Renderer {
  friend class Application;
 public:
  enum class NativePlatform {
    Auto,
    Win32,
    X11,
    Wayland,
    Cocoa,
  };

  /** macOS native window tabbing preference; ignored by the other backends. */
  enum class TabbingMode : uint8_t {
    Automatic,
    Preferred,
    Disallowed,
  };

  struct Options {
    /**
     * Initial client size.
     *
     * Native Windows and Linux windows interpret it as device pixels; a macOS window interprets it
     * as points, because AppKit's window geometry is point-based. The layer tree still lays out at
     * `width() / uiScale()` logical units on every platform.
     */
    int width = 1280;
    /** Initial client size (device pixels; points on macOS). */
    int height = 720;
    std::string title = "Coral UI";
    /** Desktop application id: Wayland `app_id`, X11 `WM_CLASS`, used for grouping and the task bar. */
    std::string appId = "coral-ui";
    /** Render into an offscreen surface and never create a native window. */
    bool headless = false;
    /**
     * Force a pure CPU raster frame sink. `Auto` uses the platform GPU path; `Raster` creates a
     * headless bitmap-backed window with no device at all.
     */
    RendererBackend rendererBackend = RendererBackend::Auto;
    /** When frames are produced. `OnDemand` needs `requestFrame` (input, tasks and animations do it). */
    FramePolicy framePolicy = FramePolicy::OnDemand;

    /**
 * Whether the UI thread also owns the device (`SameThread`) or records packets for a render
 * thread (`RenderThread`). Defaults to `SameThread`, the historical path.
     */
    RenderHostKind renderHost = RenderHostKind::SameThread;

    /**
     * Advanced: an application/session host shared by several windows. `Application` installs one
     * automatically for render-thread windows; a single window leaves it null and owns its own host.
     */
    std::shared_ptr<host::ThreadedRenderHost> sharedRenderHost;
    /** Headless runs stop after this many frames; 0 means no limit. */
    int frameLimit = 0;
    /**
     * Native backend preference.
     *
     * Linux only: Auto prefers Wayland when WAYLAND_DISPLAY is set and falls back to X11 when the
     * Wayland connection or surface creation fails; X11/Wayland force one native backend. On macOS
     * `Cocoa` is the only native backend and `Auto` selects it; Windows ignores the field.
     */
    NativePlatform platform = NativePlatform::Auto;
    Color clearColor = Color::MakeRGBA8(24, 26, 32, 255);
    bool enableValidation = false;

    /**
     * Where to place the native window, in virtual-screen coordinates; unset keeps the platform default.
     *
     * An acceptance run that has to cover a specific monitor names that monitor's origin here (the
     * secondary display on a two-monitor desktop can start at a negative x). Headless windows ignore it.
     */
    std::optional<Point> position;

    /**
     * Ask for a PQ (HDR10) presentation when the display offers one.
     *
     * The canvas draws sRGB-encoded colours; the presentation pass converts them with the BT.2408 white
     * point. A display without HDR10 keeps the SDR picture (`FrameStatistics::hdrOutput` says which one
     * happened). `CORAL_HDR_OUTPUT=1` in the environment requests the same thing.
     */
    bool hdrOutput = false;

    /**
     * An existing renderer device to share with this window, or null to create one.
     *
     * Native Win32/X11/Wayland windows can share a device; the handle must outlive every window
     * that borrowed it. `Application` owns the handle and passes it to every window.
     */
    std::shared_ptr<RendererDevice> sharedDevice;

    /**
     * Which renderer API to prefer, or `Auto` to let the platform choose.
     *
     * Normal applications leave this at `Auto`. The explicit values exist for backend conformance
     * tests and benchmarking; no application needs to know whether frames land on a GPU or a raster
     * surface.
     */
    RendererApi rendererApi = RendererApi::Auto;

    /**
     * Logical-to-physical pixels. 0 means "automatic": the platform DPI for a native window and 1.0
     * for a headless one. A non-zero value overrides it (a design preview at 2x, a test at 1.5x).
     */
    float uiScale = 0.0f;
    /** When false, an automatic scale stays 1.0 even on a high-DPI display. */
    bool useDpiScale = true;

    /**
     * macOS only: AppKit frame autosave name, so the window restores its frame across launches.
     * Empty disables frame saving; the position is centred as before.
     */
    std::string frameAutosaveName;

    /** macOS only: native window tabbing preference. */
    TabbingMode tabbingMode = TabbingMode::Automatic;
  };

  ~Window();
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  /** @return null when no device (or, for a native window, no platform window) could be created. */
  [[nodiscard]] static std::unique_ptr<Window> create(const Options& options);

  /**
   * The reason the last `create()` on this thread returned null.
   *
   * Platform and host creation failures otherwise have no object to report diagnostics through; a
   * caller that shows an error message (or a test that expects a rejection) reads this instead.
   */
  [[nodiscard]] static std::string lastCreationError();

  void setRoot(std::shared_ptr<Layer> root);

  /**
   * A layer the window draws above the page and above its own popups, laid out at the window size.
   *
   * This is the slot for a development panel (frame statistics, a debug HUD, a profiler readout) that
   * has to survive root rebuilds and stay readable while a menu is open. The layer is hit-test
   * transparent, so it never takes a click away from the product; a `Flex` in this slot positions its
   * children absolutely, which is how a fixed-size panel goes in a corner.
   *
   * Pass nullptr (or call `clearOverlay`) to remove it.
   */
  void setOverlay(std::shared_ptr<Layer> overlay);
  /** Removes the overlay layer; safe when none is set. */
  void clearOverlay();
  /** The layer passed to `setOverlay`, or null. */
  [[nodiscard]] const std::shared_ptr<Layer>& overlay() const noexcept;
  [[nodiscard]] const std::shared_ptr<Layer>& root() const noexcept;
  /** A snapshot for a platform accessibility bridge; safe to call at any time. */
  [[nodiscard]] AccessibilityNode accessibilityTree() const;

  /**
   * Focusable nodes in platform tab order, copied out of the temporary tree.
   *
   * A platform AX bridge can consume this directly without re-sorting children itself.
   */
  [[nodiscard]] std::vector<AccessibilityNode> accessibilityFocusNodes() const;
  [[nodiscard]] Layer* focusedLayer() const noexcept;

  /**
   * The topmost visible focus scope, or null when focus is not trapped.
   *
   * A recycling list uses this before re-focusing a row so a restored row cannot steal focus out of
   * a modal/popup scope that appeared while the row was detached.
   */
  [[nodiscard]] Layer* activeFocusScope() const noexcept;

  [[nodiscard]] int width() const noexcept;
  [[nodiscard]] int height() const noexcept;

  /** Updates the native title bar/task switcher name. No-op on a headless window. */
  void setTitle(std::string title);
  [[nodiscard]] const std::string& title() const noexcept;

  /** Moves the native window's top-left to screen coordinates (@p x, @p y), in device pixels. */
  void setPosition(int x, int y);

  /** Sets the native minimum client size; 0 disables that axis. Already-clamped by the platform. */
  void setMinimumSize(int width, int height);
  [[nodiscard]] int minimumWidth() const noexcept;
  [[nodiscard]] int minimumHeight() const noexcept;

  /** Enters/leaves native fullscreen. Wayland lets the compositor decide the final state. */
  void setFullscreen(bool fullscreen);
  [[nodiscard]] bool isFullscreen() const noexcept;

  /** The pointer shape the window requests; a control can override it while hovered. */
  enum class Cursor : uint8_t {
    Arrow,
    IBeam,
    Hand,
    ResizeHorizontal,
    ResizeVertical,
    Move,
    Crosshair,
    NotAllowed,
    Wait,
  };
  void setCursor(Cursor cursor);
  [[nodiscard]] Cursor cursor() const noexcept;

  /**
   * Whether the OS (system) or the framework (client) draws the title bar and border.
   *
   * The macOS backend always keeps the system frame: `Client` is accepted for API compatibility but
   * maps to `System`, because the window server owns drag, edge resize and zoom there.
   */
  enum class Decorations : uint8_t { System, Client };
  void setDecorations(Decorations decorations);
  [[nodiscard]] Decorations decorations() const noexcept;

  /** Chrome geometry used by client-side decorations; ignored in system-decoration mode. */
  void setChromeMetrics(WindowChromeMetrics metrics);
  [[nodiscard]] const WindowChromeMetrics& chromeMetrics() const noexcept;

  /**
   * Hit-tests a window-local logical point in client-decoration mode.
   *
   * A system-decorated window answers `Client` (the OS owns the frame). A platform maps the result to
   * its native hit-test constants; the framework itself only needs it for the client-drawn buttons.
   */
  [[nodiscard]] WindowHit hitTest(Point point) const;

  /** Native maximize/restore/minimize. Fullscreen owns the state while it is active. */
  void maximize();
  void restore();
  void minimize();
  [[nodiscard]] bool isMaximized() const noexcept;
  [[nodiscard]] bool isMinimized() const noexcept;

  /** Whole-window opacity in [0,1]; platforms without compositing keep the value but ignore it. */
  void setOpacity(float opacity);
  [[nodiscard]] float opacity() const noexcept;
  void setAlwaysOnTop(bool onTop);
  [[nodiscard]] bool isAlwaysOnTop() const noexcept;

  /**
   * Makes this window transient for @p parent: it stays above the parent and must close with it.
   *
   * The relationship is also what `WindowManager::isBlocked` uses to enforce modality.
   */
  void setTransientFor(Window* parent);
  [[nodiscard]] Window* transientFor() const noexcept;
  /** A modal window blocks its transient parent's input until it closes. */
  void setModal(bool modal);
  [[nodiscard]] bool isModal() const noexcept;

  /**
   * The window rectangle in screen coordinates (device pixels).
   *
   * Wayland has no global window position; it returns `{0, 0, width, height}`. A headless window
   * returns its client size at the origin.
   */
  [[nodiscard]] Rect bounds() const;

  /**
   * Installs a close interceptor, called on the UI thread for a title-bar close / Alt+F4 / `requestClose`.
   *
   * Return true to allow the close, false to veto it (for example after showing a save prompt). The
   * default (no callback) allows the close.
   */
  void setCloseRequestedCallback(std::function<bool()> callback);

  /** Requests a close through the interceptor; a native close button uses the same path. */
  void requestClose();

  /**
   * Sets initial geometry before the first frame; the native window is moved/resized immediately.
   *
   * Runs before the first frame so a restored window does not flash at the default position.
   */
  void restoreGeometry(Rect screenBounds);

  /**
   * Raises the window and asks the OS to activate it.
   *
   * Wayland has no client-side activation protocol, and a headless window has no OS presence, so
   * both are no-ops. The call is safe on any platform and does not block.
   */
  void raise();

  /** Processes pending platform messages. @return false when the window is closing. */
  bool pump();

  /** Injects input as if it came from the platform. The tests use these; the WndProc uses them too. */
  bool pointerDown(const PointerEvent& event);
  bool pointerUp(const PointerEvent& event);
  bool pointerMove(const PointerEvent& event);

  /**
   * Routes every pointer event for @p pointerId to @p layer until it is released.
   *
   * A layer that starts a gesture on press uses this when the gesture has to keep tracking the pointer
   * after it leaves the layer's bounds -- a slider, a splitter, a canvas tool. The capture is held
   * weakly, so a layer removed from the tree loses it instead of leaving a dangling target; a pointer
   * up releases it as well. Mouse events use pointer id 0.
   */
  void setPointerCapture(const std::shared_ptr<Layer>& layer, std::uint32_t pointerId = 0);
  /** Ends a capture started with `setPointerCapture`; safe when none is held. */
  void releasePointerCapture();
  /** The layer currently capturing the pointer, or null. */
  [[nodiscard]] Layer* pointerCapture() const noexcept;
  /** The pointer id a capture is held for. */
  [[nodiscard]] std::uint32_t pointerCaptureId() const noexcept;
  bool keyDown(const KeyEvent& event);
  bool keyUp(const KeyEvent& event);
  bool textInput(char32_t character);
  bool textComposition(const std::string& text);
  bool textCommit(const std::string& text);

  /**
   * Shows the input method's candidate list near the focused control's caret.
   *
   * The IME backend supplies the strings (typically `1. candidate`); the window owns presentation,
   * placement and keyboard navigation. An empty list hides the popup.
   */
  void setImeCandidates(std::vector<std::string> candidates, size_t selected = 0);
  void clearImeCandidates();
  [[nodiscard]] bool imeCandidatesVisible() const noexcept;
  [[nodiscard]] size_t imeCandidateCount() const noexcept;
  [[nodiscard]] size_t imeCandidateSelected() const noexcept;
  bool scroll(const ScrollEvent& event);
  /** Delivers a trackpad gesture to the layer under `event.position`; true when handled. */
  bool gesture(const GestureEvent& event);
  /** Injects a file drop as if it came from the platform. */
  void drop(const DropEvent& event);

  /** Draws and presents one frame. */
  bool renderFrame() override;

  [[nodiscard]] RendererKind rendererKind() const noexcept override;
  [[nodiscard]] const RendererCapabilities& capabilities() const noexcept override;
  bool recover() override;

  /**
   * Asks for one frame.
   *
   * Thread-safe and coalescing: any number of requests before the next frame produce one frame. In
   * `Continuous` mode this is a no-op for the loop; in `OnDemand` mode it is what wakes an idle
   * window. Input, `post` and a running `Layer` animation already request frames themselves.
   */
  void requestFrame();

  /** The policy new requests are interpreted under; changing it takes effect on the next loop pass. */
  void setFramePolicy(FramePolicy policy);
  [[nodiscard]] FramePolicy framePolicy() const noexcept;

  /** Stops the run loop after the current pass. Use from the UI thread (a posted task works). */
  void quit();

  /** Where the last completed frame's time went, or an invalid record before the first frame. */
  [[nodiscard]] FrameTimings lastFrameTimings() const noexcept;

  /** Frame count, recovery count and total/average frame time since creation. */
  [[nodiscard]] FrameStatistics frameStatistics() const noexcept;

  /** True when a frame has been requested and not yet produced. */
  [[nodiscard]] bool frameRequested() const noexcept;

  /** Called on the UI thread after a frame has been presented (including a retained replay). */
  void setFrameCallback(std::function<void()> callback);

  /**
   * Adds a frame-completion observer without replacing the legacy `setFrameCallback` slot.
   *
   * Several diagnostics (a stats overlay, a profiler, a test probe) can observe the same window.
   * @return a token for `removeFrameCallback`.
   */
  uint64_t addFrameCallback(std::function<void()> callback);
  void removeFrameCallback(uint64_t id);

  /**
   * Marks @p deviceRect as needing a repaint on the next frame.
   *
   * The rectangle is in device pixels. A platform without partial-damage support still repaints the
   * whole window, so this is a hint rather than a contract about pixels outside the rect.
   */
  void invalidate(const Rect& deviceRect);

  /** The union of invalidation rectangles accumulated for the next frame. */
  [[nodiscard]] Rect damage() const noexcept;

  /** Clears the accumulated damage without drawing. */
  void clearDamage() noexcept;

  /** Runs `pump` + `renderFrame` until the window closes or the headless frame limit is reached. */
  int run();

  /**
   * One non-blocking run-loop iteration: pump events, render one frame when the policy asks for it.
   *
   * `Application` drives several windows with this. @return false when the window stopped.
   */
  bool tick(bool blockOnIdle = true);

  /** @return true while the window has not been closed or quit. */
  [[nodiscard]] bool running() const noexcept;

  /** Re-renders the last frame into a bitmap. Works for headless windows and native ones. */
  [[nodiscard]] bool snapshot(Bitmap* out) override;

  /**
   * Requests a client size in **device pixels** and lays the tree out again.
   *
   * `width()`/`height()` afterwards are the size the platform actually granted: a window manager
   * can clamp it, and DPI scaling can make an exact request impossible. Logical layout units are
   * always `width() / uiScale()`; callers must not assume the requested value was preserved.
   */
  bool resize(int width, int height);

  /** The native window handle (HWND/X11 Window/wl_surface), or null for headless. */
  [[nodiscard]] void* nativeHandle() const noexcept;

  /**
   * The value to pass to `FileDialogOptions::parentWindow`.
   *
   * X11 returns `x11:<window-id>`; Wayland exports the toplevel through xdg-foreign and returns
   * `wayland:<handle>`. Empty when the platform has no handle or the protocol is unavailable.
   */
  [[nodiscard]] std::string portalParentHandle() const;

  /** The display scale (1.0 at 96 DPI). Headless windows always report 1.0 unless overridden. */
  [[nodiscard]] float dpiScale() const noexcept;

  /** Logical-to-physical scale used for layout and drawing. */
  [[nodiscard]] float uiScale() const noexcept;

  /** The effective devicePixelRatio; the same value as `uiScale()`. */
  [[nodiscard]] float devicePixelRatio() const noexcept;

  /** The native backend that was actually selected (Auto for headless windows). */
  [[nodiscard]] NativePlatform nativePlatform() const noexcept;

  /**
   * The opaque renderer device this window draws through, or null when the backend is not up.
   *
   * Content producers that need the RHI device use `ui/WindowBackend.h`; ordinary UI code needs
   * only this handle to share a device between windows.
   */
  [[nodiscard]] std::shared_ptr<RendererDevice> rendererDevice() const;

  /**
   * Transfers ownership of a device this window created to the caller.
   *
   * The window keeps rendering through it but no longer deletes it. `Application` uses this to own
   * one device for all windows; a window created with `Options::sharedDevice` returns null.
   */
  [[nodiscard]] std::shared_ptr<RendererDevice> releaseRendererDevice();

  /**
   * The platform differences this window's frames can rely on.
   *
   * Captured once after the device comes up and refreshed by `recoverDevice`, so it is stable for a
   * frame and safe to read from the UI thread. Layers read it through `RenderContext::capabilities`.
   */
  [[nodiscard]] const RendererCapabilities& rendererCapabilities() const noexcept;

  /**
   * Recreates the window/offscreen surface after a device or swapchain loss.
   *
   * The run loop calls this once when a frame fails; an application that owns its own loop can call it
   * explicitly. @return false when no new surface could be created.
   */
  bool recoverDevice();
  /** Re-probes capabilities after the sharing Application recovered the host device. */
  bool recoverFromSharedHost();

  /** The platform diagnostics, empty when nothing went wrong. */
  [[nodiscard]] std::string diagnostics() const;

  /**
   * @return true when frames reach a native window.
   *
   * False for headless targets and for a render-thread host that fell back to an offscreen surface
   * because the platform did not expose a window handle. Frames and `snapshot()` still work in that
   * case, so tests must use this instead of assuming presentation.
   */
  [[nodiscard]] bool isRenderTargetOnScreen() const;

  /** The clock's measured refresh, valid once the display source has warmed up. */
  [[nodiscard]] double refreshHertz() const noexcept;

  /** Wall-clock time of the last completed frame, in milliseconds. */
  [[nodiscard]] double lastFrameMilliseconds() const noexcept;

  /** Number of completed frames since creation. */
  [[nodiscard]] int frameCount() const noexcept;

  /** Number of successful device/surface recreations. */
  [[nodiscard]] int recoveryCount() const noexcept;

  /** Sets the keyboard/IME focus to @p layer (or clears it with nullptr). */
  void focusLayer(Layer* layer);

  /**
   * The window's command table.
   *
   * A key event first goes to the focused layer (so a text field owns its editing shortcuts) and
   * then to this table. Bind shortcuts with `CommandRegistry::bind`.
   */
  [[nodiscard]] CommandRegistry& commands();

  /**
   * Queues @p task to run on the UI thread at the start of the next pump.
   *
   * Thread-safe. Model/background producers use this (directly or through a `UiExecutor`) to apply
   * changes to the layer tree without touching it from their own thread.
   */
  void post(std::function<void()> task);

  /** Replaces the theme used by the clear colour and the helpers that read it. */
  /**
   * Shows @p layer above the page and returns an id for `dismissOverlay`.
   *
   * This is the single entry point for transient surfaces: stacking, light dismiss, the modal scrim,
   * Escape and the focus trap are owned here rather than re-implemented by every popup. The window
   * keeps the layer alive; dismissing detaches it and restores the focus it interrupted.
   */
  uint64_t pushOverlay(std::shared_ptr<Layer> layer, OverlayOptions options = {});

  /**
   * Starts an internal drag carrying @p payload; the window routes moves and the drop until the
   * pointer is released, Escape is pressed or `cancelDrag` is called.
   *
   * @p preview is an optional layer that follows the pointer (a drag image). It is hosted by the
   * overlay, so the page cannot interact with it. `beginDrag` fails when a drag is already running
   * or the payload is empty.
   */
  bool beginDrag(DragPayload payload, std::shared_ptr<Layer> preview = {});

  /**
   * Hands the payload to the platform for an external drag (Win32 OLE today).
   *
   * Call from a mouse-down gesture: on Win32 this runs the blocking `DoDragDrop` loop and
   * returns the effect the drop target chose. A headless window has no platform drag, so it
   * returns `DragEffect::None`. Other platforms are not wired yet and also return None.
   */
  DragEffect beginPlatformDrag(const DragPayload& payload, DragEffect allowed);
  /**
   * Routes a cross-window drag hover into this window.
   *
   * The manager calls this when the pointer is over this window's screen rectangle; the window walks
   * the layer tree from the hit layer upwards and returns the effect the acceptor chose. A window
   * that is not receiving hovers keeps no state, so `dragLeave()` is cheap to call defensively.
   */
  DragEffect dragHover(const DragPayload& payload, Point position);
  void dragLeave();
  /** Delivers a drop; returns whether some layer accepted it. */
  bool dragDrop(const DragPayload& payload, Point position);
  void cancelDrag();
  [[nodiscard]] bool isDragging() const noexcept;
  /** The payload of the drag in progress, or null. Valid until the drag ends. */
  [[nodiscard]] const DragPayload* dragPayload() const noexcept;
  /** Dismisses one overlay by id; @return false when it is no longer open. */
  bool dismissOverlay(uint64_t id);
  /** Dismisses the topmost overlay. */
  bool dismissTopOverlay();
  /** Dismisses every overlay. */
  void dismissAllOverlays();
  [[nodiscard]] bool hasOverlay() const noexcept;

  /**
   * The window-level undo history.
   *
   * The window also binds `edit.undo` (Ctrl+Z) and `edit.redo` (Ctrl+Shift+Z, Ctrl+Y) to it, so a
   * menu can bind those names and get correct enabled state without any extra wiring. A focused
   * editor that handles the shortcut itself wins over the window command.
   */
  [[nodiscard]] UndoStack& undoStack();
  [[nodiscard]] const UndoStack& undoStack() const;

  /** The two built-in skins; a theme mode switches every control through `Window::setTheme`. */
  enum class ThemeMode : uint8_t {
    Light,
    Dark,
    /** Follow the operating system's light/dark preference. */
    System,
  };
  void setTheme(Theme theme);
  /** Applies the built-in light or dark theme and remembers the mode. */
  void setThemeMode(ThemeMode mode);
  [[nodiscard]] ThemeMode themeMode() const noexcept;
  /** Re-reads the OS preference when the mode is `System` (call it from a settings/system event). */
  void refreshThemeMode();

  /**
   * Applies what the platform reports about the system's appearance.
   *
   * High contrast replaces the app theme with a flat palette built from the system colours (see
   * `highContrastTheme`), and turning it off restores the theme the app had installed. The reduce-motion
   * flag is applied process-wide, so animations started afterwards snap instead of animating. The
   * window calls this when it is created and again whenever the platform reports `WM_SETTINGCHANGE`.
   */
  void applySystemAppearance(const SystemAppearance& appearance);
  /** True while the high-contrast palette is the theme in use. */
  [[nodiscard]] bool highContrast() const noexcept;
  /** The last appearance the platform reported (invalid when it reports nothing). */
  [[nodiscard]] SystemAppearance systemAppearance() const;
  /**
   * Installs a style sheet on the whole window.
   *
   * Swapping the sheet re-resolves every style-named layer, which is the hot-reload path for a theme
   * editor; `nullptr` clears it and falls back to each control's theme colours.
   */
  void setStyleSheet(std::shared_ptr<const StyleSheet> sheet);
  [[nodiscard]] std::shared_ptr<const StyleSheet> styleSheet() const noexcept;
  /** Sets the GPU clear colour used for the whole window background. */
  void setClearColor(Color color);
  [[nodiscard]] Color clearColor() const noexcept;
  /**
   * Development switch for the R3 frame plan; defaults to on unless `CORAL_UI_FRAME_PLAN=0`.
   *
   * Only affects frames that are full repaints on a platform that can take boundary lists.
   */
  static void setFramePlanEnabled(bool enabled) noexcept;
  /**
   * Forces the frame plan on or off for a scope and restores the previous setting.
   *
   * The switch is process-wide, so a test that turns it on would otherwise leave every later test running
   * a different frame path -- which is how a test ordering turns into a timing measurement nobody can
   * compare.
   */
  class FramePlanScope {
   public:
    explicit FramePlanScope(bool enabled) noexcept;
    ~FramePlanScope();
    FramePlanScope(const FramePlanScope&) = delete;
    FramePlanScope& operator=(const FramePlanScope&) = delete;

   private:
    int mPrevious = -1;
  };
  /** The raw override: -1 follows the environment, 0 forces off, 1 forces on. Used by `FramePlanScope`. */
  static int framePlanOverride() noexcept;
  static void setFramePlanOverride(int value) noexcept;
  [[nodiscard]] static bool framePlanEnabled() noexcept;

  [[nodiscard]] const Theme& theme() const noexcept;

 private:
  explicit Window(std::unique_ptr<WindowImpl> impl);

  /**
   * Applies a theme that has already been resolved.
   *
   * `setTheme` resolves what the caller asked for against the high-contrast state; this is the part
   * that hands the result to the tree and asks for a frame, so both paths share it.
   */
  void applyResolvedTheme(std::shared_ptr<const Theme> theme);

  std::unique_ptr<WindowImpl> mImpl;
};

} // namespace coral::ui

#endif // CORAL_UI_WINDOW_H

