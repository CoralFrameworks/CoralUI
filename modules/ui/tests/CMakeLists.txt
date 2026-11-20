add_executable(test_ui
    test_main.cpp
    test_Layout.cpp
    test_Window.cpp
    test_RasterRenderer.cpp
    test_Input.cpp
    test_Content.cpp
    test_Host.cpp
    test_MvvmBinding.cpp
    test_Reconciler.cpp
test_Retained.cpp
    test_ElementSpec.cpp
    test_NumberFormat.cpp
    test_DateFormat.cpp
    test_RecyclingListView.cpp
    test_M3.cpp
    test_M4.cpp
    test_DPI_RichText.cpp
    test_TextLayoutOptions.cpp
    test_SystemFonts.cpp
    test_TextPolicy.cpp
    test_TextVisual.cpp
    test_TextEditing.cpp
    test_RichTextVisual.cpp
    test_Binding.cpp
    test_ClipboardMime.cpp
    test_DragOut.cpp
    test_Focus.cpp
    test_LinuxIme.cpp
    test_Controls.cpp
    test_Style.cpp
    test_TitleBar.cpp
    test_Component.cpp
    test_Animation.cpp
    test_Splitters.cpp
    test_Golden.cpp
    test_GoldenScenes.cpp
    unicode/LineBreakTest.cpp
    unicode/test_BidiConformance.cpp
    unicode/test_BidiProperties.cpp
    unicode/test_GraphemeBreak.cpp
    unicode/test_Hyphenation.cpp
    unicode/test_LineBreakDfa.cpp
    unicode/test_LineBreakPolicy.cpp
    unicode/test_WordBreak.cpp
    unicode/test_LineBreakTest.cpp
    unicode/test_LineBreakProperties.cpp
)

target_link_libraries(test_ui PRIVATE ui mvvm gtest)
if (NOT CORAL_ASSETS_DIR)
  set(CORAL_ASSETS_DIR "${CORAL_TEST_DATA_DIR}")
