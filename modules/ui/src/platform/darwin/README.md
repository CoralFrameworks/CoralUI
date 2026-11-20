# Darwin platform (reserved)

No Cocoa backend is implemented yet. The Cocoa adapters would plug into the same
`WindowPlatform`/`ClipboardPlatform`/`FileDialogPlatform` interfaces as the Win32 and Linux
backends.

When implementing it, add these adapters here and wire them from `modules/ui/CMakeLists.txt` under
`if (APPLE)`, mirroring the win32/linux/android split:

| File | Implements | Notes |
|---|---|---|
| `WindowCocoa.mm` | `WindowPlatform` (create/show/hide/invalidate/size, input dispatch) | `NSWindow` + `NSView` with a `CAMetalLayer` or `NSOpenGLView`, matching whichever backend CoralGraphics enables on macOS. Retina scaling must feed the existing `uiScale` path rather than being baked into layer coordinates. |
| `ClipboardCocoa.mm` | `ClipboardPlatform` | `NSPasteboard` with `changeCount` polling for `Clipboard::notifySystemChanged`; put rich types (UTF-8 text, PNG) on the pasteboard, not a single string. |
| `FileDialogCocoa.mm` | `FileDialogPlatform` | `NSOpenPanel`/`NSSavePanel`; the completion arrives on the main queue, so marshal it back to the UI thread. |
| `InputCocoa.mm` | event mapping | `NSEvent` mouse/key into `PointerEvent`/`KeyEvent`; Command-based shortcuts map through `shortcutLabelFor` conventions. |

Acceptance: the macOS build produces the UI library with these adapters, and `ui-gates` and
`ui-gates -PackageSmoke` pass on a macOS runner.

Keep Objective-C++ out of the general `modules/ui` sources: desktop Linux/Windows builds must not
need `<Cocoa/Cocoa.h>`, which is why these files live here and are only added in the `APPLE` branch.
