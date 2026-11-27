# Tests for the zorder module (core/zorder). Pure CPU. Owned by the zorder contributor.
add_executable(lumora_zorder_tests zorder_tests.cpp)
target_link_libraries(lumora_zorder_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_zorder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/zorder)
lumora_target_warnings(lumora_zorder_tests)
add_test(NAME lumora_zorder_tests COMMAND lumora_zorder_tests)
