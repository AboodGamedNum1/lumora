# Tests for the seqdist module (core/seqdist). Pure CPU. Owned by the seqdist contributor.
add_executable(lumora_seqdist_tests seqdist_tests.cpp)
target_link_libraries(lumora_seqdist_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_seqdist_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/seqdist)
lumora_target_warnings(lumora_seqdist_tests)
add_test(NAME lumora_seqdist_tests COMMAND lumora_seqdist_tests)
