# Tests for the box filter module (core/boxfilter). Pure CPU. Owned by the box filter contributor.
add_executable(lumora_boxfilter_tests box_filter_tests.cpp)
target_link_libraries(lumora_boxfilter_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_boxfilter_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/boxfilter)
lumora_target_warnings(lumora_boxfilter_tests)
add_test(NAME lumora_boxfilter_tests COMMAND lumora_boxfilter_tests)
