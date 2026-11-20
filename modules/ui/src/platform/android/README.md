# Android platform (reserved)

There is no Android window backend yet. The Android adapters would plug into the same
`WindowPlatform`/`ClipboardPlatform`/`FileDialogPlatform` interfaces as the desktop backends.

When implementing it, add these adapters here and wire them from `modules/ui/CMakeLists.txt` under
`if (ANDROID)`, mirroring the win32/linux split:

| File | Implements | Notes |
|---|---|---|
| `WindowAndroid.cpp` | `WindowPlatform` (create/show/hide/invalidate/size, input dispatch) | The surface comes from `ANativeWindow`; rendering already has an Android path in CoralGraphics and CoralPlayback. Text input needs `ANativeActivity_showSoftInput` and a `TextInputConnection` that forwards `commitText`/`setComposingText`. |
| `ClipboardAndroid.cpp` | `ClipboardPlatform` | `android.content.ClipboardManager` through JNI; announce changes with the existing `Clipboard::notifySystemChanged` hook. |
| `FileDialogAndroid.cpp` | `FileDialogPlatform` | `ACTION_OPEN_DOCUMENT`/`ACTION_CREATE_DOCUMENT` via `startActivityForResult`; the result arrives asynchronously on the UI thread. |
| `InputAndroid.cpp` | touch/key mapping | Android reports touch, not a mouse: map `ACTION_DOWN`/`MOVE`/`UP` to `PointerEvent`, and turn `KeyEvent` back (Back/Enter) into `KeyEvent`. |

Acceptance: the Android build produces the UI library with these adapters, and at least one
headless UI test runs on the emulator with the skip ratio bounded.

Do not add JNI glue to the general `modules/ui` sources: the desktop build must keep compiling
without `jni.h`, which is why the adapter files live here and are only added in the `ANDROID` branch.
