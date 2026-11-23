# Darwin platform (macOS Cocoa)

The macOS window backend, added to `modules/ui/CMakeLists.txt` under `if (APPLE AND NOT iOS)`
behind the `CORAL_UI_COCOA` define. It presents through the same `graphics::gpu::WindowSurface` path
as the Win32 and Linux backends: the content view is backed by a `CAMetalLayer`, which is handed to
the RHI as the native window, so frames go straight to the display.

| File | Implements | Notes |
|---|---|---|
| `WindowCocoa.mm` | `WindowPlatform` (create/show/hide/size, input dispatch, appearance) | Standard titled `NSWindow` + an unflipped `NSView` whose backing layer is a `CAMetalLayer`. The framework owns the frame loop, so the backend drains AppKit's queue from `pump()` and never calls `[NSApp run]`. |
| `ClipboardCocoa.mm` | `ClipboardPlatform` | `NSPasteboard` with `text/plain`, `text/html`, `image/png` and file URLs; `changeCount` is the system change counter. |
| `FileDialogCocoa.mm` | `FileDialogPlatform` | `NSOpenPanel`/`NSSavePanel` run modally; extensions from `FileDialogOptions::filters` map to `allowedFileTypes`. |

## Coordinates and scaling

AppKit speaks points and uses a bottom-left window origin; the framework speaks device pixels and
top-left layout coordinates. The content view is deliberately **not** flipped: a flipped layer-backed
view also flips its `CAMetalLayer` geometry, and `contentsGravity = topLeft` then anchors the last
drawable from the other edge while a vertical resize changes the height (the picture jittered).
Keeping the layer unflipped makes top-left gravity mean the visual top-left; input positions are
flipped once in `devicePointForEvent` and multiplied by the backing scale before reaching
`WindowHost`, which divides them back by `uiScale`. The layer's `bounds`/`position`/`contents`/
`drawableSize` implicit Core Animation actions are disabled as well: an animated bound would make the
content chase the window during a vertical resize. `Window::Options::width`/`height` are client points on macOS, so a 960x540 request is a
960x540-point window (1920x1080 backing pixels at 2x) and the layer tree lays out at 960x540 logical
units. `Window::resize` still takes device pixels, matching the other backends.

The surface is resized from the backing bounds whenever the window or display changes, and
`NSWindowDelegate` reports minimize/zoom/fullscreen state so `Window::isMaximized()` reflects what
the window manager actually did.

## Window chrome

macOS always uses the **standard titled `NSWindow`**. The window server owns the title bar, window
drag, edge hit testing, live resize, zoom, full screen and Spaces integration; CoralUI puts its
layer tree in the window's content view, backed by a `CAMetalLayer`. This is deliberate:

- `Window::Decorations::Client` is accepted for API compatibility but maps to the system frame on
  this backend. A borderless window that draws its own caption has to reimplement the move loop and
  the resize border, and in practice the edges are hard to grab and the window can stop following the
  pointer.
- `CoralWindow` still overrides `canBecomeKeyWindow`/`canBecomeMainWindow` so a window created with
  unusual style masks can take keyboard focus, but the normal path never needs it.

## Live resize

A standard window's frame is moved by the window server, so a resize stays responsive even while the
content catches up. The content path is still optimized:

- A live resize renders from `windowDidResize:` only when the backing size actually changed, at the
  display's full backing resolution, so text stays crisp throughout. Nothing about the layer changes
  at the start of the drag (no resolution switch, no `contentsGravity` change, no display-sync
  toggle); those transitions themselves were the flash at the beginning of a resize.
- The next frame is paced from the previous one's cost: `max(16 ms, 2 × cost)`, capped at 50 ms, so a
  cheap frame updates at 60 Hz while an expensive one backs off instead of blocking AppKit's own
  resize loop. The window frame is native, so a slower content frame cannot stall the resize itself.
- pointer/scroll dispatch is skipped for the duration of the resize; those events would each request
  another frame.
- `WindowImpl::renderFrameNow()` (the AppKit live-resize path and Win32's live-resize timer) renders
  without the frame clock's deadline sleep, which exists to align animation frames with a display
  deadline rather than to make interactive input wait.
- `windowDidEndLiveResize:` draws one final frame and resets the throttle.

`CORAL_UI_COCOA_DEBUG=1` prints each resize frame with its `resize`, `acquire`, `present` and renderer
stage timings.

## Not implemented yet

IME (`NSTextInputClient`), accessibility (AXUIElement), drag and drop, and OpenGL presentation. The
backend requires the Metal build of CoralGraphics; the default backend priority already selects
Metal on macOS.
