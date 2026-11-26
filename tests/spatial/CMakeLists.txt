# Tests for the spatial index module (core/spatial). Pure CPU. Owned by the spatial contributor.
add_executable(lumora_spatial_tests spatial_tests.cpp)
target_link_libraries(lumora_spatial_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_spatial_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/spatial)
lumora_target_warnings(lumora_spatial_tests)
add_test(NAME lumora_spatial_tests COMMAND lumora_spatial_tests)
