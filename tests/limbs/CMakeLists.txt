# Tests for the limbs module (core/limbs). Pure CPU. Owned by the limbs contributor.
add_executable(lumora_limbs_tests limbs_tests.cpp)
target_link_libraries(lumora_limbs_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_limbs_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/limbs)
lumora_target_warnings(lumora_limbs_tests)
add_test(NAME lumora_limbs_tests COMMAND lumora_limbs_tests)
