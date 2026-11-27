# Tests for the combin module (core/combin). Pure CPU. Owned by the combin contributor.
add_executable(lumora_combin_tests combin_tests.cpp)
target_link_libraries(lumora_combin_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_combin_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/combin)
lumora_target_warnings(lumora_combin_tests)
add_test(NAME lumora_combin_tests COMMAND lumora_combin_tests)
