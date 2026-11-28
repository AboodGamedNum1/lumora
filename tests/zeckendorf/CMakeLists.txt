# Tests for the zeckendorf module (core/zeckendorf). Pure CPU. Owned by the zeckendorf contributor.
add_executable(lumora_zeckendorf_tests zeckendorf_tests.cpp)
target_link_libraries(lumora_zeckendorf_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_zeckendorf_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/zeckendorf)
lumora_target_warnings(lumora_zeckendorf_tests)
add_test(NAME lumora_zeckendorf_tests COMMAND lumora_zeckendorf_tests)
