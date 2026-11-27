# Tests for the slotmap module (core/slotmap). Pure CPU. Owned by the slotmap contributor.
add_executable(lumora_slotmap_tests slot_map_tests.cpp)
target_link_libraries(lumora_slotmap_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_slotmap_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/slotmap)
lumora_target_warnings(lumora_slotmap_tests)
add_test(NAME lumora_slotmap_tests COMMAND lumora_slotmap_tests)
