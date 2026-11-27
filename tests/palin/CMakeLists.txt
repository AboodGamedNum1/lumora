# Tests for the palin module (core/palin). Pure CPU. Owned by the palin contributor.
add_executable(lumora_palin_tests palin_tests.cpp)
target_link_libraries(lumora_palin_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_palin_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/palin)
lumora_target_warnings(lumora_palin_tests)
add_test(NAME lumora_palin_tests COMMAND lumora_palin_tests)
