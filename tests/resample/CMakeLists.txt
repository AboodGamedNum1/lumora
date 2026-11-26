# Tests for the resampling module (core/resample). Pure CPU. Owned by the resample contributor.
add_executable(lumora_resample_tests resample_tests.cpp)
target_link_libraries(lumora_resample_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_resample_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/resample)
lumora_target_warnings(lumora_resample_tests)
add_test(NAME lumora_resample_tests COMMAND lumora_resample_tests)
