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
    ├── ui_demo.cpp       # window/layout/input/present smoke test, native or --headless
    └── ui_mvvm_demo.cpp  # DataContext + Element/Reconciler reference page, headless
```

## Configuring

The framework's dependencies are git submodules, so initialize them from the repository root first:

```sh
git submodule update --init --recursive
```

Then configure either the examples project or the framework root; both build the examples:

```sh
# Windows: the standalone examples project (what an IDE opens when working on a demo)
cmake -S examples -B out/examples -G "Visual Studio 17 2022" -A x64

# Or the framework root: libraries, tests and examples in one tree
cmake -S . -B out/ui -G "Visual Studio 17 2022" -A x64
cmake --build out/ui

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

# macOS/Linux single-config generators put the executables under out/examples/ui/
cmake --build out/examples -j8
out/examples/ui/ui_demo --headless --frames 2 --out out/ui_demo.png

# Native window: Esc (or Cmd+W on macOS) closes it; the title bar exercises
# move/maximize/minimize/close
out/examples/ui/ui_demo --frames 0 --width 960 --height 540

# The declarative reference page (headless by default)
out/examples/ui/ui_mvvm_demo --out out/ui_mvvm_demo.png --frames 1
```

`ui_demo` also accepts `--platform auto|cocoa|x11|wayland`,
`--render-host same-thread|render-thread`, `--theme dark|light`, `--scale S`, `--focus-input` and
`--frames N`. `--frames 0` keeps a native window open until it is closed. On macOS the window opens
at the requested point size (a 960x540 request is a 960x540-point window, 1920x1080 backing pixels on
a Retina display) and presents through Metal.

## Verification

`enable_testing()` is called and the checks that do not need a person looking at a window are
registered:

| Test | What it proves |
|---|---|
| `example_ui_demo_headless` | an offscreen window lays out, renders and snapshots the layer showcase |
| `example_ui_mvvm_headless` | the Element/Reconciler page mounts, patches from a Command and renders |

The native window path is the manual check; `ui_demo --frames N --out frame.png` also snapshots the
presented frame, so a macOS or Windows run can be verified without a screenshot.

```sh
ctest --test-dir out/examples -C Release -L examples --output-on-failure
```

Each check runs the example with `--headless --out <build>/.../*.png`; a failed window creation,
frame render, snapshot or PNG encode is a non-zero exit. No display or GPU window is needed — the
headless path creates an offscreen surface.
