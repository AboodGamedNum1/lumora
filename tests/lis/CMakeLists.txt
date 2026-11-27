# Tests for the lis module (core/lis). Pure CPU. Owned by the lis contributor.
add_executable(lumora_lis_tests lis_tests.cpp)
target_link_libraries(lumora_lis_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_lis_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/lis)
lumora_target_warnings(lumora_lis_tests)
add_test(NAME lumora_lis_tests COMMAND lumora_lis_tests)
