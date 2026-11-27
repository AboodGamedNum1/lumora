# Tests for the ratio module (core/ratio). Pure CPU. Owned by the ratio contributor.
add_executable(lumora_ratio_tests ratio_tests.cpp)
target_link_libraries(lumora_ratio_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_ratio_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/ratio)
lumora_target_warnings(lumora_ratio_tests)
add_test(NAME lumora_ratio_tests COMMAND lumora_ratio_tests)
