# Tests for the partition module (core/partition). Pure CPU. Owned by the partition contributor.
add_executable(lumora_partition_tests partition_tests.cpp)
target_link_libraries(lumora_partition_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_partition_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/partition)
lumora_target_warnings(lumora_partition_tests)
add_test(NAME lumora_partition_tests COMMAND lumora_partition_tests)
