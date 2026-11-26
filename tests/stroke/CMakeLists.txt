# Tests for the stroker module (core/stroke). Pure CPU. Owned by the stroke contributor.
add_executable(lumora_stroke_tests stroke_tests.cpp)
target_link_libraries(lumora_stroke_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_stroke_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/stroke)
lumora_target_warnings(lumora_stroke_tests)
add_test(NAME lumora_stroke_tests COMMAND lumora_stroke_tests)
