# Tests for the pisano module (core/pisano). Pure CPU. Owned by the pisano contributor.
add_executable(lumora_pisano_tests pisano_tests.cpp)
target_link_libraries(lumora_pisano_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_pisano_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/pisano)
lumora_target_warnings(lumora_pisano_tests)
add_test(NAME lumora_pisano_tests COMMAND lumora_pisano_tests)
