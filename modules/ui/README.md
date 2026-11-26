# CoralUI — the GUI framework

CoralUI is a cross-platform desktop GUI framework in C++20. It provides the retained-mode UI
toolkit behind Coral applications: a layer tree with layout, painting, input and focus; a
declarative element layer and view-model adapters; a widget set; text layout and shaping; theming,
animation, i18n and accessibility; and the native window and application surface. It builds on
CoralCore and CoralGraphics and owns no renderer of its own: a frame is recorded as a draw list and
submitted through CoralGraphics' GPU or software raster backend.

Entry points: `<ui/UI.h>` (umbrella), `<ui/Window.h>` and `<ui/Application.h>`. The repository is
one project in two roles: the application adds it as a framework subproject, and a standalone
checkout builds it for independent development and verification. The view-model layer is the
separate `modules/mvvm` library; `modules/ui` is the toolkit.

## Features

### Retained layer tree

- `Layer` base: bounds, visibility, clipping, z-order, opacity, translation/scale/rotation, blend
  modes, rounded and path clips, shadows, transform origins, hit testing and focus.
- Retained recording with damage rectangles and per-layer caches; layout is recomputed only when
  invalidated, so an idle window does not repaint.
- Layout parameters: grow/shrink/basis, percent sizes, margins, min/max sizes, alignment, absolute
  positioning.
- Containers: `Flex` (CSS-shaped one-dimensional layout, wrapping, justify), `Grid`, `ScrollView`,
  `SplitView`, `DockView`, `TabBar`, `Overlay`, `ContentView` for externally produced textures.

### Declarative elements

- Immutable `Element` tree with keyed `Reconciler` patching, `Component` state and context,
  lifecycle hooks, `ElementHost` binding, `State<T>` and `LayerRef`.
- `ui/dsl` is the variadic, theme-scoped façade prototype (`Column`, `Row`, `Page`, `Card`, `Section`, `Canvas`, `Overlay`); see `docs/declarative-dsl.md` and `docs/dsl-api.md`.
- `OverlayHost` owns transient Dialog/Popup/Toast/Tooltip/Menu subtrees above the page through
  `Window::pushOverlay` (stacking, light dismiss, Escape, focus restore).
- `ElementRegistry` builds subtrees from a flat `type#key prop=value` specification
  (`<ui/ElementSpec.h>`), so a UI description can be data as well as code.
- 80 of the 89 public layer types have a first-class element wrapper; the rest are hosted through
  `components::Layer`. The wrapper matrix, the `ElementSpec` v0 properties and the gaps are in
  [docs/declarative-components.md](../../docs/declarative-components.md).
- `ui/mvvm`: `DataContext`, command and observable-list bindings, and a UI-thread `Dispatcher`
  adapter over the `mvvm` library.

### Text

- `TextLayer`, `RichTextLayer` and the declarative `StyledText`/`TextDocument` with wrapping,
  alignment (including justify), line height, ellipsis and truncation, selection, link hit testing
  and RTL.
- Unicode text stack: UAX #9 bidi resolution (including nested isolate/override spans), UAX #14
  line breaking with the Unicode test corpus, UAX #29 grapheme and word boundaries, and Liang/TeX
  hyphenation for thirteen languages (English, Italian, Spanish, French, Portuguese, German,
  Russian, Greek, Dutch, Swedish, Danish, Finnish and Catalan).
- Vertical text (`Vertical`, `VerticalMixed`, `VerticalRl`), CJK-aware line breaking, curated
  system-font fallback, and locale-aware shaping.
- DPI-aware rendering at device resolution, subpixel/ClearType text where the platform provides it,
  and grayscale fallback elsewhere.
- `TextInput` with selection, clipboard, undo/redo, IME composition and commit, and UAX #29
  grapheme-aware editing and caret movement.

### Controls

- Form and input: `Button`, `IconButton`, `ColorButton`, `CheckBox`, `Toggle`, `RadioButton`,
  `SegmentedControl`, `Slider`, `ProgressBar`, `Spinner`, `NumberField`, `SearchField`,
  `TextInput`.
- Collections and navigation: `ListView`, `RecyclingListView`, `Table`, `TableView`, `TreeView`,
  `CandidateListView`, `Menu`, `MenuBar`, `ContextMenu`, `TabBar`, `DockView`, `SplitView`.
