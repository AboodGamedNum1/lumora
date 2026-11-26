# Tests for the fixed-timestep accumulator (core/timing/fixed_step). Pure CPU. Owned by the fixed step contributor.
add_executable(lumora_fixed_step_tests fixed_step_tests.cpp)
target_link_libraries(lumora_fixed_step_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_fixed_step_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/timing)
lumora_target_warnings(lumora_fixed_step_tests)
add_test(NAME lumora_fixed_step_tests COMMAND lumora_fixed_step_tests)
