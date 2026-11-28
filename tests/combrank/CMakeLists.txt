# Tests for the combrank module (core/combrank). Pure CPU. Owned by the combrank contributor.
add_executable(lumora_combrank_tests combrank_tests.cpp)
target_link_libraries(lumora_combrank_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_combrank_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/combrank)
lumora_target_warnings(lumora_combrank_tests)
add_test(NAME lumora_combrank_tests COMMAND lumora_combrank_tests)
