# Native control extensions

CoralUI's cross-platform UI contract is the custom-drawn control set under `ui/`:

```text
Button, IconButton, SearchField, Toggle, CheckBox, RadioButton
Slider, SegmentedControl, Select, ColorPicker, DateField
TextInput, NumberField, ProgressBar, Table, TreeView, ListView, ...
```

Those controls must behave the same on Windows, Linux and macOS. A macOS-only control such as
`NSButton` must not become the cross-platform `Button` implementation.

## The general host: NativeViewLayer

`ui/NativeViewLayer.h` is the platform-neutral host for a native view:

```cpp
auto layer = std::make_shared<NativeViewLayer>(nativeHandle);
layer->setBounds(Rect::MakeLTRB(20.0f, 20.0f, 180.0f, 60.0f));
root->addChild(layer);
```

It synchronizes attachment, frame and visibility through the generic `Window` API:

```cpp
Window::setNativeViewBounds(void* nativeView, Rect bounds, bool visible)
Window::removeNativeView(void* nativeView)
```

A platform backend implements those two hooks in its `WindowPlatform`:

- macOS: `NSView` inside the Cocoa content view;
- Windows: child `HWND`/XAML island/WebView host;
- Linux: X11 child window, Wayland subsurface or GTK widget.

The core layer does not know which native type it hosts. `NativeViewLayer` is inert until a platform
backend implements the hooks.

## Platform control namespaces

Concrete native controls are platform extensions, not core API:

```text
include/ui/macos/NativeButton.h
include/ui/macos/NativeSearchField.h
include/ui/macos/NativePopUpButton.h
include/ui/macos/NativeColorWell.h
include/ui/macos/NativeSwitch.h
include/ui/macos/NativeSlider.h
include/ui/macos/NativeDatePicker.h
include/ui/macos/NativeSegmentedControl.h

namespace coral::ui::macos { ... }

include/ui/win32/NativeButton.h
include/ui/win32/NativeSwitch.h
include/ui/win32/NativeSearchField.h
include/ui/win32/NativePopUpButton.h
include/ui/win32/NativeProgressIndicator.h
include/ui/win32/NativeSlider.h
include/ui/win32/NativeStepper.h
include/ui/win32/NativeDatePicker.h

namespace coral::ui::win32 { ... }
```

These headers are installed only on their platform (`modules/ui/CMakeLists.txt`). They are optional: an
app that wants AppKit- or Win32-managed controls opts into `coral::ui::macos` or
`coral::ui::win32`; an app that wants one cross-platform UI uses the controls under `ui/`.

## Windows implementation

The Windows extension follows the macOS shape without changing the core controls:

- `WindowWin32` implements `WindowPlatform::setNativeViewBounds`/`removeNativeView`;
- every wrapper creates a real child HWND (`BUTTON`, `EDIT`, `COMBOBOX`,
  `msctls_progress32`, `msctls_trackbar32`, `msctls_updown32`, `SysDateTimePick32`);
- the wrapper owns the child HWND; the window only positions, shows, hides and detaches it;
- `WM_COMMAND`/`WM_NOTIFY`/`WM_HSCROLL` are routed back to the wrapper by subclassing the host
  window with the control's id (`NativeControlHost` under `src/platform/win32`).

The mapping is deliberately "closest native control", not one-to-one AppKit parity:

| Win32 wrapper | Native class | Notes |
|---|---|---|
| `NativeButton` | `BUTTON` | `BS_PUSHBUTTON`, `BN_CLICKED` |
| `NativeSwitch` | `BUTTON` | `BS_AUTOCHECKBOX`; Win32 has no `NSSwitch` equivalent |
| `NativeSearchField` | `EDIT` | `EM_SETCUEBANNER` for the placeholder |
| `NativePopUpButton` | `COMBOBOX` | `CBS_DROPDOWNLIST`, `CBN_SELCHANGE` |
| `NativeProgressIndicator` | `msctls_progress32` | determinate bar or `PBS_MARQUEE` |
| `NativeSlider` | `msctls_trackbar32` | `WM_HSCROLL` |
| `NativeStepper` | `msctls_updown32` | `UDN_DELTAPOS`, wrapper-owned increment |
| `NativeDatePicker` | `SysDateTimePick32` | `DTN_DATETIMECHANGE` |

