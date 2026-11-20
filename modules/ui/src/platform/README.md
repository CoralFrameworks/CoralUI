# modules/ui/src/platform

Platform-specific window, clipboard and dialog implementations live here. The common interfaces and
the headless backend stay at this level; an OS-specific directory contains only that OS's
implementation.

| Directory | Contents |
|---|---|
| `linux/` | X11 and Wayland windows (including fractional scale via viewporter), X11 and Wayland clipboards, xdg-desktop-portal file dialog with `zenity`/`kdialog` fallback. |
| `win32/` | Win32 window, native clipboard and `IFileOpenDialog`/`IFileSaveDialog`. |
| `darwin/` | Reserved for the Cocoa backend; no implementation yet. |
| `android/` | Reserved for the Android backend; no implementation yet. |

Files at the root are platform-neutral:

- `WindowPlatform.h` — the `WindowPlatform`/`WindowHost` interfaces and factories.
- `WindowPlatform.cpp` — runtime Linux backend selection.
- `WindowHeadless.cpp` — the headless window.
- `ClipboardPlatform.h` / `ClipboardPlatformFallback.cpp` — the native clipboard factory; the
  fallback returns null so `Clipboard::system()` installs `MemoryClipboard`.
- `FileDialogPlatform.h` / `FileDialogPlatformFallback.cpp` — the native file-dialog factory; the
  fallback returns a cancelled result. Linux's portal client is in `linux/FileDialogPortal.cpp`.

When adding a backend, put the implementation under the OS directory and add it to
`modules/ui/CMakeLists.txt` in the corresponding platform block. Do not add `#if defined(_WIN32)` /
`#if defined(__linux__)` branches to the shared translation units.
