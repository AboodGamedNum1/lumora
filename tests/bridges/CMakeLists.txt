# Tests for the bridges module (core/bridges). Pure CPU. Owned by the bridges contributor.
add_executable(lumora_bridges_tests bridges_tests.cpp)
target_link_libraries(lumora_bridges_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_bridges_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/bridges)
lumora_target_warnings(lumora_bridges_tests)
add_test(NAME lumora_bridges_tests COMMAND lumora_bridges_tests)
