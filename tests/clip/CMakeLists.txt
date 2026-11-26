# Tests for the clipping module (core/clip). Pure CPU. Owned by the clip contributor.
add_executable(lumora_clip_tests clip_tests.cpp)
target_link_libraries(lumora_clip_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_clip_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/clip)
lumora_target_warnings(lumora_clip_tests)
add_test(NAME lumora_clip_tests COMMAND lumora_clip_tests)
