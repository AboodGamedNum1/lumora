# Tests for the render gate (core/lifecycle/render_gate). Pure CPU. Owned by the render_gate contributor.
add_executable(lumora_render_gate_tests render_gate_tests.cpp)
target_link_libraries(lumora_render_gate_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_render_gate_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/lifecycle)
lumora_target_warnings(lumora_render_gate_tests)
add_test(NAME lumora_render_gate_tests COMMAND lumora_render_gate_tests)
