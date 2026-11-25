# Tests for the atlas packer (core/image/atlas.*). Pure CPU. Owned by the atlas contributor.
add_executable(lumora_atlas_tests atlas_tests.cpp)
target_link_libraries(lumora_atlas_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_atlas_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/image)
lumora_target_warnings(lumora_atlas_tests)
add_test(NAME lumora_atlas_tests COMMAND lumora_atlas_tests)
