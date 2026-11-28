# Tests for the mulorder module (core/mulorder). Pure CPU. Owned by the mulorder contributor.
add_executable(lumora_mulorder_tests mulorder_tests.cpp)
target_link_libraries(lumora_mulorder_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_mulorder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/mulorder)
lumora_target_warnings(lumora_mulorder_tests)
add_test(NAME lumora_mulorder_tests COMMAND lumora_mulorder_tests)
