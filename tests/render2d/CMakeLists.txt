# Tests for core/render2d: the CPU reference renderer. Pure CPU, no GPU.
add_executable(lumora_render2d_tests render2d_tests.cpp)
target_link_libraries(lumora_render2d_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_render2d_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/render2d ${CMAKE_CURRENT_SOURCE_DIR}/../../core/scene)
target_compile_definitions(lumora_render2d_tests PRIVATE LUMORA_SAMPLES_DIR="${CMAKE_SOURCE_DIR}/samples")
lumora_target_warnings(lumora_render2d_tests)
add_test(NAME lumora_render2d_tests COMMAND lumora_render2d_tests)
