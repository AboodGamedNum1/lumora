# Tests for the animation module (core/anim). Pure CPU. Owned by the animation contributor.
add_executable(lumora_anim_tests anim_tests.cpp player_tests.cpp)
target_link_libraries(lumora_anim_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_anim_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/anim ${CMAKE_CURRENT_SOURCE_DIR}/../../core/scene)
lumora_target_warnings(lumora_anim_tests)
add_test(NAME lumora_anim_tests COMMAND lumora_anim_tests)
