# Tests for the bytesearch module (core/bytesearch). Pure CPU. Owned by the bytesearch contributor.
add_executable(lumora_bytesearch_tests bytesearch_tests.cpp)
target_link_libraries(lumora_bytesearch_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_bytesearch_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/bytesearch)
lumora_target_warnings(lumora_bytesearch_tests)
add_test(NAME lumora_bytesearch_tests COMMAND lumora_bytesearch_tests)
