# Tests for the particle module (core/geom). Pure CPU. Owned by the geom contributor.
add_executable(lumora_geom_tests geom_tests.cpp)
target_link_libraries(lumora_geom_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_geom_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/geom)
lumora_target_warnings(lumora_geom_tests)
add_test(NAME lumora_geom_tests COMMAND lumora_geom_tests)
