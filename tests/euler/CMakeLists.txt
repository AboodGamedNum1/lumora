# Tests for the euler module (core/euler). Pure CPU. Owned by the euler contributor.
add_executable(lumora_euler_tests euler_tests.cpp)
target_link_libraries(lumora_euler_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_euler_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/euler)
lumora_target_warnings(lumora_euler_tests)
add_test(NAME lumora_euler_tests COMMAND lumora_euler_tests)
