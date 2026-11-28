# Tests for the jacobi module (core/jacobi). Pure CPU. Owned by the jacobi contributor.
add_executable(lumora_jacobi_tests jacobi_tests.cpp)
target_link_libraries(lumora_jacobi_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_jacobi_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/jacobi)
lumora_target_warnings(lumora_jacobi_tests)
add_test(NAME lumora_jacobi_tests COMMAND lumora_jacobi_tests)
