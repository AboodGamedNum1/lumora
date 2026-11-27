# Tests for the suffix module (core/suffix). Pure CPU. Owned by the suffix contributor.
add_executable(lumora_suffix_tests suffix_tests.cpp)
target_link_libraries(lumora_suffix_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_suffix_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/suffix)
lumora_target_warnings(lumora_suffix_tests)
add_test(NAME lumora_suffix_tests COMMAND lumora_suffix_tests)
