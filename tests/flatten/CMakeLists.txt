# Tests for the Bezier flattener (core/flatten). Pure CPU. Owned by the flatten contributor.
add_executable(lumora_flatten_tests flatten_tests.cpp)
target_link_libraries(lumora_flatten_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_flatten_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/flatten)
lumora_target_warnings(lumora_flatten_tests)
add_test(NAME lumora_flatten_tests COMMAND lumora_flatten_tests)
