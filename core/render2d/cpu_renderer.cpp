// Stub: the contributor implements render_scene, encode_ppm, decode_ppm and load_textures (see header).
#include "cpu_renderer.h"

namespace lumora::render2d {

bool decode_ppm(const std::uint8_t*, std::size_t, Image&, std::string& error, std::uint32_t) {
  error = "not implemented";
  return false;
}

bool load_textures(const lumora::scene::Scene&, const lumora::scene::FileMap&, TextureMap&, std::string& error) {
  error = "not implemented";
  return false;
}

Image render_scene(const lumora::scene::Scene&, const TextureMap&, const View&) { return {}; }

std::vector<std::uint8_t> encode_ppm(const Image&) { return {}; }

}  // namespace lumora::render2d
