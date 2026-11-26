# Tests for the capabilities (core/package/capabilities). Pure CPU. Owned by the capabilities contributor.
add_executable(lumora_capabilities_tests capabilities_tests.cpp)
target_link_libraries(lumora_capabilities_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_capabilities_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/package)
lumora_target_warnings(lumora_capabilities_tests)
add_test(NAME lumora_capabilities_tests COMMAND lumora_capabilities_tests)
