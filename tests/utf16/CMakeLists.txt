# Tests for the utf16 module (core/utf16). Pure CPU. Owned by the utf16 contributor.
add_executable(lumora_utf16_tests utf16_tests.cpp)
target_link_libraries(lumora_utf16_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_utf16_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/utf16)
lumora_target_warnings(lumora_utf16_tests)
add_test(NAME lumora_utf16_tests COMMAND lumora_utf16_tests)
