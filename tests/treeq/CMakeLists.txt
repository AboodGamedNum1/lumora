# Tests for the treeq module (core/treeq). Pure CPU. Owned by the treeq contributor.
add_executable(lumora_treeq_tests treeq_tests.cpp)
target_link_libraries(lumora_treeq_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_treeq_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/treeq)
lumora_target_warnings(lumora_treeq_tests)
add_test(NAME lumora_treeq_tests COMMAND lumora_treeq_tests)
