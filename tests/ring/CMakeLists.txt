# Tests for the ring module (core/ring). Pure CPU. Owned by the ring contributor.
add_executable(lumora_ring_tests byte_ring_tests.cpp)
target_link_libraries(lumora_ring_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_ring_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/ring ${CMAKE_CURRENT_SOURCE_DIR}/../../core/rect)
lumora_target_warnings(lumora_ring_tests)
add_test(NAME lumora_ring_tests COMMAND lumora_ring_tests)
