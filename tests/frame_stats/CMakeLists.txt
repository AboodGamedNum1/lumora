# Tests for frame statistics and the fps overlay text (core/lifecycle/frame_stats). Pure CPU. Owned by the frame_stats contributor.
add_executable(lumora_frame_stats_tests frame_stats_tests.cpp)
target_link_libraries(lumora_frame_stats_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_frame_stats_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/lifecycle)
lumora_target_warnings(lumora_frame_stats_tests)
add_test(NAME lumora_frame_stats_tests COMMAND lumora_frame_stats_tests)
