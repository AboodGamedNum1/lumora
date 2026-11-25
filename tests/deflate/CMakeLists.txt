# Tests for core/package/deflate. Pure CPU. Owned by the deflate contributor.
add_executable(lumora_deflate_tests deflate_tests.cpp deflate_fixtures.h)
target_link_libraries(lumora_deflate_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_deflate_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/package)
lumora_target_warnings(lumora_deflate_tests)
add_test(NAME lumora_deflate_tests COMMAND lumora_deflate_tests)
