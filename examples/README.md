# CoralUI examples

Demo and verification programs built as a client of the framework: they include `<ui/...>`,
`<mvvm/...>` and `<graphics/...>` and nothing private. The project configures in two modes: on its own
(shown below), or as a subdirectory of the framework root, which adds it when the root is top-level.
An application that consumes CoralUI as a submodule gets libraries and tests only; the examples are
what a standalone checkout uses to verify the UI framework end to end.

```
examples/
├── CMakeLists.txt        # the project: dependencies, then the framework, then the examples
├── fonts/                # the reference fonts the demos use (bundled, no external assets needed)
└── ui/
    ├── CMakeLists.txt    # the executables and their headless CTest checks
    ├── gallery_sections.cpp # sectioned gallery pages
    ├── ui_demo.cpp       # gallery + window/layout/input/AX/IME acceptance app
    └── ui_mvvm_demo.cpp  # DataContext + Element/Reconciler reference page, headless
```

The cross-platform control inventory and accessibility/style contracts are in
[`docs/cross-platform-controls.md`](../docs/cross-platform-controls.md).

## Configuring

The framework's dependencies are git submodules, so initialize them from the repository root first:

```sh
git submodule update --init --recursive
```

Then configure either the examples project or the framework root; both build the examples:

```sh
# Windows: one command for configure + build + the headless checks. It finds CMake and the newest
# Visual Studio generator and resolves the shared libz/libyuv/libgtest tree; -Tests also builds the
# framework unit suites.
scripts\build-windows.cmd

# Or the examples project by hand (run from a Visual Studio Developer PowerShell). Name the shared
# third-party tree when this checkout does not carry third_party/ itself.
cmake -S examples -B out/examples -G "Visual Studio 18 2026" -A x64 `
    -DCORAL_SHARED_THIRD_PARTY_DIR=<third_party>
cmake --build out/examples --config Release

# Or the framework root: libraries, tests and examples in one tree
cmake -S . -B out/ui -G "Visual Studio 18 2026" -A x64 `
    -DCORAL_SHARED_THIRD_PARTY_DIR=<third_party>
cmake --build out/ui --config Release

# macOS: the stand-alone examples project
cmake -S examples -B out/examples -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build out/examples -j8

# macOS: the framework root
cmake -S . -B out/ui -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build out/ui -j8
```

The examples project configures the framework with `CORAL_BUILD_TESTS=OFF`: it links `ui`/`mvvm` but
does not build or run the unit suites, so GoogleTest is not required.

The reference fonts are bundled under `examples/fonts/`, and the executables read `CORAL_ASSETS_DIR`
pointed at this directory, so nothing outside the repository is needed. The framework dependencies
come from `external/` when the submodules are initialized, or from the sibling CoralCore/CoralGraphics
checkouts in the frameworks/workgroup layout.

## Building and running

```sh
# Windows
cmake --build out/examples --config Release
out/examples/ui/Release/ui_demo --headless --frames 2 --out out/ui_demo.png
# Native Win32 window; --width/--height are logical units, scaled by the monitor DPI
out/examples/ui/Release/ui_demo --frames 3 --width 960 --height 700 --out out/native.png

# Linux single-config generators put the executables under out/examples/ui/
cmake --build out/examples -j8
out/examples/ui/ui_demo --headless --frames 2 --out out/ui_demo.png

# macOS builds ui_demo as an application bundle (Finder launch, Dock identity, Info.plist).
# The executable inside it takes the same arguments:
out/examples/ui/ui_demo.app/Contents/MacOS/ui_demo --headless --frames 2 --out out/ui_demo.png
open out/examples/ui/ui_demo.app
out/examples/ui/ui_demo.app/Contents/MacOS/ui_demo --frames 0 --width 960 --height 700
# Pick a gallery section at launch (0 Inputs, 1 Selection, ... 6 Lists, 7 Native, 8 More)
out/examples/ui/ui_demo.app/Contents/MacOS/ui_demo --frames 0 --section 6
# Open one control's standalone demo (section is inferred from the control row)
out/examples/ui/ui_demo.app/Contents/MacOS/ui_demo --frames 0 --control SplitButton

# Manual macOS acceptance modes
open out/examples/ui/ui_demo.app --args --toolbar
open out/examples/ui/ui_demo.app --args --toolbar --native-disabled
open out/examples/ui/ui_demo.app --args --ime-demo

# Linux native window (the macOS equivalent is the bundle executable above)
out/examples/ui/ui_demo --frames 0 --width 960 --height 700

# The declarative reference page (headless by default)
out/examples/ui/ui_mvvm_demo --out out/ui_mvvm_demo.png --frames 1
```

