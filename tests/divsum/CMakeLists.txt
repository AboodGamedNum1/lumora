# Tests for the divsum module (core/divsum). Pure CPU. Owned by the divsum contributor.
add_executable(lumora_divsum_tests divsum_tests.cpp)
target_link_libraries(lumora_divsum_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_divsum_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/divsum)
lumora_target_warnings(lumora_divsum_tests)
add_test(NAME lumora_divsum_tests COMMAND lumora_divsum_tests)
