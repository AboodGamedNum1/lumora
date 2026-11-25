# Tests for core/scene: JSON model and manifest policy. Pure CPU, no GPU.
add_executable(lumora_scene_tests scene_tests.cpp checksums_tests.cpp)
target_link_libraries(lumora_scene_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_scene_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/scene)
lumora_target_warnings(lumora_scene_tests)
add_test(NAME lumora_scene_tests COMMAND lumora_scene_tests)
