// SPIR-V to GLSL ES conversion for the GLES 3.2 backend, through SPIRV-Cross (pinned in docs/dependency-manifest.md).
// Built only when LUMORA_ENABLE_SPIRV_CROSS is ON. At runtime the phone never compiles shaders from source
// languages; this converter is also the planned export-time step of the packer (D8).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace lumora::rhi {

enum class ShaderStage { Vertex, Fragment };

struct GlesShader {
  bool ok = false;
  std::string source;  // GLSL ES text, "#version 300 es" or newer, on success
  std::string error;
};

// Converts one Vulkan-flavoured SPIR-V module to GLSL ES. Vertex outputs are Y-flipped so that the same
// SPIR-V draws the same image on Vulkan and GLES. `version` is the GLSL ES version number (300 or 320).
GlesShader spirv_to_glsl_es(const std::uint32_t* words, std::size_t word_count, ShaderStage stage, unsigned version = 300);

}  // namespace lumora::rhi
