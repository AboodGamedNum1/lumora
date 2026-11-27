# Tests for the cartree module (core/cartree). Pure CPU. Owned by the cartree contributor.
add_executable(lumora_cartree_tests cartree_tests.cpp)
target_link_libraries(lumora_cartree_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_cartree_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/cartree)
lumora_target_warnings(lumora_cartree_tests)
add_test(NAME lumora_cartree_tests COMMAND lumora_cartree_tests)
