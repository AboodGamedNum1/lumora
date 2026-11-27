# Tests for the perm module (core/perm). Pure CPU. Owned by the perm contributor.
add_executable(lumora_perm_tests perm_tests.cpp)
target_link_libraries(lumora_perm_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_perm_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/perm)
lumora_target_warnings(lumora_perm_tests)
add_test(NAME lumora_perm_tests COMMAND lumora_perm_tests)
