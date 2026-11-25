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
```

These headers are installed only on macOS (`modules/ui/CMakeLists.txt`). They are optional: an app
that wants AppKit-managed controls opts into `coral::ui::macos`; an app that wants one cross-platform
UI uses the controls under `ui/`.

A future Windows extension would follow the same shape:

```text
include/ui/win32/NativeButton.h
include/ui/win32/NativeSearchField.h
namespace coral::ui::win32 { ... }
```

and implement the `NativeViewLayer` hooks in `WindowPlatform`. No core control changes are required.

## Rules

1. Do not add a platform-native control to the `coral::ui` namespace.
2. Do not make a core control return `nullptr` on a platform without a native implementation; core
   controls are always available and custom-drawn.
3. Platform extensions may expose only platform-specific affordances (AppKit toolbar, native menus,
   native color well); they must not replace a core control's public contract.
4. Every platform extension must document its minimum platform and its no-op behavior when the
   platform view cannot be created.

## Cross-platform use

A `Native*` control is an optional macOS extension, not a core control:

- its header is under `ui/macos` and is installed only on Apple platforms;
- the class lives in `coral::ui::macos`, never in `coral::ui`;
- `NativeViewLayer` is the only platform-neutral piece; on a backend without native-view
  hooks it is inert.

Application code that targets several platforms should therefore choose at the call site and keep a
core fallback:

```cpp
std::shared_ptr<Layer> progress;
#if defined(__APPLE__)
progress = coral::ui::macos::NativeProgressIndicator::create(false);
#endif
if (progress == nullptr) {
  progress = std::make_shared<coral::ui::ProgressBar>(font, 0.0);
}
```

The same pattern applies to `NativeStepper` vs `Stepper`, `NativeDatePicker` vs `DateField`,
`NativeSwitch` vs `Toggle`, and so on.

The `ui_demo` gallery follows this rule: the native progress indicator/stepper block is compiled
under `#if defined(__APPLE__)`, with a text fallback on other platforms. Headless screenshots do not
render `NSView` children, so the gallery labels that block “live NSView only”.

A future `ui/win32` or `ui/linux` extension can expose the same shape of wrappers in its own
namespace while `NativeViewLayer` and `Window::setNativeViewBounds` stay unchanged.
