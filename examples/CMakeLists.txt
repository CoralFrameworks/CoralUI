# ==================================================================================================
# CoralUI examples
# ==================================================================================================
# The framework's demo and verification programs, written as a client of the public API only: they
# include <ui/...>, <mvvm/...> and <graphics/...> and nothing private.
#
# Two ways to build them:
#
#   cmake -S examples -B out/examples ...   # standalone: dependencies, framework, then the examples
#   cmake -S . -B build ...                 # the framework root adds this directory when top-level
#
# An application that adds CoralUI as a subproject does not get example executables: the framework
# stays libraries plus tests. This keeps the repository usable both as a dependency and as a
# standalone project for working on the UI framework itself.
#
# The reference fonts ship with the examples under examples/fonts/, so the project is self-contained
# once the submodules are initialized. Its programs read CORAL_ASSETS_DIR, which points at this
# directory.
#
# `ctest` runs the headless checks: each example creates an offscreen window, renders, snapshots and
# encodes a PNG, so the window, layout, paint, text and image paths are verified without a display.

set(CORALUI_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  # Standalone. The framework and its dependencies have to exist before any example names them, the
  # same way the application root adds its dependency graph before its frameworks.
  cmake_minimum_required(VERSION 3.19)
  project(CoralUIExamples LANGUAGES C CXX)

  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

  include("${CORALUI_ROOT}/cmake/CoralBuild.cmake")
  coral_apply_global_build_settings()

  set(IS_HOST_PLATFORM TRUE)

  # The examples link the framework libraries; they do not build or run the unit suites, which is why
  # this project does not need GoogleTest even when the shared third-party tree carries it.
  set(CORAL_BUILD_TESTS OFF CACHE BOOL "Build the CoralUI tests" FORCE)

  add_subdirectory("${CORALUI_ROOT}" "${CMAKE_BINARY_DIR}/coral-ui")
endif ()

enable_testing()

add_subdirectory(ui)
