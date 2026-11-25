# Tests for core/script: the sandboxed Lua host. Pure CPU, no GPU. Built only with LUMORA_ENABLE_LUA.
add_executable(lumora_script_tests script_tests.cpp)
target_link_libraries(lumora_script_tests PRIVATE lumora_script Catch2::Catch2WithMain)
lumora_target_warnings(lumora_script_tests)
add_test(NAME lumora_script_tests COMMAND lumora_script_tests)
