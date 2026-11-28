# Tests for the pell module (core/pell). Pure CPU. Owned by the pell contributor.
add_executable(lumora_pell_tests pell_tests.cpp)
target_link_libraries(lumora_pell_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_pell_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/pell)
lumora_target_warnings(lumora_pell_tests)
add_test(NAME lumora_pell_tests COMMAND lumora_pell_tests)
