# Tests for the PCG32 generator (core/prng). Pure CPU. Owned by the prng contributor.
add_executable(lumora_prng_tests prng_tests.cpp)
target_link_libraries(lumora_prng_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_prng_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/prng)
lumora_target_warnings(lumora_prng_tests)
add_test(NAME lumora_prng_tests COMMAND lumora_prng_tests)
