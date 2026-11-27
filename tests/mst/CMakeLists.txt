# Tests for the mst module (core/mst). Pure CPU. Owned by the mst contributor.
add_executable(lumora_mst_tests mst_tests.cpp)
target_link_libraries(lumora_mst_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_mst_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/mst)
lumora_target_warnings(lumora_mst_tests)
add_test(NAME lumora_mst_tests COMMAND lumora_mst_tests)
