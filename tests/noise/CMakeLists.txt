# Tests for the noise module (core/noise). Pure CPU. Owned by the noise contributor.
add_executable(lumora_noise_tests noise_tests.cpp)
target_link_libraries(lumora_noise_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_noise_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/noise)
lumora_target_warnings(lumora_noise_tests)
add_test(NAME lumora_noise_tests COMMAND lumora_noise_tests)
