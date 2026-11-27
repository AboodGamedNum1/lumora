# Tests for the dsu module (core/dsu). Pure CPU. Owned by the dsu contributor.
add_executable(lumora_dsu_tests dsu_tests.cpp)
target_link_libraries(lumora_dsu_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_dsu_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/dsu)
lumora_target_warnings(lumora_dsu_tests)
add_test(NAME lumora_dsu_tests COMMAND lumora_dsu_tests)
