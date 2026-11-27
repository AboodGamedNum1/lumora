# Tests for the ordstat module (core/ordstat). Pure CPU. Owned by the ordstat contributor.
add_executable(lumora_ordstat_tests ordstat_tests.cpp)
target_link_libraries(lumora_ordstat_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_ordstat_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/ordstat)
lumora_target_warnings(lumora_ordstat_tests)
add_test(NAME lumora_ordstat_tests COMMAND lumora_ordstat_tests)
