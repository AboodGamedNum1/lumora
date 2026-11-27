# Tests for the histogram module (core/histogram). Pure CPU. Owned by the histogram contributor.
add_executable(lumora_histogram_tests histogram_tests.cpp)
target_link_libraries(lumora_histogram_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_histogram_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/histogram)
lumora_target_warnings(lumora_histogram_tests)
add_test(NAME lumora_histogram_tests COMMAND lumora_histogram_tests)
