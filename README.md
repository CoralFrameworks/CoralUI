# CoralUI

CoralUI is the cross-platform GUI framework of the Coral project. It is one repository in two roles:

- the application adds it as a framework subproject — the application has already added CoralCore and
  CoralGraphics, so this root only adds the view-model and UI modules;
- a standalone checkout configures from this root for independent development, and configures
  `examples/` for the demo and verification programs.

The toolkit is a retained-mode UI framework: a `Layer` tree with layout, painting, input and focus;
a declarative `Element`/`Reconciler` layer; a widget set; Unicode text layout and shaping; theming,
animation, i18n and accessibility; and native Windows, Linux and macOS windows plus headless and
raster targets. It sits on CoralGraphics and owns no renderer of its own. Public entry points are
`<ui/UI.h>`, `<ui/Window.h>` and `<ui/Application.h>`.

## Layout

```
modules/ui/         the toolkit: layer tree, elements, controls, text, windows, platform backends
modules/mvvm/       the view-model layer (observable objects, commands, lists, validation)
examples/           the demo and verification project, with its reference fonts
cmake/              shared build settings and the package configs
external/           CoralCore and CoralGraphics as git submodules
third_party/        the shared libz / libyuv / libgtest submodules
```

`modules/ui/README.md` documents the toolkit's features and supported platforms;
`modules/mvvm/README.md` documents the view-model layer; `examples/README.md` documents the demos.

## Features

- **Retained layer tree** — bounds, clipping, z-order, transforms, opacity, blend modes, rounded and
  path clips, shadows, hit testing and focus; layout recomputed only when invalidated.
- **Layout** — `Flex` (grow/shrink/basis, percent, margins, min/max, alignment, wrapping, justify),
  `Grid`, `ScrollView`, `SplitView`, `DockView`, `TabBar`, `Overlay`, `ContentView`.
- **Declarative UI** — immutable `Element` tree, keyed `Reconciler` patching, `Component` state and
  lifecycle, `ElementHost`, and a flat element-spec parser.
- **Widgets** — buttons, check boxes, toggles, radio buttons, segmented controls, sliders, progress,
  spinners, number/search/text inputs, list/table/tree views, recycling lists, menus, dialogs,
  popups, toasts, tooltips, colour pickers, dock/split/tab layouts and a client-side title bar.
- **Text** — UAX #9 bidi, UAX #14 line breaking, UAX #29 grapheme/word boundaries, hyphenation,
  justify/vertical/RTL text, rich text, DPI-aware subpixel rendering, font fallback, IME and
  grapheme-aware editing.
- **Windowing** — native Win32, Linux X11/Wayland and macOS Cocoa windows, fractional/high-DPI
  scaling, multi-window `Application`/`WindowManager`, layout capture/restore, same-thread and
  threaded render hosts, device recovery, snapshots and frame statistics.
- **Platform integration** — multi-MIME clipboard (text/HTML/PNG/files, PRIMARY), drag and drop,
  native file dialogs, IME and system appearance.
- **Styling and motion** — theme tokens, theme files with hot reload, light/dark, control states and
  styles, curves/springs, implicit transitions and shared-element transitions.
- **i18n and accessibility** — translation catalogs with plurals, number/date formatting; Windows UI
  Automation and Linux AT-SPI.
- **Verification** — headless rendering, snapshot and golden-image tests, and the `examples/` demo
  checks.

## Supported platforms

| Capability | Windows | Linux X11 | Linux Wayland | macOS |
|---|---|---|---|---|
| Native window and GPU surface | Yes | Yes | Yes | Yes, titled `NSWindow` + `CAMetalLayer` content view |
| High-DPI scaling | Per-monitor v2 | Global DPI | Per-output scale, including fractional | Backing scale, per screen |
| IME | IMM32 | IBus/XIM | `text-input` v1/v3 | `NSTextInputClient` (marked text, commit, candidates) |
| Clipboard | Multi-MIME, files, PNG | Multi-MIME, PRIMARY, INCR | Per-MIME, PRIMARY | Multi-MIME, HTML, PNG, file URLs |
| Drag and drop | OLE | XDND | Not yet | `NSDraggingSource` / destination |
| File dialogs | Native | Portal, `zenity`/`kdialog` | Portal | `NSOpenPanel`/`NSSavePanel` |
| Native alerts | `TaskDialog` | In-window `Dialog` | In-window `Dialog` | `NSAlert` |
| Native control hosting | `NativeViewLayer` child HWND | Not yet | Not yet | `NativeViewLayer` NSView |
| Native chrome | Custom | Custom | Custom / server-side | Toolbar, sidebar/outline, titlebar accessory, native menus |
| Accessibility | UI Automation | AT-SPI | AT-SPI | `NSAccessibility` tree, actions, text, grid, hit test |

