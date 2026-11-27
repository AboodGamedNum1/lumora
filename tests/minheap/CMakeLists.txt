# Tests for the minheap module (core/minheap). Pure CPU. Owned by the minheap contributor.
add_executable(lumora_minheap_tests minheap_tests.cpp)
target_link_libraries(lumora_minheap_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_minheap_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/minheap)
lumora_target_warnings(lumora_minheap_tests)
add_test(NAME lumora_minheap_tests COMMAND lumora_minheap_tests)
