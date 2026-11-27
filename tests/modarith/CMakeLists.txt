# Tests for the modarith module (core/modarith). Pure CPU. Owned by the modarith contributor.
add_executable(lumora_modarith_tests modarith_tests.cpp)
target_link_libraries(lumora_modarith_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_modarith_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/modarith)
lumora_target_warnings(lumora_modarith_tests)
add_test(NAME lumora_modarith_tests COMMAND lumora_modarith_tests)
