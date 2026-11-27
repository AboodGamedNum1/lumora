# Tests for the decfmt module (core/decfmt). Pure CPU. Owned by the decfmt contributor.
add_executable(lumora_decfmt_tests decfmt_tests.cpp)
target_link_libraries(lumora_decfmt_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_decfmt_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/decfmt)
lumora_target_warnings(lumora_decfmt_tests)
add_test(NAME lumora_decfmt_tests COMMAND lumora_decfmt_tests)
