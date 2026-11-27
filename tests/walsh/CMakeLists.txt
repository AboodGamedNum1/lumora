# Tests for the walsh module (core/walsh). Pure CPU. Owned by the walsh contributor.
add_executable(lumora_walsh_tests walsh_tests.cpp)
target_link_libraries(lumora_walsh_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_walsh_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/walsh)
lumora_target_warnings(lumora_walsh_tests)
add_test(NAME lumora_walsh_tests COMMAND lumora_walsh_tests)
