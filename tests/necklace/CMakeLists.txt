# Tests for the necklace module (core/necklace). Pure CPU. Owned by the necklace contributor.
add_executable(lumora_necklace_tests necklace_tests.cpp)
target_link_libraries(lumora_necklace_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_necklace_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/necklace)
lumora_target_warnings(lumora_necklace_tests)
add_test(NAME lumora_necklace_tests COMMAND lumora_necklace_tests)
