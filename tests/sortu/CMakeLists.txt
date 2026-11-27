# Tests for the sortu module (core/sortu). Pure CPU. Owned by the sortu contributor.
add_executable(lumora_sortu_tests sortu_tests.cpp)
target_link_libraries(lumora_sortu_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_sortu_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/sortu)
lumora_target_warnings(lumora_sortu_tests)
add_test(NAME lumora_sortu_tests COMMAND lumora_sortu_tests)