- Overlays and dialogs: `Dialog`, `Popup`, `Toast`, `Tooltip`, `Overlay`.
- Pickers: `ColorPicker`, `ColorInput`, `ColorDialog`.
- Window chrome: `TitleBar` with native move/maximize/minimize/close hit testing and client-side
  decoration support.

### Application and windowing

- `Window`: native Win32, X11, Wayland and macOS Cocoa windows plus headless and raster targets;
  logical-unit layout with per-monitor DPI scaling; fullscreen, minimum size, title/position, close
  interception and runtime theme updates.
- `Application` and `WindowManager`: multi-window management, activation and window-layout
  capture/restore.
- On-demand frame scheduling, frame callbacks, same-thread and threaded render hosts, device
  recovery, readback snapshots and frame timing telemetry.
- `FrameStatsOverlay` and `ProcessStats` for live diagnostics; `CrashReporter` writes a minidump
  plus a text report on Windows and a text report on other platforms.

### Platform integration

- Clipboard: multi-MIME read/write (UTF-8 text, HTML, PNG, file lists, custom MIME), X11/Wayland
  PRIMARY selection, X11 INCR transfers and change notification.
- Drag and drop: Windows OLE, X11 XDND, macOS `NSDraggingSource`/`NSDraggingDestination`.
- File dialogs: Win32 `IFileOpenDialog`/`IFileSaveDialog`; Linux xdg-desktop-portal with
  `zenity`/`kdialog` fallback.
- Native alerts: Windows `TaskDialogIndirect` with a `MessageBoxW` fallback; macOS `NSAlert`,
  attached as a sheet when the parent window is available.
- Native control hosting: `NativeViewLayer` hosts a platform child view at a layer's absolute
  bounds; Windows wrappers live under `ui/win32` (child HWNDs) and macOS wrappers under
  `ui/macos` (`NSView`). The hosted control is an airspace overlay, not part of the GPU frame.
- IME: Win32 IMM32, Linux IBus/XIM and Wayland `text-input` v1/v3, including surrounding-text and
  delete-surrounding support; macOS `NSTextInputClient` with marked text and candidate anchoring.
- System appearance: dark/light, contrast and reduced-motion preferences where the platform
  exposes them.

### Styling, animation and localization

- `Theme` tokens (colours, spacing, radii, shadows, control colours), `ThemeFile` loading with hot
  reload, light/dark presets, and runtime theme switching.
- All built-in custom-drawn controls except the debug-only `FrameStatsOverlay` override
  `applyTheme`, so `Window::setTheme` retokens the live tree without rebuilding it. Caller-owned child
  layers (table cells, popup content) have to use theme colours themselves; `Chip`/`Banner` severity
  colours, `ColorPicker` hue fields, native dialogs and platform-native controls keep their
  semantic/system colours by design.
- `Style`, `StylePatch`, `StyleVariant`, `StyleSheet` and `ControlState` for per-control states.
- Animation: cubic-bezier/spring curves, animation controllers, implicit transitions, shared-element
  transitions, and a process-wide reduced-motion switch.
- i18n: translation catalogs with plural categories and interpolation, `NumberFormat` and
  `DateFormat`, and locale-aware controls.

### Accessibility

- Platform-neutral accessibility node snapshot: role, name, description, value, states and actions.
- Windows UI Automation provider with Invoke/Toggle/Value/RangeValue/Scroll/Grid/GridItem/Text/
  SelectionItem/Selection patterns and focus/property/structure events.
- Linux AT-SPI D-Bus bridge (Accessible/Component/Action/Cache with focus state changes).
- macOS `NSAccessibility` tree with roles, actions, text values/selection, grid metadata, hit testing
  and focused-element notifications.

### Verification and packaging

- Headless rendering, snapshot and golden-image tests; the UI suite covers layout, input, focus,
  controls, text, DPI, themes, windows and the declarative layer.
- `examples/` builds the framework's demo and verification programs; its headless checks render,
  snapshot and encode a PNG without a display.
- Installed SDK: `find_package(CoralUI)` exports `Coral::ui`, public headers and the i18n catalogs.

## Supported platforms

