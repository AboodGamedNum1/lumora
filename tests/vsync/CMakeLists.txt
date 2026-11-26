# Tests for the vsync period estimator (core/lifecycle/vsync). Pure CPU. Owned by the vsync contributor.
add_executable(lumora_vsync_tests vsync_tests.cpp)
target_link_libraries(lumora_vsync_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_vsync_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/lifecycle)
lumora_target_warnings(lumora_vsync_tests)
add_test(NAME lumora_vsync_tests COMMAND lumora_vsync_tests)
