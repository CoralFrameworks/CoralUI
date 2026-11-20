# ==================================================================================================
# Tests
#
# MVVM is header-only; the test executable is the only thing this module builds.
# ==================================================================================================

add_executable(tests_MVVM tests_MVVM.cpp test_main.cpp)
target_link_libraries(tests_MVVM PRIVATE mvvm observer gtest)
set_target_properties(tests_MVVM PROPERTIES FOLDER Tests)

add_test(NAME tests_MVVM COMMAND tests_MVVM)
