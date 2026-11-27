# Tests for the coins module (core/coins). Pure CPU. Owned by the coins contributor.
add_executable(lumora_coins_tests coins_tests.cpp)
target_link_libraries(lumora_coins_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_coins_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/coins)
lumora_target_warnings(lumora_coins_tests)
add_test(NAME lumora_coins_tests COMMAND lumora_coins_tests)
