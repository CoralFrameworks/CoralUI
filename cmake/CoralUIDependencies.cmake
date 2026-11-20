# ==================================================================================================
# CoralUI dependencies
# ==================================================================================================
# Resolves every target `modules/mvvm` and `modules/ui` name in their link lines, so this repository can
# be configured on its own:
#
#   cmake -S . -B build                  # the framework: libraries + tests
#   cmake -S examples -B build/examples  # the demos
#
# CoralUI has two checkouts, and both are supported here:
#
#   - a standalone clone carries the frameworks as submodules under external/;
#   - a checkout inside the application's frameworks/ tree sits beside CoralCore and CoralGraphics, so
#     it reuses those sibling modules instead of initializing another copy.
#
# The application root adds its own dependency graph before CoralUI, and every `add_subdirectory` below
# is guarded by `NOT TARGET`, so including this file in that tree is a no-op.
#
# What comes from where:
#
#   external/CoralCore    (or ../CoralCore)      utils, minilog, and the foundation framework
#   external/CoralGraphics(or ../CoralGraphics)  graphics and its third-party graph
#   third_party/ in this repository              the shared libz, libyuv and libgtest submodules
#
# A standalone clone initializes the submodules once:
#
#   git submodule update --init --recursive

include_guard(GLOBAL)

get_filename_component(CORALUI_REPO_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# ==================================================================================================
# CoralCore
# ==================================================================================================
# A standalone clone carries it as `external/CoralCore`; a checkout beside the other frameworks (the
# application's frameworks/ tree, or the workgroup layout) keeps it as `../CoralCore`. The directory is
# resolved here but added after GoogleTest, so that this repository's bundled gtest wins over
# CoralCore's copy (CoralCore's CMakeLists is guarded by `NOT TARGET gtest` too).
set(_coral_ui_core_dir "")
if (NOT TARGET utils)
  if (DEFINED CORAL_CORE_DIR AND EXISTS "${CORAL_CORE_DIR}/CMakeLists.txt")
    set(_coral_ui_core_dir "${CORAL_CORE_DIR}")
  elseif (EXISTS "${CORALUI_REPO_DIR}/external/CoralCore/CMakeLists.txt")
    set(_coral_ui_core_dir "${CORALUI_REPO_DIR}/external/CoralCore")
  elseif (EXISTS "${CORALUI_REPO_DIR}/../CoralCore/CMakeLists.txt")
    set(_coral_ui_core_dir "${CORALUI_REPO_DIR}/../CoralCore")
  else ()
    message(FATAL_ERROR
        "CoralUI needs CoralCore for its `utils` and `minilog` targets. Initialize the submodule with "
        "'git submodule update --init --recursive external/CoralCore', keep a sibling ../CoralCore "
        "checkout, pass -DCORAL_CORE_DIR=<path>, or add the CoralCore targets before CoralUI.")
  endif ()
endif ()

# ==================================================================================================
# CoralGraphics
# ==================================================================================================
# Same resolution as CoralCore. CoralUI does not use CoralGraphics' optional effect module, so a
# configure that adds the framework leaves it off unless the caller chose otherwise.
set(_coral_ui_graphics_dir "")
if (NOT TARGET graphics)
  if (DEFINED CORAL_GRAPHICS_DIR AND EXISTS "${CORAL_GRAPHICS_DIR}/CMakeLists.txt")
    set(_coral_ui_graphics_dir "${CORAL_GRAPHICS_DIR}")
  elseif (EXISTS "${CORALUI_REPO_DIR}/external/CoralGraphics/CMakeLists.txt")
    set(_coral_ui_graphics_dir "${CORALUI_REPO_DIR}/external/CoralGraphics")
  elseif (EXISTS "${CORALUI_REPO_DIR}/../CoralGraphics/CMakeLists.txt")
    set(_coral_ui_graphics_dir "${CORALUI_REPO_DIR}/../CoralGraphics")
  else ()
    message(FATAL_ERROR
        "CoralUI needs CoralGraphics. Initialize the submodule with "
        "'git submodule update --init --recursive external/CoralGraphics', keep a sibling "
        "../CoralGraphics checkout, pass -DCORAL_GRAPHICS_DIR=<path>, or add the CoralGraphics targets "
        "before CoralUI.")
  endif ()

  if (NOT DEFINED CORAL_GRAPHICS_WITH_EFFECT)
    set(CORAL_GRAPHICS_WITH_EFFECT OFF)
  endif ()
endif ()

# ==================================================================================================
# Shared third-party tree
# ==================================================================================================
# libz, libyuv and libgtest are shared with the other frameworks. A standalone clone carries its own
# pinned copies under third_party/; a tree beside the application uses the application's targets, and
# CORAL_SHARED_THIRD_PARTY_DIR remains an explicit override.
if (NOT DEFINED CORAL_SHARED_THIRD_PARTY_DIR AND NOT TARGET z AND
    EXISTS "${CORALUI_REPO_DIR}/third_party/libz/coral/CMakeLists.txt")
  set(CORAL_SHARED_THIRD_PARTY_DIR "${CORALUI_REPO_DIR}/third_party")
endif ()

# ==================================================================================================
# GoogleTest
# ==================================================================================================
# Only the test suites name it. An explicit CORAL_GTEST_DIR or shared tree wins; this repository's
# bundled submodule is the standalone default, and CoralCore's copy is the last fallback.
if (CORAL_BUILD_TESTS AND NOT TARGET gtest)
  if (DEFINED CORAL_GTEST_DIR AND EXISTS "${CORAL_GTEST_DIR}/CMakeLists.txt")
    add_subdirectory("${CORAL_GTEST_DIR}" "${CMAKE_BINARY_DIR}/gtest")
  elseif (DEFINED CORAL_SHARED_THIRD_PARTY_DIR AND
      EXISTS "${CORAL_SHARED_THIRD_PARTY_DIR}/libgtest/coral/CMakeLists.txt")
    add_subdirectory("${CORAL_SHARED_THIRD_PARTY_DIR}/libgtest/coral" "${CMAKE_BINARY_DIR}/gtest")
  elseif (EXISTS "${CORALUI_REPO_DIR}/third_party/libgtest/coral/CMakeLists.txt")
    add_subdirectory("${CORALUI_REPO_DIR}/third_party/libgtest/coral" "${CMAKE_BINARY_DIR}/gtest")
  elseif (DEFINED _coral_ui_core_dir AND
      EXISTS "${_coral_ui_core_dir}/third_party/libgtest/coral/CMakeLists.txt")
    add_subdirectory("${_coral_ui_core_dir}/third_party/libgtest/coral" "${CMAKE_BINARY_DIR}/gtest")
  endif ()

  if (NOT TARGET gtest)
    message(FATAL_ERROR
        "CoralUI tests need GoogleTest. Initialize the submodules with "
        "'git submodule update --init --recursive', pass -DCORAL_GTEST_DIR=<path>, point "
        "-DCORAL_SHARED_THIRD_PARTY_DIR at a tree with libgtest, or configure with "
        "-DCORAL_BUILD_TESTS=OFF.")
  endif ()
endif ()

# ==================================================================================================
# Adding the frameworks
# ==================================================================================================
if (NOT TARGET utils)
  add_subdirectory("${_coral_ui_core_dir}" "${CMAKE_BINARY_DIR}/coral-core")
endif ()

if (NOT TARGET graphics)
  add_subdirectory("${_coral_ui_graphics_dir}" "${CMAKE_BINARY_DIR}/coral-graphics")
endif ()