Headless and software-raster targets work on every platform and are what the tests and examples
use. The macOS window backend presents through Metal using the same `graphics::gpu::WindowSurface`
path as the other native backends; Android has no native window backend yet.

## Building standalone

The framework builds against CoralCore and CoralGraphics. A standalone clone carries them as the
`external/` git submodules, plus the shared `libz`/`libyuv`/`libgtest` submodules under `third_party/`;
initialize them recursively. A checkout inside the application's `frameworks/` tree sits beside
CoralCore and CoralGraphics, so it reuses those sibling modules and the application's third-party
targets instead of initializing another copy. GoogleTest is needed only when the tests are built:

```sh
git submodule update --init --recursive

# Windows: one command. The script finds CMake in the Visual Studio installation, picks the newest
# installed Visual Studio generator, resolves the shared libz/libyuv/libgtest tree and runs the
# suites. Use scripts\build-windows.cmd (the .ps1 needs an execution policy that allows local
# scripts).
scripts\build-windows.cmd -Tests

# macOS (Command Line Tools; Ninja is optional)
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

For a manual Windows configure, run this from a Visual Studio Developer PowerShell. Use the
generator that matches the installed toolset (`Visual Studio 18 2026` or `Visual Studio 17 2022`),
and name the shared third-party tree when this checkout does not carry `third_party/` itself:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 `
    -DCORAL_SHARED_THIRD_PARTY_DIR=<third_party>
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Useful switches:

| Option | Default | Effect |
|---|---|---|
| `CORAL_BUILD_TESTS` | `ON` standalone, `OFF` as a subproject | Build the unit suites |
| `CORAL_TEST_DATA_DIR` | `examples/` (bundled fonts) standalone; the application's assets as a subproject | Reference fonts and fixtures for the tests and benchmarks; the examples bundle their own |
| `CORAL_SHARED_THIRD_PARTY_DIR` | this repository's `third_party` | Point at another shared `libz`/`libyuv`/`libgtest` tree |
| `CORAL_CORE_DIR` / `CORAL_GRAPHICS_DIR` | `external/`, then `../CoralCore` / `../CoralGraphics` | Point at explicit framework checkouts |

## The examples

The demos live in `examples/`. A top-level configure of this root adds them, so one build produces
the libraries, their tests and the example checks. The project is also configurable on its own,
which keeps the framework build to libraries and tests. The reference fonts are bundled under
`examples/fonts/`, so this project needs no external assets:

```sh
# Windows
cmake -S examples -B out/examples -G "Visual Studio 17 2022" -A x64
cmake --build out/examples --config Release
ctest --test-dir out/examples -C Release -L examples --output-on-failure

# macOS
cmake -S examples -B out/examples -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build out/examples -j8
ctest --test-dir out/examples -L examples --output-on-failure
```

The examples are built only for a standalone checkout; an application that embeds CoralUI as a
subproject builds its own products.

## Using it as a subproject

```cmake
add_subdirectory(CoralCore)       # provides utils and minilog
add_subdirectory(CoralGraphics)   # provides graphics and its third-party targets
add_subdirectory(CoralUI)         # provides mvvm and ui

target_link_libraries(my_app PRIVATE ui)    # add mvvm for <ui/mvvm/...>
```

As a subproject the framework builds no examples and no tests unless the caller sets
`CORAL_BUILD_TESTS`.

## License

Apache License 2.0 — see [LICENSE](LICENSE).
