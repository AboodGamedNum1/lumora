# Tests for the xxh32 module (core/xxh32). Pure CPU. Owned by the xxh32 contributor.
add_executable(lumora_xxh32_tests xxh32_tests.cpp)
target_link_libraries(lumora_xxh32_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_xxh32_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/xxh32)
lumora_target_warnings(lumora_xxh32_tests)
add_test(NAME lumora_xxh32_tests COMMAND lumora_xxh32_tests)
