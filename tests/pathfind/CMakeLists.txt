# Tests for the pathfinding module (core/pathfind). Pure CPU. Owned by the pathfind contributor.
add_executable(lumora_pathfind_tests pathfind_tests.cpp)
target_link_libraries(lumora_pathfind_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_pathfind_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/pathfind)
lumora_target_warnings(lumora_pathfind_tests)
add_test(NAME lumora_pathfind_tests COMMAND lumora_pathfind_tests)
