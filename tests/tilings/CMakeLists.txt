# Tests for the tilings module (core/tilings). Pure CPU. Owned by the tilings contributor.
add_executable(lumora_tilings_tests tilings_tests.cpp)
target_link_libraries(lumora_tilings_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_tilings_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/tilings)
lumora_target_warnings(lumora_tilings_tests)
add_test(NAME lumora_tilings_tests COMMAND lumora_tilings_tests)
