# Tests for the sumsq module (core/sumsq). Pure CPU. Owned by the sumsq contributor.
add_executable(lumora_sumsq_tests sumsq_tests.cpp)
target_link_libraries(lumora_sumsq_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_sumsq_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/sumsq)
lumora_target_warnings(lumora_sumsq_tests)
add_test(NAME lumora_sumsq_tests COMMAND lumora_sumsq_tests)
