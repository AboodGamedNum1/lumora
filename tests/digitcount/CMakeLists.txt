# Tests for the digitcount module (core/digitcount). Pure CPU. Owned by the digitcount contributor.
add_executable(lumora_digitcount_tests digitcount_tests.cpp)
target_link_libraries(lumora_digitcount_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_digitcount_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/digitcount)
lumora_target_warnings(lumora_digitcount_tests)
add_test(NAME lumora_digitcount_tests COMMAND lumora_digitcount_tests)
