# Native test target — GoogleTest for DisplayAffinityController,
# HotkeyManager parsing/registration logic, dpi.cpp scaling math,
# isolated from actual window creation where possible
# (02-ARCHITECTURE.md §11).

cmake_minimum_required(VERSION 3.15)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
# Windows: link against the same CRT as the rest of the project.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(native_overlay_tests
  overlay_manager_test.cpp
  display_affinity_test.cpp
  hotkey_manager_test.cpp
  markdown_parser_test.cpp
  syntax_highlighter_test.cpp
  content_model_test.cpp
)

target_link_libraries(native_overlay_tests PRIVATE
  native_overlay
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(native_overlay_tests)
