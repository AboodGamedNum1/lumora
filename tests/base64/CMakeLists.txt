# Tests for the base64 module (core/base64). Pure CPU. Owned by the base64 contributor.
add_executable(lumora_base64_tests base64_tests.cpp)
target_link_libraries(lumora_base64_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_base64_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/base64)
lumora_target_warnings(lumora_base64_tests)
add_test(NAME lumora_base64_tests COMMAND lumora_base64_tests)
