# Tests for the utf8 module (core/utf8). Pure CPU. Owned by the utf8 contributor.
add_executable(lumora_utf8_tests utf8_tests.cpp)
target_link_libraries(lumora_utf8_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_utf8_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/utf8)
lumora_target_warnings(lumora_utf8_tests)
add_test(NAME lumora_utf8_tests COMMAND lumora_utf8_tests)
