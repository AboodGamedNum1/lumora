# Tests for the signed distance field module (core/sdf). Pure CPU. Owned by the sdf contributor.
add_executable(lumora_sdf_tests sdf_tests.cpp)
target_link_libraries(lumora_sdf_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_sdf_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/sdf)
lumora_target_warnings(lumora_sdf_tests)
add_test(NAME lumora_sdf_tests COMMAND lumora_sdf_tests)
