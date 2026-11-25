# Tests for the particle module (core/spline). Pure CPU. Owned by the spline contributor.
add_executable(lumora_spline_tests spline_tests.cpp)
target_link_libraries(lumora_spline_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_spline_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/spline)
lumora_target_warnings(lumora_spline_tests)
add_test(NAME lumora_spline_tests COMMAND lumora_spline_tests)
