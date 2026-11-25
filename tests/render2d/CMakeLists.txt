# Tests for core/render2d: the CPU reference renderer. Pure CPU, no GPU.
add_executable(lumora_render2d_tests render2d_tests.cpp aurora_render_tests.cpp golden_cpu_tests.cpp)
target_link_libraries(lumora_render2d_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_render2d_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/render2d ${CMAKE_CURRENT_SOURCE_DIR}/../../core/scene)
target_compile_definitions(lumora_render2d_tests PRIVATE LUMORA_SAMPLES_DIR="${CMAKE_SOURCE_DIR}/samples")
target_compile_definitions(lumora_render2d_tests PRIVATE LUMORA_GOLDEN_CPU_DIR="${CMAKE_SOURCE_DIR}/tests/golden/cpu-reference" LUMORA_GOLDEN_CPU_OUT_DIR="${CMAKE_BINARY_DIR}/golden-out/cpu-reference")
lumora_target_warnings(lumora_render2d_tests)
add_test(NAME lumora_render2d_tests COMMAND lumora_render2d_tests)
