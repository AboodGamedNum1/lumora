# Tests for the wallpaper offset filter (core/input/offset_filter). Pure CPU. Owned by the offset_filter contributor.
add_executable(lumora_offset_filter_tests offset_filter_tests.cpp)
target_link_libraries(lumora_offset_filter_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_offset_filter_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/input)
lumora_target_warnings(lumora_offset_filter_tests)
add_test(NAME lumora_offset_filter_tests COMMAND lumora_offset_filter_tests)
