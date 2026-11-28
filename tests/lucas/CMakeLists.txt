# Tests for the lucas module (core/lucas). Pure CPU. Owned by the lucas contributor.
add_executable(lumora_lucas_tests lucas_tests.cpp)
target_link_libraries(lumora_lucas_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_lucas_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/lucas)
lumora_target_warnings(lumora_lucas_tests)
add_test(NAME lumora_lucas_tests COMMAND lumora_lucas_tests)
