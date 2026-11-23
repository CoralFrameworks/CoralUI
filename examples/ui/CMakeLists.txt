# ==================================================================================================
# CoralUI examples - the UI demos
# ==================================================================================================
# `ui_demo` is the manual smoke test for the window, layout, input and present path: a native window,
# or `--headless` for an offscreen surface, with a flex column, solid panels, text, a focusable button,
# a text input and a scroll view. `ui_mvvm_demo` is the reference page for the declarative slice: a
# view model in a `DataContext`, an immutable `Element` tree and the `Reconciler` patching the layer
# tree.
#
# Both are clients of the public API only and both can write a PNG, which is what the headless checks
# below run: no display, no input, just create -> render -> snapshot -> encode.

# The reference fonts are part of this project: CORAL_ASSETS_DIR is the examples/ directory, which
# contains fonts/. Nothing outside the repository is needed to build or run the checks.
set(CORALUI_EXAMPLE_ASSETS "${CMAKE_CURRENT_LIST_DIR}/..")

add_executable(ui_demo ui_demo.cpp)
target_link_libraries(ui_demo PRIVATE ui minilog)
target_compile_definitions(ui_demo PRIVATE CORAL_ASSETS_DIR="${CORALUI_EXAMPLE_ASSETS}")

add_executable(ui_mvvm_demo ui_mvvm_demo.cpp)
target_link_libraries(ui_mvvm_demo PRIVATE ui mvvm minilog)
target_compile_definitions(ui_mvvm_demo PRIVATE CORAL_ASSETS_DIR="${CORALUI_EXAMPLE_ASSETS}")

# On macOS the interactive demo is a real application bundle: Finder launch, Dock identity, a bundle
# id and version. The headless checks still invoke the executable inside the bundle, so `ctest` needs
# no separate target.
if (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(_coral_demo_version "0.0.0")
  if (EXISTS "${CORALUI_EXAMPLE_ASSETS}/../VERSION")
    file(STRINGS "${CORALUI_EXAMPLE_ASSETS}/../VERSION" _coral_demo_version LIMIT_COUNT 1)
    string(STRIP "${_coral_demo_version}" _coral_demo_version)
  endif ()
  set_target_properties(ui_demo PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "Coral UI Demo"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.coral.ui.demo"
    MACOSX_BUNDLE_BUNDLE_VERSION "${_coral_demo_version}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${_coral_demo_version}"
  )
endif ()

foreach (example ui_demo ui_mvvm_demo)
  if (MSVC)
    target_compile_options(${example} PRIVATE /W4 /utf-8)
  else ()
    target_compile_options(${example} PRIVATE -Wall -Wextra)
  endif ()
  set_target_properties(${example} PROPERTIES FOLDER Examples)
endforeach ()

# The target file, not the bare name: with a multi-config generator the test has to run the executable
# this build produced.
add_test(NAME example_ui_demo_headless
    COMMAND $<TARGET_FILE:ui_demo> --headless --frames 2 --width 960 --height 540
        --out "${CMAKE_CURRENT_BINARY_DIR}/ui_demo.png")
set_tests_properties(example_ui_demo_headless PROPERTIES LABELS examples)

# The MVVM page measures and shapes real text, so it needs the reference font bundled under
# examples/fonts/. The executable still builds if the font is missing; only the check is left out.
if (EXISTS "${CORALUI_EXAMPLE_ASSETS}/fonts/NotoSansSC-Regular.otf")
  add_test(NAME example_ui_mvvm_headless
      COMMAND $<TARGET_FILE:ui_mvvm_demo> --frames 1
          --out "${CMAKE_CURRENT_BINARY_DIR}/ui_mvvm_demo.png")
  set_tests_properties(example_ui_mvvm_headless PROPERTIES LABELS examples)
else ()
  message(STATUS
      "CoralUI examples: the bundled reference font examples/fonts/NotoSansSC-Regular.otf is missing; "
      "the ui_mvvm_demo check is not registered")
endif ()
