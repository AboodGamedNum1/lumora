# Tests for the diagnostics log ring (core/diag/diag_log). Pure CPU. Owned by the diag_log contributor.
add_executable(lumora_diag_log_tests diag_log_tests.cpp)
target_link_libraries(lumora_diag_log_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_diag_log_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/diag)
lumora_target_warnings(lumora_diag_log_tests)
add_test(NAME lumora_diag_log_tests COMMAND lumora_diag_log_tests)
