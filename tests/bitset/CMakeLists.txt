# Tests for the bitset module (core/bitset). Pure CPU. Owned by the bitset contributor.
add_executable(lumora_bitset_tests bitset_tests.cpp)
target_link_libraries(lumora_bitset_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_bitset_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/bitset)
lumora_target_warnings(lumora_bitset_tests)
add_test(NAME lumora_bitset_tests COMMAND lumora_bitset_tests)
