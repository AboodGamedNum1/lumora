# Tests for the checksum module (core/checksum). Pure CPU. Owned by the checksum contributor.
add_executable(lumora_checksum_tests checksum_tests.cpp)
target_link_libraries(lumora_checksum_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_checksum_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/checksum)
lumora_target_warnings(lumora_checksum_tests)
add_test(NAME lumora_checksum_tests COMMAND lumora_checksum_tests)
