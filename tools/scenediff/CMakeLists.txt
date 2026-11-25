# lumora-scenediff: compare two Lumora packages. CPU only, no extra dependencies.
add_library(lumora_scenediff STATIC scene_diff.cpp)
target_link_libraries(lumora_scenediff PUBLIC lumora_core)
target_include_directories(lumora_scenediff PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core/scene)
lumora_target_warnings(lumora_scenediff)

add_executable(lumora_scenediff_cli main.cpp)
target_link_libraries(lumora_scenediff_cli PRIVATE lumora_scenediff)
set_target_properties(lumora_scenediff_cli PROPERTIES OUTPUT_NAME lumora-scenediff)
lumora_target_warnings(lumora_scenediff_cli)

if(LUMORA_BUILD_TESTS)
  add_executable(lumora_scenediff_tests tests/scenediff_tests.cpp)
  target_link_libraries(lumora_scenediff_tests PRIVATE lumora_scenediff Catch2::Catch2WithMain)
  target_compile_definitions(lumora_scenediff_tests PRIVATE LUMORA_SAMPLES_DIR="${CMAKE_SOURCE_DIR}/samples")
  lumora_target_warnings(lumora_scenediff_tests)
  add_test(NAME lumora_scenediff_tests COMMAND lumora_scenediff_tests)
endif()
