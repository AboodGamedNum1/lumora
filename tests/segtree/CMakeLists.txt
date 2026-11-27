# Tests for the segtree module (core/segtree). Pure CPU. Owned by the segtree contributor.
add_executable(lumora_segtree_tests segtree_tests.cpp)
target_link_libraries(lumora_segtree_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_segtree_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/segtree)
lumora_target_warnings(lumora_segtree_tests)
add_test(NAME lumora_segtree_tests COMMAND lumora_segtree_tests)
