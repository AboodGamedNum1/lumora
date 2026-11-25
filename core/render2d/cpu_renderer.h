// CPU reference renderer for 2D parallax scenes (milestone M3S support).
// Pure CPU, deterministic, no GPU and no platform code. It exists so that a scene can be rendered the same way
// on every CI machine (golden images, the "Linux software renderer" half of the M3S exit) and so that the GPU
// path has an independent reference. It is software evidence only, never device or GPU evidence.
//
// Conventions (normative for this module):
//  * Image: 8-bit RGBA, straight (non-premultiplied) alpha, row 0 is the top row, values are sRGB-encoded and
//    blended directly in that space (no linearisation in this draft).
//  * Scene space: normalised device coordinates, x right, y up, the viewport spans [-1,1] on both axes.
//    A layer node's texture covers exactly the unit quad [-1,1]^2 before its transform is applied.
//  * Transform of a node: scale (x,y), then rotation about the node origin by rotation_z_degrees
//    (counter-clockwise), then translation by position (x,y). Child transforms compose with the parent's.
//    position.z and scale.z are ignored in this draft.
//  * Parallax: a layer with parallax_depth d (0 = moves with the device, 1 = far background) is additionally
//    translated by view.offset * (1 - d) * kParallaxRange in scene space.
//  * Draw order: depth-first pre-order over the node tree, later nodes over earlier ones, source-over blending.
//  * Sampling: bilinear with texel centres at half-integers, outside the quad is transparent (no tiling).
//    Inside the quad, coordinates beyond the outermost texel centres clamp to the edge texel (hard cut at the edge).
//  * Positive view.offset_y moves layers up (scene y is up). PPM P6 input must have exactly 3*w*h raster bytes.
//  * Output is bit-exact reproducible for the same inputs on the same compiler; across compilers goldens use
//    the tolerance helpers in tests/golden.h.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "scene.h"

namespace lumora::render2d {

inline constexpr float kParallaxRange = 0.1f;

struct Image {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::vector<std::uint8_t> rgba;  // width * height * 4 bytes
  bool valid() const { return width > 0 && height > 0 && rgba.size() == std::size_t{width} * height * 4; }
};

// Decodes a PPM (P3 text or P6 binary, maxval 255 only) into an opaque image. Rejects everything else,
// including dimensions over max_dim, truncated data and trailing garbage. Never reads out of bounds.
bool decode_ppm(const std::uint8_t* data, std::size_t size, Image& out, std::string& error, std::uint32_t max_dim = 8192);

using TextureMap = std::map<std::string, Image>;  // key: asset id from scene.json

// Decodes every asset of `files` that scene.assets lists (kind "texture"; PNG by signature, otherwise PPM) into a TextureMap.
// False and an error naming the asset when one is missing or does not decode.
bool load_textures(const lumora::scene::Scene& scene, const lumora::scene::FileMap& files, TextureMap& out,
                   std::string& error);

struct View {
  std::uint32_t width = 256;
  std::uint32_t height = 256;
  float offset_x = 0;  // parallax offset in [-1,1], device tilt or touch
  float offset_y = 0;
  std::uint8_t clear_rgba[4] = {0, 0, 0, 255};
};

// Renders the scene. A layer whose texture is missing from `textures` is skipped (the loader already rejects
// such packages). Returns an invalid Image when the view size is zero or over 8192 on either side.
Image render_scene(const lumora::scene::Scene& scene, const TextureMap& textures, const View& view);

// Binary PPM (P6) encoding for golden files; alpha is dropped.
std::vector<std::uint8_t> encode_ppm(const Image& image);

}  // namespace lumora::render2d
