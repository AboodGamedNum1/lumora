# Tests for the 16.16 fixed-point module (core/fixed). Pure CPU. Owned by the fixed contributor.
add_executable(lumora_fixed_tests fixed_tests.cpp)
target_link_libraries(lumora_fixed_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_fixed_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/fixed)
lumora_target_warnings(lumora_fixed_tests)
add_test(NAME lumora_fixed_tests COMMAND lumora_fixed_tests)
