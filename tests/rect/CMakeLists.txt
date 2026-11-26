# Tests for the rect module (core/rect). Pure CPU. Owned by the rect contributor.
add_executable(lumora_rect_tests rect_tests.cpp)
target_link_libraries(lumora_rect_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_rect_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/rect)
lumora_target_warnings(lumora_rect_tests)
add_test(NAME lumora_rect_tests COMMAND lumora_rect_tests)
