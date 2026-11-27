# Tests for the bitio module (core/bitio). Pure CPU. Owned by the bitio contributor.
add_executable(lumora_bitio_tests bitio_tests.cpp)
target_link_libraries(lumora_bitio_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_bitio_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/bitio)
lumora_target_warnings(lumora_bitio_tests)
add_test(NAME lumora_bitio_tests COMMAND lumora_bitio_tests)
