# Tests for the extraction plan (core/package/extract_plan). Pure CPU. Owned by the extraction plan contributor.
add_executable(lumora_extract_plan_tests extract_plan_tests.cpp)
target_link_libraries(lumora_extract_plan_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_extract_plan_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/package)
lumora_target_warnings(lumora_extract_plan_tests)
add_test(NAME lumora_extract_plan_tests COMMAND lumora_extract_plan_tests)
