#include "spirv_to_gles.h"

#include <exception>
#include <vector>

#include "spirv_glsl.hpp"

namespace lumora::rhi {

GlesShader spirv_to_glsl_es(const std::uint32_t* words, std::size_t word_count, ShaderStage stage, unsigned version) {
  GlesShader r;
  if (!words || word_count < 5 || words[0] != 0x07230203u) {
    r.error = "not a SPIR-V module";
    return r;
  }
  try {
    spirv_cross::CompilerGLSL c(words, word_count);
    spirv_cross::CompilerGLSL::Options o;
    o.es = true;
    o.version = version;
    o.vulkan_semantics = false;
    o.vertex.flip_vert_y = stage == ShaderStage::Vertex;
    o.fragment.default_float_precision = spirv_cross::CompilerGLSL::Options::Mediump;
    o.fragment.default_int_precision = spirv_cross::CompilerGLSL::Options::Mediump;
    c.set_common_options(o);
    r.source = c.compile();
    r.ok = true;
  } catch (const std::exception& e) {
    r.error = e.what();
  }
  return r;
}

}  // namespace lumora::rhi
