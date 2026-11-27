# Tests for the scc module (core/scc). Pure CPU. Owned by the scc contributor.
add_executable(lumora_scc_tests scc_tests.cpp)
target_link_libraries(lumora_scc_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_scc_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/scc)
lumora_target_warnings(lumora_scc_tests)
add_test(NAME lumora_scc_tests COMMAND lumora_scc_tests)