There is no native inline colour well or segmented control in Win32. Those should stay as the core
`ColorPicker`/`SegmentedControl` on Windows rather than pretending a checkbox group is a segmented
control. `NativeControlSize` exists for source parity; Windows keeps the system metrics.

## Windows limitations

A child HWND is an **airspace overlay**, not a texture composited into the Layer's frame. This is the
same model as an `NSView` above the Metal content on macOS, and it has the same consequences:

- the native control is always above GPU-rendered content; a CoralUI `Popup`/`Overlay`/`Dialog`
  cannot draw over it;
- opacity, blend mode, scale, rotation, rounded clips and shadows do not apply to it; clipping is the
  parent client rectangle (a scroll viewport needs `SetWindowRgn` if exact clipping is required);
- `Window::snapshot`, headless rendering and golden-image tests do not contain it; the gallery prints
  a label saying the block is live HWND only;
- Win32 owns focus, keyboard, IME and its own UI Automation tree; CoralUI focus traversal and
  `AccessibilityNode` do not automatically include the child control;
- the control uses the system theme; dark mode needs `SetWindowTheme`/DWM attributes and will not
  match CoralUI theme tokens exactly;
- DPI changes reposition the child in `WindowWin32` using `mDpiScale`; fractional-scale and
  multi-monitor cases must be tested on a real desktop.

## Rules

1. Do not add a platform-native control to the `coral::ui` namespace.
2. Do not make a core control return `nullptr` on a platform without a native implementation; core
   controls are always available and custom-drawn.
3. Platform extensions may expose only platform-specific affordances (AppKit toolbar, native menus,
   native color well); they must not replace a core control's public contract.
4. Every platform extension must document its minimum platform and its no-op behavior when the
   platform view cannot be created.
5. A platform extension owns its native handle. `NativeViewLayer` and the window backend only borrow,
   position, show/hide and detach it; they must not destroy a view the platform wrapper still owns.

## Cross-platform use

A `Native*` control is an optional platform extension, not a core control:

- its header is under `ui/macos` or `ui/win32` and is installed only on that platform;
- the class lives in `coral::ui::macos` or `coral::ui::win32`, never in `coral::ui`;
- `NativeViewLayer` is the only platform-neutral piece; on a backend without native-view
  hooks it is inert.

Application code that targets several platforms should therefore choose at the call site and keep a
core fallback:

```cpp
std::shared_ptr<Layer> progress;
#if defined(__APPLE__)
progress = coral::ui::macos::NativeProgressIndicator::create(false);
#elif defined(_WIN32)
progress = coral::ui::win32::NativeProgressIndicator::create(false);
#endif
if (progress == nullptr) {
  progress = std::make_shared<coral::ui::ProgressBar>(font, 0.0);
}
```

The same pattern applies to `NativeStepper` vs `Stepper`, `NativeDatePicker` vs `DateField`,
`NativeSwitch` vs `Toggle`, and so on.

The `ui_demo` gallery follows this rule: the native block is compiled for `__APPLE__` and `_WIN32`,
with a text fallback on other platforms. Headless screenshots do not render native children, so the
gallery labels that block "live native view only".

Linux has no equivalent available in this repository today. X11 could host a child window (XEmbed or
a toolkit socket), but Wayland has no cross-client native-control embedding protocol and GTK4 removed
`GtkSocket`/`GtkPlug`; a Linux extension would therefore need a separate X11-only design and a
custom-drawn fallback on Wayland.
