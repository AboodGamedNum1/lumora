# Tests for the maxflow module (core/maxflow). Pure CPU. Owned by the maxflow contributor.
add_executable(lumora_maxflow_tests maxflow_tests.cpp)
target_link_libraries(lumora_maxflow_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_maxflow_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/maxflow)
lumora_target_warnings(lumora_maxflow_tests)
add_test(NAME lumora_maxflow_tests COMMAND lumora_maxflow_tests)
