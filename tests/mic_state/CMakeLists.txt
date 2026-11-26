# Tests for the microphone state machine (core/lifecycle/mic_state). Pure CPU. Owned by the mic state contributor.
add_executable(lumora_mic_state_tests mic_state_tests.cpp)
target_link_libraries(lumora_mic_state_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_mic_state_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/lifecycle)
lumora_target_warnings(lumora_mic_state_tests)
add_test(NAME lumora_mic_state_tests COMMAND lumora_mic_state_tests)
