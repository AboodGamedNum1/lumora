# Tests for the ordered dither (core/dither). Pure CPU. Owned by the dither contributor.
add_executable(lumora_dither_tests dither_tests.cpp)
target_link_libraries(lumora_dither_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_dither_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/dither)
lumora_target_warnings(lumora_dither_tests)
add_test(NAME lumora_dither_tests COMMAND lumora_dither_tests)
