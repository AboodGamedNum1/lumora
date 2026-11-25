# Tests for the particle module (core/color). Pure CPU. Owned by the color contributor.
add_executable(lumora_color_tests color_tests.cpp)
target_link_libraries(lumora_color_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_color_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/color)
lumora_target_warnings(lumora_color_tests)
add_test(NAME lumora_color_tests COMMAND lumora_color_tests)
