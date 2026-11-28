# Tests for the dyck module (core/dyck). Pure CPU. Owned by the dyck contributor.
add_executable(lumora_dyck_tests dyck_tests.cpp)
target_link_libraries(lumora_dyck_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_dyck_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/dyck)
lumora_target_warnings(lumora_dyck_tests)
add_test(NAME lumora_dyck_tests COMMAND lumora_dyck_tests)
