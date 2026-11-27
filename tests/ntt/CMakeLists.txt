# Tests for the ntt module (core/ntt). Pure CPU. Owned by the ntt contributor.
add_executable(lumora_ntt_tests ntt_tests.cpp)
target_link_libraries(lumora_ntt_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_ntt_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/ntt)
lumora_target_warnings(lumora_ntt_tests)
add_test(NAME lumora_ntt_tests COMMAND lumora_ntt_tests)
