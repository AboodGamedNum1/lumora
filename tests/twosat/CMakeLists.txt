# Tests for the twosat module (core/twosat). Pure CPU. Owned by the twosat contributor.
add_executable(lumora_twosat_tests twosat_tests.cpp)
target_link_libraries(lumora_twosat_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_twosat_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/twosat)
lumora_target_warnings(lumora_twosat_tests)
add_test(NAME lumora_twosat_tests COMMAND lumora_twosat_tests)
