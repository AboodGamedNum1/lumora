# Tests for the lyndon module (core/lyndon). Pure CPU. Owned by the lyndon contributor.
add_executable(lumora_lyndon_tests lyndon_tests.cpp)
target_link_libraries(lumora_lyndon_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_lyndon_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/lyndon)
lumora_target_warnings(lumora_lyndon_tests)
add_test(NAME lumora_lyndon_tests COMMAND lumora_lyndon_tests)
