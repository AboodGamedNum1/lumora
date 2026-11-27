# Tests for the floorsum module (core/floorsum). Pure CPU. Owned by the floorsum contributor.
add_executable(lumora_floorsum_tests floorsum_tests.cpp)
target_link_libraries(lumora_floorsum_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_floorsum_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/floorsum)
lumora_target_warnings(lumora_floorsum_tests)
add_test(NAME lumora_floorsum_tests COMMAND lumora_floorsum_tests)
