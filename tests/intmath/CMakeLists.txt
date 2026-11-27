# Tests for the intmath module (core/intmath). Pure CPU. Owned by the intmath contributor.
add_executable(lumora_intmath_tests intmath_tests.cpp)
target_link_libraries(lumora_intmath_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_intmath_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/intmath)
lumora_target_warnings(lumora_intmath_tests)
add_test(NAME lumora_intmath_tests COMMAND lumora_intmath_tests)