| Capability | Windows | Linux X11 | Linux Wayland | macOS |
|---|---|---|---|---|
| Native window and GPU surface | Yes | Yes | Yes | Yes, Cocoa + `CAMetalLayer` |
| High-DPI scaling | Per-monitor v2 | Global DPI | Per-output scale, including fractional | Backing scale, per screen |
| Client-side title bar | Yes | Yes | Yes; server-side `xdg-decoration` when offered | System frame only (`Client` maps to `System`) |
| IME | IMM32 | IBus/XIM | `text-input` v1/v3 | `NSTextInputClient` |
| Native chrome | Custom | Custom | Custom / server-side | Toolbar, sidebar/outline, titlebar accessory, native menus |
| Clipboard | Multi-MIME, files, PNG | Multi-MIME, PRIMARY, INCR | Per-MIME, PRIMARY | Multi-MIME, HTML, PNG, file URLs |
| Drag and drop | OLE | XDND | Not yet | `NSDraggingSource` / destination |
| File dialogs | Native | Portal, `zenity`/`kdialog` | Portal | `NSOpenPanel`/`NSSavePanel` |
| Native alerts | `TaskDialog` | In-window `Dialog` | In-window `Dialog` | `NSAlert` |
| Native control hosting | `NativeViewLayer` child HWND | Not yet | Not yet | `NativeViewLayer` NSView |
| Accessibility | UI Automation | AT-SPI | AT-SPI | `NSAccessibility` tree, actions, text, grid, hit test |
| Maximize/minimize | Yes | Yes | Yes | Yes (zoom/miniaturize) |
| Always-on-top | Yes | X11 only | No | Yes |
| Threaded render host | Yes | Yes | Rejected with a diagnostic | Yes |

Headless and software-raster targets work on every platform and are what the test suite and the
examples use. The macOS backend presents through Metal and drains AppKit's event queue from
CoralUI's own frame loop, so it does not call `[NSApp run]`. Android has no native window backend
yet; Windows, Linux and macOS are the supported desktop product targets.

## Building

A standalone clone needs CoralCore and CoralGraphics as the `external/` git submodules and the shared
`libz`/`libyuv`/`libgtest` submodules under `third_party/`. A checkout in the application's
`frameworks/` tree reuses the sibling CoralCore/CoralGraphics and the application's targets instead.
GoogleTest is needed only when the tests are built:

```sh
git submodule update --init --recursive

# Windows
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

# Linux
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure

# macOS
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

- `-DCORAL_BUILD_TESTS=OFF` builds the libraries without GoogleTest.
- `-DCORAL_TEST_DATA_DIR=<path>` points the tests and benchmarks at the reference fonts and assets;
  a standalone checkout defaults to the bundled `examples/` fonts, and the examples always use them.
- `-DCORAL_SHARED_THIRD_PARTY_DIR=<path>` uses a specific `libz`/`libyuv`/`libgtest` tree instead of
  the bundled `third_party/` submodules.
- `-DCORAL_CORE_DIR` / `-DCORAL_GRAPHICS_DIR` point at explicit framework checkouts; otherwise the
  `external/` submodules are used, then the sibling checkouts.

The examples build with the framework (`cmake --build build`, one tree for libraries, tests and
examples) or as their own project:

```sh
# Linux
cmake -S examples -B out/examples -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/examples
ctest --test-dir out/examples -L examples --output-on-failure

# macOS
cmake -S examples -B out/examples -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build out/examples -j8
ctest --test-dir out/examples -L examples --output-on-failure
```

When the application adds CoralUI as a subproject it provides CoralCore and CoralGraphics first;
this repository then adds only `modules/mvvm` and `modules/ui`, builds no examples, and builds tests
only when the caller sets `CORAL_BUILD_TESTS`. `examples/README.md` documents the demo programs.

## Using the framework

```cmake
add_subdirectory(CoralUI)
target_link_libraries(my_app PRIVATE ui)          # add mvvm when using <ui/mvvm/...>
```

```cpp
#include <ui/UI.h>

coral::ui::Window::Options options;
options.width = 960;
options.height = 540;
auto window = coral::ui::Window::create(options);

auto root = std::make_shared<coral::ui::Flex>(coral::ui::Axis::Column);
root->setPadding({ 24.0f, 24.0f, 24.0f, 24.0f });
root->addChild(std::make_shared<coral::ui::Button>("Hello"));
window->setRoot(root);
window->run();
```

## Boundaries

CoralUI is a GUI toolkit, not a product shell or an application framework: application-level
widgets, navigation and product pages live in the application. It does not provide a CSS selector
engine, an ABI-stable binary interface yet, or an Android window backend. The source-level API is the supported
contract; rebuild against the matching CoralCore/CoralGraphics revisions.
