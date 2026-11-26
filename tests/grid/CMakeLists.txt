# Tests for the spatial grid module (core/grid). Pure CPU. Owned by the grid contributor.
add_executable(lumora_grid_tests grid_tests.cpp)
target_link_libraries(lumora_grid_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_grid_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/grid)
lumora_target_warnings(lumora_grid_tests)
add_test(NAME lumora_grid_tests COMMAND lumora_grid_tests)
