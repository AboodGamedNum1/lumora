# Tests for the bitrank module (core/bitrank). Pure CPU. Owned by the bitrank contributor.
add_executable(lumora_bitrank_tests bitrank_tests.cpp)
target_link_libraries(lumora_bitrank_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_bitrank_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/bitrank)
lumora_target_warnings(lumora_bitrank_tests)
add_test(NAME lumora_bitrank_tests COMMAND lumora_bitrank_tests)
