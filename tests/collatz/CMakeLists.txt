# Tests for the collatz module (core/collatz). Pure CPU. Owned by the collatz contributor.
add_executable(lumora_collatz_tests collatz_tests.cpp)
target_link_libraries(lumora_collatz_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_collatz_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/collatz)
lumora_target_warnings(lumora_collatz_tests)
add_test(NAME lumora_collatz_tests COMMAND lumora_collatz_tests)
