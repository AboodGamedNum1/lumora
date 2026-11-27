# Tests for the hull2d module (core/hull2d). Pure CPU. Owned by the hull2d contributor.
add_executable(lumora_hull2d_tests hull2d_tests.cpp)
target_link_libraries(lumora_hull2d_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_hull2d_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/hull2d)
lumora_target_warnings(lumora_hull2d_tests)
add_test(NAME lumora_hull2d_tests COMMAND lumora_hull2d_tests)
