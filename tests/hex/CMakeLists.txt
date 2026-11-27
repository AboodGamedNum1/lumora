# Tests for the hex module (core/hex). Pure CPU. Owned by the hex contributor.
add_executable(lumora_hex_tests hex_tests.cpp)
target_link_libraries(lumora_hex_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_hex_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/hex)
lumora_target_warnings(lumora_hex_tests)
add_test(NAME lumora_hex_tests COMMAND lumora_hex_tests)
