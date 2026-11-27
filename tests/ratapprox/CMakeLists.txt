# Tests for the ratapprox module (core/ratapprox). Pure CPU. Owned by the ratapprox contributor.
add_executable(lumora_ratapprox_tests ratapprox_tests.cpp)
target_link_libraries(lumora_ratapprox_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_ratapprox_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/ratapprox)
lumora_target_warnings(lumora_ratapprox_tests)
add_test(NAME lumora_ratapprox_tests COMMAND lumora_ratapprox_tests)
