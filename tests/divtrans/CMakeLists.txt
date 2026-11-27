# Tests for the divtrans module (core/divtrans). Pure CPU. Owned by the divtrans contributor.
add_executable(lumora_divtrans_tests divtrans_tests.cpp)
target_link_libraries(lumora_divtrans_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_divtrans_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/divtrans)
lumora_target_warnings(lumora_divtrans_tests)
add_test(NAME lumora_divtrans_tests COMMAND lumora_divtrans_tests)
