# Tests for the iroot module (core/iroot). Pure CPU. Owned by the iroot contributor.
add_executable(lumora_iroot_tests iroot_tests.cpp)
target_link_libraries(lumora_iroot_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_iroot_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/iroot)
lumora_target_warnings(lumora_iroot_tests)
add_test(NAME lumora_iroot_tests COMMAND lumora_iroot_tests)
