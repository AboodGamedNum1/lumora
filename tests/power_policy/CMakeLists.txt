# Tests for the power policy (core/lifecycle/power_policy). Pure CPU. Owned by the power_policy contributor.
add_executable(lumora_power_policy_tests power_policy_tests.cpp)
target_link_libraries(lumora_power_policy_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_power_policy_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/lifecycle)
lumora_target_warnings(lumora_power_policy_tests)
add_test(NAME lumora_power_policy_tests COMMAND lumora_power_policy_tests)
