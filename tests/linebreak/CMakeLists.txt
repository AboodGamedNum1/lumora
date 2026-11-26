# Tests for the line breaker (core/linebreak). Pure CPU. Owned by the line break contributor.
add_executable(lumora_linebreak_tests linebreak_tests.cpp)
target_link_libraries(lumora_linebreak_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_linebreak_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/linebreak)
lumora_target_warnings(lumora_linebreak_tests)
add_test(NAME lumora_linebreak_tests COMMAND lumora_linebreak_tests)
