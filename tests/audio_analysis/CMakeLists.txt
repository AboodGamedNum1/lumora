# Tests for the audio analysis core (core/audio/analysis). Pure CPU. Owned by the audio analysis contributor.
add_executable(lumora_audio_analysis_tests audio_analysis_tests.cpp)
target_link_libraries(lumora_audio_analysis_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_audio_analysis_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/audio)
lumora_target_warnings(lumora_audio_analysis_tests)
add_test(NAME lumora_audio_analysis_tests COMMAND lumora_audio_analysis_tests)
