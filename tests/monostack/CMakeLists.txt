# Tests for the monostack module (core/monostack). Pure CPU. Owned by the monostack contributor.
add_executable(lumora_monostack_tests monostack_tests.cpp)
target_link_libraries(lumora_monostack_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_monostack_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/monostack)
lumora_target_warnings(lumora_monostack_tests)
add_test(NAME lumora_monostack_tests COMMAND lumora_monostack_tests)
