# Tests for the stirling module (core/stirling). Pure CPU. Owned by the stirling contributor.
add_executable(lumora_stirling_tests stirling_tests.cpp)
target_link_libraries(lumora_stirling_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_stirling_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/stirling)
lumora_target_warnings(lumora_stirling_tests)
add_test(NAME lumora_stirling_tests COMMAND lumora_stirling_tests)
