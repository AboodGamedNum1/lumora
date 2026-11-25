include(Dependencies)
lumora_fetch_catch2()
add_executable(lumora_tests core_tests.cpp rhi_tests.cpp)
target_link_libraries(lumora_tests PRIVATE lumora_core Catch2::Catch2WithMain)
lumora_target_warnings(lumora_tests)

# Test shaders are compiled to SPIR-V with glslangValidator into C headers at build time.
find_program(GLSLANG_VALIDATOR glslangValidator)
if(GLSLANG_VALIDATOR)
  set(SHADER_OUT ${CMAKE_CURRENT_BINARY_DIR}/shaders)
  file(MAKE_DIRECTORY ${SHADER_OUT})
  foreach(stage vert frag)
    add_custom_command(
      OUTPUT ${SHADER_OUT}/triangle_${stage}_spv.h
      COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.1 --vn triangle_${stage}_spv
              -o ${SHADER_OUT}/triangle_${stage}_spv.h ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.${stage}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.${stage}
      VERBATIM)
    list(APPEND SHADER_HEADERS ${SHADER_OUT}/triangle_${stage}_spv.h)
  endforeach()
  add_custom_target(lumora_test_shaders DEPENDS ${SHADER_HEADERS})
  add_dependencies(lumora_tests lumora_test_shaders)
  target_include_directories(lumora_tests PRIVATE ${SHADER_OUT})
  target_compile_definitions(lumora_tests PRIVATE LUMORA_HAS_TEST_SHADERS=1)
endif()

add_test(NAME lumora_tests COMMAND lumora_tests)
