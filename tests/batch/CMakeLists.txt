# Tests for the sprite batcher module (core/batch). Pure CPU. Owned by the batch contributor.
add_executable(lumora_batch_tests batch_tests.cpp)
target_link_libraries(lumora_batch_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_batch_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/batch)
lumora_target_warnings(lumora_batch_tests)
add_test(NAME lumora_batch_tests COMMAND lumora_batch_tests)
