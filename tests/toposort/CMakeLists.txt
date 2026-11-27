# Tests for the toposort module (core/toposort). Pure CPU. Owned by the toposort contributor.
add_executable(lumora_toposort_tests toposort_tests.cpp)
target_link_libraries(lumora_toposort_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_toposort_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/toposort)
lumora_target_warnings(lumora_toposort_tests)
add_test(NAME lumora_toposort_tests COMMAND lumora_toposort_tests)
