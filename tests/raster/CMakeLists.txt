# Tests for the rasterizer module (core/raster). Pure CPU. Owned by the raster contributor.
add_executable(lumora_raster_tests raster_tests.cpp)
target_link_libraries(lumora_raster_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_raster_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/raster)
lumora_target_warnings(lumora_raster_tests)
add_test(NAME lumora_raster_tests COMMAND lumora_raster_tests)
