# Tests for the rle module (core/rle). Pure CPU. Owned by the rle contributor.
add_executable(lumora_rle_tests rle_tests.cpp)
target_link_libraries(lumora_rle_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_rle_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/rle)
lumora_target_warnings(lumora_rle_tests)
add_test(NAME lumora_rle_tests COMMAND lumora_rle_tests)