`ui_demo` also accepts `--bookmark-panel` (NSOpenPanel + security-scoped bookmark),
`--bookmark-smoke` (container-local bookmark smoke; both are macOS-only and report an error on other
platforms), `--platform auto|cocoa|x11|wayland`,
`--render-host same-thread|render-thread`, `--theme dark|light`, `--scale S`, `--focus-input`,
`--section N` and `--frames N` (section 8 is the additional-controls page). The left gallery list is a `RecyclingListView` with expandable
sections and selectable control rows; a row opens that control's standalone demo with an MVVM event/value readout, while its section
header opens the group page. `--control NAME` opens a control demo directly. Section 6 (`Lists`)
contains an inner `ScrollView` to exercise nested wheel routing. `--frames 0` keeps a native window open until it is closed. On macOS the window opens
at the requested point size (a 960x540 request is a 960x540-point window, 1920x1080 backing pixels on
a Retina display) and presents through Metal. Windows and Linux size native windows in device
pixels, so `ui_demo` scales `--width`/`--height` by the monitor DPI after creating the window: a
960x700 request is a 960x700-logical window, backed by 1440x1050 pixels on a 150% display.

## macOS signing

`scripts/check-macos-release.sh` verifies the Developer ID/runtime/universal/Gatekeeper prerequisites
before notarization. `scripts/notarize-macos-app.sh` submits a Developer ID-signed bundle to notarytool.
`scripts/check-macos-sandbox.sh` signs the demo with the sandbox entitlement template and verifies
the embedded entitlements. The reference font is bundled under `Contents/Resources/fonts`, so a
sandboxed headless launch works; `ui_demo --bookmark-smoke` exercises a container-local
security-scoped bookmark, while user-selected file access still needs a real NSOpenPanel.

## Verification

`enable_testing()` is called and the checks that do not need a person looking at a window are
registered:

| Test | What it proves |
|---|---|
| `example_ui_demo_light_headless` | the gallery renders with the light theme |
| `example_ui_demo_headless` | the sectioned gallery lays out and snapshots the Inputs page |
| `example_ui_demo_lists_headless` | the Lists page renders the outer and nested ScrollViews |
| `example_ui_demo_control_headless` | a standalone control demo renders with its accent frame |
| `example_ui_demo_native_headless` | the Native page renders its fallback text offscreen (native children are not in the GPU frame) |
| `example_ui_demo_more_headless` | the More page renders additional framework controls |
| `example_ui_mvvm_headless` | the Element/Reconciler page mounts, patches from a Command and renders |

The native window path is the manual check; `ui_demo --frames N --out frame.png` also snapshots the
presented GPU frame, so a macOS or Windows run can be verified without a screenshot. A snapshot does
not include platform-native children (`NSView`/child HWND); the Native gallery page labels that block
as live native only.

The two programs have different jobs and are intentionally kept separate:

- `ui_demo` is the macOS bundle/acceptance app and the cross-platform control gallery. It owns the
  `.app`, Info.plist, signing/entitlement options, `--platform`/`--render-host`/`--theme` flags,
  the manual AX/IME/window checks and the visual control catalog.
- `ui_mvvm_demo` is the DataContext/Element/Reconciler reference page.

Deleting `ui_mvvm_demo` would remove the MVVM reference path; the gallery is part of `ui_demo`.

```sh
ctest --test-dir out/examples -C Release -L examples --output-on-failure
```

Each check runs the example with `--headless --out <build>/.../*.png`; a failed window creation,
frame render, snapshot or PNG encode is a non-zero exit. No display or GPU window is needed — the
headless path creates an offscreen surface.
