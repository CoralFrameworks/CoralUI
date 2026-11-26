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

## Two ways to build a UI

CoralUI keeps the retained layer tree as the primitive and offers a declarative `Element` layer on
top of it. Both styles are public API and can be mixed in one window; neither is a compatibility
shim.

**Traditional: build and update layers directly.** You own the rectangles and the calls that change
them. This is the right path for custom drawing, precise layout control and the largest virtualized
surfaces.

```cpp
using namespace coral::ui;

auto page = std::make_shared<Layer>();
page->setBackgroundColorRole(ThemeColorRole::Background);

auto title = std::make_shared<TextLayer>("Library", window->theme().title, window->theme().text);
title->setBounds(Rect::MakeXYWH(24.0f, 20.0f, 320.0f, 32.0f));
page->addChild(title);

auto search = std::make_shared<TextInput>("Search", window->theme().body);
search->setBounds(Rect::MakeXYWH(24.0f, 64.0f, 320.0f, 32.0f));
search->observeChanged([&](const std::string& value) { model.setQuery(value); });
page->addChild(search);

window->setRoot(page);
```

**Declarative: describe the tree, let the reconciler patch it.** A `State<T>` (or an
`observer::Property` / `mvvm::Property`) drives a build function; `ElementHost` coalesces writes and
`Reconciler` matches `(kind, key)` so layer identity, focus, scroll position and animations survive
the rebuild.

```cpp
using namespace coral::ui;

State<int> selected{0};
ElementHost host(*window, [&] {
  const Theme& theme = window->theme();
  std::vector<ElementPtr> rows;
  for (size_t i = 0; i < model.items().size(); ++i) {
    rows.push_back(components::WithProps(
      components::Button(model.items()[i], theme.body,
        [&, i] { selected.set(static_cast<int>(i)); }, "row-" + std::to_string(i)),
      [] { ElementProps p; p.layout.basis = 32.0f; p.layout.shrink = 0.0f; return p; }(),
      "row-" + std::to_string(i) + ".wrap"));
  }
  return std::make_shared<StackElement>(Axis::Column, std::vector<ElementPtr>{
    std::make_shared<TextElement>("Library", theme.title, theme.text, "title"),
    components::TextInput(model.query(), theme.body,
      [&](const std::string& value) { model.setQuery(value); }, "search"),
    std::make_shared<TextElement>(std::to_string(selected.get()) + " selected",
      theme.body, theme.textMuted, "selection"),
    std::make_shared<StackElement>(Axis::Column, std::move(rows), "rows", 6.0f,
      Insets{}.all(24.0f), Align::Stretch),
  }, "page", 12.0f, Insets{}.all(24.0f), Align::Stretch);
});
auto binding = host.bind(selected.property());   // keep the observation alive
host.mount();
window->setRoot(host.root());
```

`ComponentHost` adds component lifecycle, an error boundary and `provide`/`find` dependency
injection; `ElementRegistry` registers an application type name; `ElementSpec` parses a flat text
form. The gallery example is built entirely in this style.

**Which one?**

| Need | Use |
|---|---|
| Canvas, waveform, timeline, custom `drawSelf` | Retained layers |
| Exact measure/layout for a container, pixel-level benchmarks | Retained layers |
| Forms, settings pages, lists, dialogs driven by a model | Declarative elements |
| Keyed reorder, `When` branches, component state | Declarative elements |
| A widget with no wrapper yet inside an element page | `components::Layer` (escape hatch) |

**Coverage, as of this checkout:** 80 of the 89 public `Layer` types have a first-class element
wrapper (text, styled text, colour, layout/form controls, display and data leaves, interactive
widgets, selection groups, content/structured containers, editor/date-time fields and the tree view).
The other 9 are reachable from an element tree through
`components::Layer`, but the layer owns its state. The complete matrix, the `ElementSpec` grammar,
the wrapper checklist and the list of gaps are in
[docs/declarative-components.md](docs/declarative-components.md).

## Features

- **Retained layer tree** — bounds, clipping, z-order, transforms, opacity, blend modes, rounded and
  path clips, shadows, hit testing and focus; layout recomputed only when invalidated.
- **Layout** — `Flex` (grow/shrink/basis, percent, margins, min/max, alignment, wrapping, justify),
  `Grid`, `ScrollView`, `SplitView`, `DockView`, `TabBar`, `Overlay`, `ContentView`.
- **Declarative UI** — immutable `Element` tree, keyed `Reconciler` patching, `OverlayHost`, `Component` state and
  lifecycle, `ElementHost`, `State<T>`, a flat element-spec parser and a registry for application
  types; 80 first-class control wrappers (see [docs/declarative-components.md](docs/declarative-components.md)).
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
