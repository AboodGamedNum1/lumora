# Tests for the particle module (core/particles). Pure CPU. Owned by the particle contributor.
add_executable(lumora_particles_tests particles_tests.cpp)
target_link_libraries(lumora_particles_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_particles_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/particles)
lumora_target_warnings(lumora_particles_tests)
add_test(NAME lumora_particles_tests COMMAND lumora_particles_tests)
