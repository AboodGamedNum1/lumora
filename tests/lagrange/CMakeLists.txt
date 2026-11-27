# Tests for the lagrange module (core/lagrange). Pure CPU. Owned by the lagrange contributor.
add_executable(lumora_lagrange_tests lagrange_tests.cpp)
target_link_libraries(lumora_lagrange_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_lagrange_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/lagrange)
lumora_target_warnings(lumora_lagrange_tests)
add_test(NAME lumora_lagrange_tests COMMAND lumora_lagrange_tests)
