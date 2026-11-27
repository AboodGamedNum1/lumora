# Tests for the sieve module (core/sieve). Pure CPU. Owned by the sieve contributor.
add_executable(lumora_sieve_tests sieve_tests.cpp)
target_link_libraries(lumora_sieve_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_sieve_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/sieve)
lumora_target_warnings(lumora_sieve_tests)
add_test(NAME lumora_sieve_tests COMMAND lumora_sieve_tests)
