cmake_minimum_required(VERSION 3.19)
set(TARGET mvvm)
set(PUBLIC_HDR_DIR include)

# ==================================================================================================
# Sources and headers
#
# MVVM is header-only and depends only on `observer`. It deliberately does not link `ui`, `event` or
# `graphics`: a view-model must be usable in a test, a tool or a future non-UI host.
# ==================================================================================================
set(PUBLIC_HDRS
    include/mvvm/AsyncCommand.h
    include/mvvm/Command.h
    include/mvvm/Dispatcher.h
    include/mvvm/Field.h
    include/mvvm/ObservableObject.h
    include/mvvm/ObservableList.h
    include/mvvm/Validation.h
)

# ==================================================================================================
# Target
# ==================================================================================================
add_library(${TARGET} INTERFACE ${PUBLIC_HDRS})

target_include_directories(${TARGET} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${PUBLIC_HDR_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${TARGET} INTERFACE observer)
target_compile_features(${TARGET} INTERFACE cxx_std_20)

set_target_properties(${TARGET} PROPERTIES FOLDER Libs)

# ==================================================================================================
# Installation
#
# `mvvm` and its only dependency (`observer`) are INTERFACE targets, so a downstream project can
# consume them with find_package(CoralMVVM) without the graphics/platform stack.
# ==================================================================================================
install(TARGETS ${TARGET} EXPORT CoralMVVMTargets)
install(DIRECTORY ${PUBLIC_HDR_DIR}/${TARGET} DESTINATION include)

include(CMakePackageConfigHelpers)
set(CORAL_MVVM_CMAKE_DIR "lib/cmake/CoralMVVM")
install(EXPORT CoralMVVMTargets
  NAMESPACE Coral::
  DESTINATION ${CORAL_MVVM_CMAKE_DIR}
)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/CoralMVVMConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/CoralMVVMConfig.cmake
  INSTALL_DESTINATION ${CORAL_MVVM_CMAKE_DIR}
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/CoralMVVMConfigVersion.cmake
  VERSION ${CORAL_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/CoralMVVMConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/CoralMVVMConfigVersion.cmake
  DESTINATION ${CORAL_MVVM_CMAKE_DIR}
)

# ==================================================================================================
# Tests
# ==================================================================================================
if (CORAL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif ()
