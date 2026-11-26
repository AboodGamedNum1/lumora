# Tests for the crash-loop breaker (core/lifecycle/crash_breaker). Pure CPU. Owned by the crash breaker contributor.
add_executable(lumora_crash_breaker_tests crash_breaker_tests.cpp)
target_link_libraries(lumora_crash_breaker_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_crash_breaker_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/lifecycle)
lumora_target_warnings(lumora_crash_breaker_tests)
add_test(NAME lumora_crash_breaker_tests COMMAND lumora_crash_breaker_tests)
