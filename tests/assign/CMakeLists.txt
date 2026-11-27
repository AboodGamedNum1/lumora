# Tests for the assign module (core/assign). Pure CPU. Owned by the assign contributor.
add_executable(lumora_assign_tests assign_tests.cpp)
target_link_libraries(lumora_assign_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_assign_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/assign)
lumora_target_warnings(lumora_assign_tests)
add_test(NAME lumora_assign_tests COMMAND lumora_assign_tests)
