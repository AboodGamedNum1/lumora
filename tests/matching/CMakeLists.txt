# Tests for the matching module (core/matching). Pure CPU. Owned by the matching contributor.
add_executable(lumora_matching_tests matching_tests.cpp)
target_link_libraries(lumora_matching_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_matching_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/matching)
lumora_target_warnings(lumora_matching_tests)
add_test(NAME lumora_matching_tests COMMAND lumora_matching_tests)
