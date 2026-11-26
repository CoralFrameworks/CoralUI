# ==================================================================================================
# CoralUI examples - the UI demos
# ==================================================================================================
# `ui_demo` is the manual smoke test for the window, layout, input and present path: a native window,
# or `--headless` for an offscreen surface, with a sectioned control gallery, a scrollable sidebar and
# a control page on the right. `ui_mvvm_demo` is the reference page for the declarative slice: a
# view model in a `DataContext`, an immutable `Element` tree and the `Reconciler` patching the layer
# tree.
#
# `ui_demo` is also the visual catalog for the cross-platform control set.
# All examples are clients of the public API only and can write a PNG, which is what the headless
# checks below run: no display, no input, just create -> render -> snapshot -> encode.

# The reference fonts are part of this project: CORAL_ASSETS_DIR is the examples/ directory, which
# contains fonts/. Nothing outside the repository is needed to build or run the checks.
set(CORALUI_EXAMPLE_ASSETS "${CMAKE_CURRENT_LIST_DIR}/..")

add_executable(ui_demo ui_demo.cpp gallery_sections.cpp)
target_link_libraries(ui_demo PRIVATE ui mvvm minilog)
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

  # The sandboxed app must read its reference font from inside the bundle.
  add_custom_command(TARGET ui_demo POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_BUNDLE_CONTENT_DIR:ui_demo>/Resources/fonts"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CORALUI_EXAMPLE_ASSETS}/fonts/NotoSansSC-Regular.otf"
        "$<TARGET_BUNDLE_CONTENT_DIR:ui_demo>/Resources/fonts/NotoSansSC-Regular.otf"
      COMMENT "Bundling the reference font into ui_demo.app"
      VERBATIM)

  # Distribution hook: an identity from the login keychain or a Developer ID certificate signs the
  # bundle after it is built. An empty identity keeps the default unsigned development build.
  set(CORAL_MACOS_CODE_SIGN_IDENTITY "" CACHE STRING
      "macOS code-signing identity for ui_demo ('-' for ad-hoc); empty leaves it unsigned")
  option(CORAL_MACOS_HARDENED_RUNTIME "Sign ui_demo with the hardened runtime" ON)
  set(CORAL_MACOS_ENTITLEMENTS "" CACHE FILEPATH
      "Optional entitlements plist used when signing ui_demo")
  if (CORAL_MACOS_CODE_SIGN_IDENTITY)
    set(_coral_sign_args --force --sign "${CORAL_MACOS_CODE_SIGN_IDENTITY}")
    if (CORAL_MACOS_CODE_SIGN_IDENTITY STREQUAL "-")
      list(APPEND _coral_sign_args --timestamp=none)
    else ()
      list(APPEND _coral_sign_args --timestamp)
    endif ()
    if (CORAL_MACOS_HARDENED_RUNTIME)
      list(APPEND _coral_sign_args --options runtime)
    endif ()
    if (CORAL_MACOS_ENTITLEMENTS)
      list(APPEND _coral_sign_args --entitlements "${CORAL_MACOS_ENTITLEMENTS}")
    endif ()
    add_custom_command(TARGET ui_demo POST_BUILD
        COMMAND codesign ${_coral_sign_args} "$<TARGET_BUNDLE_DIR:ui_demo>"
        COMMENT "Signing ui_demo.app with ${CORAL_MACOS_CODE_SIGN_IDENTITY}"
        VERBATIM)
  endif ()
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
    COMMAND $<TARGET_FILE:ui_demo> --headless --frames 2 --width 960 --height 700
        --out "${CMAKE_CURRENT_BINARY_DIR}/ui_demo.png")
set_tests_properties(example_ui_demo_headless PROPERTIES LABELS examples)

add_test(NAME example_ui_demo_light_headless
    COMMAND $<TARGET_FILE:ui_demo> --headless --frames 2 --width 960 --height 700
        --theme light --out "${CMAKE_CURRENT_BINARY_DIR}/ui_demo_light.png")
set_tests_properties(example_ui_demo_light_headless PROPERTIES LABELS examples)

# One synthetic click on the sidebar skin button: the gallery must replace the window theme,
# rebuild the current page against it, and still render a valid frame.
add_test(NAME example_ui_demo_theme_toggle_headless
    COMMAND $<TARGET_FILE:ui_demo> --headless --frames 2 --width 960 --height 700
        --click-x 100 --click-y 79 --out "${CMAKE_CURRENT_BINARY_DIR}/ui_demo_theme_toggle.png")
set_tests_properties(example_ui_demo_theme_toggle_headless PROPERTIES LABELS examples)

add_test(NAME example_ui_demo_lists_headless
    COMMAND $<TARGET_FILE:ui_demo> --headless --frames 2 --width 960 --height 700 --section 6)
set_tests_properties(example_ui_demo_lists_headless PROPERTIES LABELS examples)

add_test(NAME example_ui_demo_control_headless
    COMMAND $<TARGET_FILE:ui_demo> --headless --frames 2 --width 960 --height 700 --control Banner)
set_tests_properties(example_ui_demo_control_headless PROPERTIES LABELS examples)

add_test(NAME example_ui_demo_more_headless
    COMMAND $<TARGET_FILE:ui_demo> --headless --frames 2 --width 960 --height 700 --section 8)
set_tests_properties(example_ui_demo_more_headless PROPERTIES LABELS examples)

add_test(NAME example_ui_demo_native_headless
    COMMAND $<TARGET_FILE:ui_demo> --headless --frames 2 --width 960 --height 700 --section 7)
set_tests_properties(example_ui_demo_native_headless PROPERTIES LABELS examples)

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
