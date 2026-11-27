# Tests for the gauss module (core/gauss). Pure CPU. Owned by the gauss contributor.
add_executable(lumora_gauss_tests gauss_tests.cpp)
target_link_libraries(lumora_gauss_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_gauss_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/gauss)
lumora_target_warnings(lumora_gauss_tests)
add_test(NAME lumora_gauss_tests COMMAND lumora_gauss_tests)
