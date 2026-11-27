# Tests for the rmq module (core/rmq). Pure CPU. Owned by the rmq contributor.
add_executable(lumora_rmq_tests rmq_tests.cpp)
target_link_libraries(lumora_rmq_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_rmq_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/rmq)
lumora_target_warnings(lumora_rmq_tests)
add_test(NAME lumora_rmq_tests COMMAND lumora_rmq_tests)
