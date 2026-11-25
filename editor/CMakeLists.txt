include(Dependencies)
lumora_fetch_sdl3()
lumora_fetch_imgui()
add_executable(lumora_editor main.cpp)
target_link_libraries(lumora_editor PRIVATE lumora_core imgui SDL3::SDL3)
lumora_target_warnings(lumora_editor)
if(LUMORA_BUILD_TESTS)
  # Empty-window smoke test: opens a hidden window, runs a few ImGui frames, exits.
  # No GPU is needed, the SDL dummy video driver is used when no display exists.
  add_test(NAME editor_smoke COMMAND lumora_editor --smoke)
  set_tests_properties(editor_smoke PROPERTIES TIMEOUT 60)
endif()
