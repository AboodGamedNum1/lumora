# Tests for core/script: the sandboxed Lua host. Pure CPU, no GPU. Built only with LUMORA_ENABLE_LUA.
add_executable(lumora_script_tests script_tests.cpp aurora_script_tests.cpp)
target_link_libraries(lumora_script_tests PRIVATE lumora_script Catch2::Catch2WithMain)
target_compile_definitions(lumora_script_tests PRIVATE
  LUMORA_SAMPLES_DIR="${CMAKE_SOURCE_DIR}/samples"
  LUMORA_GOLDEN_CPU_DIR="${CMAKE_SOURCE_DIR}/tests/golden/cpu-reference"
  LUMORA_GOLDEN_CPU_OUT_DIR="${CMAKE_BINARY_DIR}/golden-out/cpu-reference")
lumora_target_warnings(lumora_script_tests)
add_test(NAME lumora_script_tests COMMAND lumora_script_tests)
