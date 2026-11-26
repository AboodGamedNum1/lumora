# Tests for the load budget (core/package/load_budget). Pure CPU. Owned by the load budget contributor.
add_executable(lumora_load_budget_tests load_budget_tests.cpp)
target_link_libraries(lumora_load_budget_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_load_budget_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/package)
lumora_target_warnings(lumora_load_budget_tests)
add_test(NAME lumora_load_budget_tests COMMAND lumora_load_budget_tests)
