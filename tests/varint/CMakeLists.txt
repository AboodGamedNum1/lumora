# Tests for the varint module (core/varint). Pure CPU. Owned by the varint contributor.
add_executable(lumora_varint_tests varint_tests.cpp)
target_link_libraries(lumora_varint_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_varint_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/varint)
lumora_target_warnings(lumora_varint_tests)
add_test(NAME lumora_varint_tests COMMAND lumora_varint_tests)
