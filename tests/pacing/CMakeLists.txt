# Tests for frame pacing and render scale (core/lifecycle/pacing). Pure CPU. Owned by the pacing contributor.
add_executable(lumora_pacing_tests pacing_tests.cpp)
target_link_libraries(lumora_pacing_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_pacing_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/lifecycle)
lumora_target_warnings(lumora_pacing_tests)
add_test(NAME lumora_pacing_tests COMMAND lumora_pacing_tests)
