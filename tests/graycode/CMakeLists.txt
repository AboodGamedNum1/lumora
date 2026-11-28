# Tests for the graycode module (core/graycode). Pure CPU. Owned by the graycode contributor.
add_executable(lumora_graycode_tests graycode_tests.cpp)
target_link_libraries(lumora_graycode_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_graycode_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/graycode)
lumora_target_warnings(lumora_graycode_tests)
add_test(NAME lumora_graycode_tests COMMAND lumora_graycode_tests)
