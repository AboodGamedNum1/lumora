# Tests for the arithfun module (core/arithfun). Pure CPU. Owned by the arithfun contributor.
add_executable(lumora_arithfun_tests arithfun_tests.cpp)
target_link_libraries(lumora_arithfun_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_arithfun_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/arithfun)
lumora_target_warnings(lumora_arithfun_tests)
add_test(NAME lumora_arithfun_tests COMMAND lumora_arithfun_tests)