endif ()
target_compile_definitions(test_ui PRIVATE CORAL_ASSETS_DIR="${CORAL_ASSETS_DIR}")
target_compile_definitions(test_ui PRIVATE CORAL_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(test_ui PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
  ${CMAKE_CURRENT_SOURCE_DIR}/unicode
)
target_compile_definitions(test_ui PRIVATE
  CORAL_UNICODE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../tools/unicode/data"
  CORAL_UNICODE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/unicode"
)

add_test(NAME test_ui COMMAND test_ui)
# The shared-device native-window case gets a fresh process. An X11 resize test that ran earlier in
# the same process can leave the thread's GL/EGL binding in a state the next Wayland device cannot
# acquire from; a process boundary is the isolation the platform actually guarantees.
set_tests_properties(test_ui PROPERTIES ENVIRONMENT
  "GTEST_FILTER=-UiWindow.AnApplicationSharesTheDeviceBetweenNativeWindows")
add_test(NAME test_ui_shared_device COMMAND test_ui
  --gtest_filter=UiWindow.AnApplicationSharesTheDeviceBetweenNativeWindows)
add_test(NAME unicode_perf_gate COMMAND test_ui
  --gtest_filter=UnicodeLineBreakPerformanceGate.*)
set_tests_properties(unicode_perf_gate PROPERTIES LABELS unicode)

# Native-window smoke: unlike the gtest cases, a session without a desktop is reported as a CTest
# skip (77) instead of a silently passing test. CI runs this separately so a platform regression is
# visible rather than hidden behind "no desktop on the runner".
add_executable(ui_native_smoke ui_native_smoke.cpp)
target_link_libraries(ui_native_smoke PRIVATE ui)
add_test(NAME ui_native_smoke COMMAND ui_native_smoke)
set_tests_properties(ui_native_smoke PROPERTIES LABELS native SKIP_RETURN_CODE 77)

if(WIN32)
  add_executable(ui_multimonitor_smoke ui_multimonitor_smoke.cpp)
  target_link_libraries(ui_multimonitor_smoke PRIVATE ui)
  add_test(NAME ui_multimonitor_smoke COMMAND ui_multimonitor_smoke)
  set_tests_properties(ui_multimonitor_smoke PROPERTIES LABELS native SKIP_RETURN_CODE 77)

  # The unhandled-exception filter only does its job when a process actually dies: the smoke spawns a
  # child that raises an access violation and checks the minidump and text report it leaves behind.
  add_executable(crash_windows_smoke crash_windows_smoke.cpp)
  target_link_libraries(crash_windows_smoke PRIVATE ui)
  add_test(NAME crash_windows_smoke COMMAND crash_windows_smoke
    "${CMAKE_CURRENT_BINARY_DIR}/crash-windows-smoke")
  set_tests_properties(crash_windows_smoke PROPERTIES LABELS crash SKIP_RETURN_CODE 77)
endif()

# X11 clipboard smoke: multiple MIME representations over a real selection need a display and a live
# owner, which the headless suites do not have. A separate executable reports 77 (skip) without one;
# CI runs it under `xvfb-run`.
if (UNIX AND NOT APPLE)
  find_package(X11 QUIET)
  if (X11_FOUND)
    add_executable(x11_clipboard_smoke x11_clipboard_smoke.cpp)
    target_link_libraries(x11_clipboard_smoke PRIVATE ui)
    target_include_directories(x11_clipboard_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
    if (TARGET X11::Xfixes)
      # The test asserts push notification only when the backend was built with XFixes; otherwise it
      # reports the owner-polling fallback and skips that check.
      target_compile_definitions(x11_clipboard_smoke PRIVATE CORAL_UI_TEST_XFIXES=1)
    endif ()
    add_test(NAME x11_clipboard_smoke COMMAND x11_clipboard_smoke)
    set_tests_properties(x11_clipboard_smoke PROPERTIES LABELS native SKIP_RETURN_CODE 77)

    # XDND drag-out: the source blocks on a pointer grab, so it needs a second X client as the drop
    # target. Same skip rule as the clipboard smoke; CI runs both under one `xvfb-run`.
    add_executable(x11_drag_smoke x11_drag_smoke.cpp)
    target_link_libraries(x11_drag_smoke PRIVATE ui)
    target_include_directories(x11_drag_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
    add_test(NAME x11_drag_smoke COMMAND x11_drag_smoke)
    set_tests_properties(x11_drag_smoke PROPERTIES LABELS native SKIP_RETURN_CODE 77)
  endif ()
endif()

# Reproducibility gate: the committed generated tables must match a fresh run.
find_package(Python3 COMPONENTS Interpreter QUIET)
if (Python3_Interpreter_FOUND)
  add_test(NAME unicode_generate_check
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../tools/unicode/generate_linebreak_properties.py --check --quiet)
  set_tests_properties(unicode_generate_check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME unicode_dfa_generate_check
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../tools/unicode/generate_uax14_dfa.py --check --quiet)
  set_tests_properties(unicode_dfa_generate_check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME unicode_bidi_generate_check
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../tools/unicode/generate_bidi_properties.py --check --quiet)
  set_tests_properties(unicode_bidi_generate_check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME unicode_bidi_brackets_check
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../tools/unicode/generate_bidi_brackets.py --check --quiet)
  set_tests_properties(unicode_bidi_brackets_check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME unicode_bidi_mirroring_check
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../tools/unicode/generate_bidi_mirroring.py --check --quiet)
  set_tests_properties(unicode_bidi_mirroring_check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME unicode_word_tries_check
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../tools/unicode/generate_word_tries.py --check --quiet)
  set_tests_properties(unicode_word_tries_check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME unicode_grapheme_generate_check
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../tools/unicode/generate_grapheme_properties.py --check --quiet)
  set_tests_properties(unicode_grapheme_generate_check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME unicode_hyphenation_generate_check
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../tools/unicode/generate_hyphenation.py --check --quiet)
  set_tests_properties(unicode_hyphenation_generate_check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME unicode_wordbreak_generate_check
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/../tools/unicode/generate_wordbreak_properties.py --check --quiet)
  set_tests_properties(unicode_wordbreak_generate_check PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif ()

# POSIX crash handler: the handler only runs in a process that is dying, so the test forks a child,
# raises SIGSEGV there and inspects the report the handler wrote. Registered on every POSIX host; the
# harness reports 77 when fork is unavailable, so it stays a skip rather than a failure.
if (UNIX AND NOT APPLE)
  add_executable(crash_posix_smoke crash_posix_smoke.cpp)
  target_link_libraries(crash_posix_smoke PRIVATE ui)
  add_test(NAME crash_posix_smoke COMMAND crash_posix_smoke)
  set_tests_properties(crash_posix_smoke PROPERTIES LABELS native SKIP_RETURN_CODE 77)
endif ()
