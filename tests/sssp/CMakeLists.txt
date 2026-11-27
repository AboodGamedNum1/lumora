# Tests for the sssp module (core/sssp). Pure CPU. Owned by the sssp contributor.
add_executable(lumora_sssp_tests sssp_tests.cpp)
target_link_libraries(lumora_sssp_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_sssp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/sssp)
lumora_target_warnings(lumora_sssp_tests)
add_test(NAME lumora_sssp_tests COMMAND lumora_sssp_tests)
