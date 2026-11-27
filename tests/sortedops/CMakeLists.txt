# Tests for the sortedops module (core/sortedops). Pure CPU. Owned by the sortedops contributor.
add_executable(lumora_sortedops_tests sortedops_tests.cpp)
target_link_libraries(lumora_sortedops_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_sortedops_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/sortedops)
lumora_target_warnings(lumora_sortedops_tests)
add_test(NAME lumora_sortedops_tests COMMAND lumora_sortedops_tests)
