# Tests for the calkinwilf module (core/calkinwilf). Pure CPU. Owned by the calkinwilf contributor.
add_executable(lumora_calkinwilf_tests calkinwilf_tests.cpp)
target_link_libraries(lumora_calkinwilf_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_calkinwilf_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/calkinwilf)
lumora_target_warnings(lumora_calkinwilf_tests)
add_test(NAME lumora_calkinwilf_tests COMMAND lumora_calkinwilf_tests)
