# Tests for the globmatch module (core/globmatch). Pure CPU. Owned by the globmatch contributor.
add_executable(lumora_globmatch_tests globmatch_tests.cpp)
target_link_libraries(lumora_globmatch_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_globmatch_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/globmatch)
lumora_target_warnings(lumora_globmatch_tests)
add_test(NAME lumora_globmatch_tests COMMAND lumora_globmatch_tests)
