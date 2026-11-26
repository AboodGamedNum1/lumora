# Tests for the polygon tessellation module (core/tess). Pure CPU. Owned by the tess contributor.
add_executable(lumora_tess_tests tess_tests.cpp)
target_link_libraries(lumora_tess_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_tess_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/tess)
lumora_target_warnings(lumora_tess_tests)
add_test(NAME lumora_tess_tests COMMAND lumora_tess_tests)
