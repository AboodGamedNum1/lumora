# Tests for the crt module (core/crt). Pure CPU. Owned by the crt contributor.
add_executable(lumora_crt_tests crt_tests.cpp)
target_link_libraries(lumora_crt_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_crt_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/crt)
lumora_target_warnings(lumora_crt_tests)
add_test(NAME lumora_crt_tests COMMAND lumora_crt_tests)
