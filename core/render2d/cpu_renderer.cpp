// CPU reference renderer for 2D parallax scenes. Conventions are in cpu_renderer.h.
// Rendering inverse-maps each pixel centre into a layer's local quad, samples bilinearly and blends
// source-over into the 8-bit RGBA buffer, layer by layer, rounding to 8 bits after each layer.
#include "cpu_renderer.h"

#include "png.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace lumora::render2d {
namespace {

constexpr std::uint32_t kMaxViewSide = 8192;

bool is_space(std::uint8_t c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }

// Cursor over PPM bytes. Every access is bounds-checked.
struct Cursor {
  const std::uint8_t* data;
  std::size_t size;
  std::size_t pos = 0;

  bool at_end() const { return pos >= size; }

  // Skips whitespace and '#' comments (to end of line).
  void skip_space() {
    while (pos < size) {
      if (is_space(data[pos])) {
        ++pos;
      } else if (data[pos] == '#') {
        while (pos < size && data[pos] != '\n') ++pos;
      } else {
        break;
      }
    }
  }

  // Reads an unsigned decimal of at most 9 digits, so it can never overflow 32 bits.
  bool read_number(std::uint32_t& value) {
    skip_space();
    std::size_t digits = 0;
    std::uint64_t v = 0;
    while (pos < size && data[pos] >= '0' && data[pos] <= '9') {
      if (++digits > 9) return false;
      v = v * 10 + (data[pos] - '0');
      ++pos;
    }
    if (digits == 0) return false;
    value = static_cast<std::uint32_t>(v);
    return true;
  }
};

// Affine 2D map: x' = a*x + c*y + tx, y' = b*x + d*y + ty.
struct Affine {
  float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
};

Affine compose(const Affine& p, const Affine& l) {
  Affine r;
  r.a = p.a * l.a + p.c * l.b;
  r.b = p.b * l.a + p.d * l.b;
  r.c = p.a * l.c + p.c * l.d;
  r.d = p.b * l.c + p.d * l.d;
  r.tx = p.a * l.tx + p.c * l.ty + p.tx;
  r.ty = p.b * l.tx + p.d * l.ty + p.ty;
  return r;
}

// Local transform: scale, then rotate counter-clockwise, then translate.
Affine local_affine(const lumora::scene::Transform& t) {
  const float rad = t.rotation_z_degrees * 3.14159265358979323846f / 180.0f;
  const float cs = std::cos(rad);
  const float sn = std::sin(rad);
  Affine m;
  m.a = cs * t.scale[0];
  m.b = sn * t.scale[0];
  m.c = -sn * t.scale[1];
  m.d = cs * t.scale[1];
  m.tx = t.position[0];
  m.ty = t.position[1];
  return m;
}

std::uint8_t to_u8(float v) {
  const float r = std::floor(v + 0.5f);
  if (!(r > 0.0f)) return 0;
  if (r >= 255.0f) return 255;
  return static_cast<std::uint8_t>(r);
}

// Bilinear sample at texel-space (tx, ty); the caller guarantees the point is inside the quad, and
// coordinates past the outermost texel centres clamp to the edge texel.
void sample(const Image& img, float tx, float ty, float out[4]) {
  const float maxx = static_cast<float>(img.width - 1);
  const float maxy = static_cast<float>(img.height - 1);
  tx = std::min(std::max(tx, 0.0f), maxx);
  ty = std::min(std::max(ty, 0.0f), maxy);
  const std::uint32_t x0 = static_cast<std::uint32_t>(tx);
  const std::uint32_t y0 = static_cast<std::uint32_t>(ty);
  const std::uint32_t x1 = std::min(x0 + 1, img.width - 1);
  const std::uint32_t y1 = std::min(y0 + 1, img.height - 1);
  const float fx = tx - static_cast<float>(x0);
  const float fy = ty - static_cast<float>(y0);
  const std::uint8_t* p00 = &img.rgba[(std::size_t{y0} * img.width + x0) * 4];
  const std::uint8_t* p10 = &img.rgba[(std::size_t{y0} * img.width + x1) * 4];
  const std::uint8_t* p01 = &img.rgba[(std::size_t{y1} * img.width + x0) * 4];
  const std::uint8_t* p11 = &img.rgba[(std::size_t{y1} * img.width + x1) * 4];
  for (int k = 0; k < 4; ++k) {
    const float top = static_cast<float>(p00[k]) * (1 - fx) + static_cast<float>(p10[k]) * fx;
    const float bot = static_cast<float>(p01[k]) * (1 - fx) + static_cast<float>(p11[k]) * fx;
    out[k] = top * (1 - fy) + bot * fy;
  }
}

// Straight-alpha source-over into dst.
void blend(std::uint8_t* dst, const float src[4]) {
  const float sa = src[3] / 255.0f;
  if (!(sa > 0.0f)) return;
  const float da = static_cast<float>(dst[3]) / 255.0f;
  const float oa = sa + da * (1 - sa);
  if (!(oa > 0.0f)) return;
  for (int k = 0; k < 3; ++k) {
    dst[k] = to_u8((src[k] * sa + static_cast<float>(dst[k]) * da * (1 - sa)) / oa);
  }
  dst[3] = to_u8(oa * 255.0f);
}

void draw_layer(Image& target, const Image& tex, const Affine& m) {
  const float det = m.a * m.d - m.b * m.c;
  if (!std::isfinite(det) || std::fabs(det) < 1e-12f) return;
  const float w = static_cast<float>(target.width);
  const float h = static_cast<float>(target.height);

  // Pixel bounding box of the transformed quad, to avoid visiting the whole view per layer.
  float minx = std::numeric_limits<float>::infinity(), maxx = -minx, miny = minx, maxy = -minx;
  for (int i = 0; i < 4; ++i) {
    const float qx = (i & 1) ? 1.0f : -1.0f;
    const float qy = (i & 2) ? 1.0f : -1.0f;
    const float x = m.a * qx + m.c * qy + m.tx;
    const float y = m.b * qx + m.d * qy + m.ty;
    if (!std::isfinite(x) || !std::isfinite(y)) return;
    const float px = (x + 1.0f) * 0.5f * w;
    const float py = (1.0f - y) * 0.5f * h;
    minx = std::min(minx, px);
    maxx = std::max(maxx, px);
    miny = std::min(miny, py);
    maxy = std::max(maxy, py);
  }
  const auto clampi = [](float v, float hi) { return static_cast<std::uint32_t>(std::min(std::max(v, 0.0f), hi)); };
  const std::uint32_t x_begin = clampi(std::floor(minx) - 1, w);
  const std::uint32_t x_end = clampi(std::ceil(maxx) + 1, w);
  const std::uint32_t y_begin = clampi(std::floor(miny) - 1, h);
  const std::uint32_t y_end = clampi(std::ceil(maxy) + 1, h);

  const float inv_det = 1.0f / det;
  const float tw = static_cast<float>(tex.width);
  const float th = static_cast<float>(tex.height);
  for (std::uint32_t py = y_begin; py < y_end; ++py) {
    const float y = 1.0f - (static_cast<float>(py) + 0.5f) * 2.0f / h;
    for (std::uint32_t px = x_begin; px < x_end; ++px) {
      const float x = (static_cast<float>(px) + 0.5f) * 2.0f / w - 1.0f;
      const float dx = x - m.tx;
      const float dy = y - m.ty;
      const float lx = (m.d * dx - m.c * dy) * inv_det;
      const float ly = (-m.b * dx + m.a * dy) * inv_det;
      const float u = (lx + 1.0f) * 0.5f;
      const float v = (1.0f - ly) * 0.5f;
      if (!(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)) continue;
      float s[4];
      sample(tex, u * tw - 0.5f, v * th - 0.5f, s);
      blend(&target.rgba[(std::size_t{py} * target.width + px) * 4], s);
    }
  }
}

}  // namespace

bool decode_ppm(const std::uint8_t* data, std::size_t size, Image& out, std::string& error, std::uint32_t max_dim) {
  out = Image{};
  if (data == nullptr && size != 0) {
    error = "null input";
    return false;
  }
  Cursor cur{data, size};
  if (size < 2 || data[0] != 'P' || (data[1] != '3' && data[1] != '6')) {
    error = "not a P3 or P6 PPM";
    return false;
  }
  const bool binary = data[1] == '6';
  cur.pos = 2;
  std::uint32_t width = 0, height = 0, maxval = 0;
  if (!cur.read_number(width) || !cur.read_number(height) || !cur.read_number(maxval)) {
    error = "bad or missing header number";
    return false;
  }
  if (width == 0 || height == 0 || width > max_dim || height > max_dim) {
    error = "image dimensions out of range";
    return false;
  }
  if (maxval != 255) {
    error = "maxval must be 255";
    return false;
  }
  const std::size_t pixels = std::size_t{width} * height;
  const std::size_t samples = pixels * 3;
  Image img;
  if (binary) {
    // Exactly one whitespace byte separates the header from the raster.
    if (cur.at_end() || !is_space(data[cur.pos])) {
      error = "missing separator after header";
      return false;
    }
    ++cur.pos;
    const std::size_t remaining = size - cur.pos;
    if (remaining < samples) {
      error = "truncated pixel data";
      return false;
    }
    if (remaining > samples) {
      error = "trailing data after pixel data";
      return false;
    }
    img.rgba.resize(pixels * 4);
    for (std::size_t i = 0; i < pixels; ++i) {
      img.rgba[i * 4 + 0] = data[cur.pos + i * 3 + 0];
      img.rgba[i * 4 + 1] = data[cur.pos + i * 3 + 1];
      img.rgba[i * 4 + 2] = data[cur.pos + i * 3 + 2];
      img.rgba[i * 4 + 3] = 255;
    }
  } else {
    // Each sample needs at least one digit and one separator (the last needs no separator), so a
    // stream shorter than that cannot be valid; this bounds the allocation by the input size.
    if (size - cur.pos < samples * 2 - 1) {
      error = "truncated pixel data";
      return false;
    }
    img.rgba.assign(pixels * 4, 255);
    for (std::size_t i = 0; i < samples; ++i) {
      std::uint32_t v = 0;
      if (!cur.read_number(v)) {
        error = "truncated or malformed pixel data";
        return false;
      }
      if (v > 255) {
        error = "sample exceeds maxval";
        return false;
      }
      img.rgba[(i / 3) * 4 + (i % 3)] = static_cast<std::uint8_t>(v);
    }
    cur.skip_space();
    if (!cur.at_end()) {
      error = "trailing data after pixel data";
      return false;
    }
  }
  img.width = width;
  img.height = height;
  out = std::move(img);
  return true;
}

bool load_textures(const lumora::scene::Scene& scene, const lumora::scene::FileMap& files, TextureMap& out,
                   std::string& error) {
  out.clear();
  for (const auto& asset : scene.assets) {
    if (asset.kind != "texture") continue;
    const auto it = files.find(asset.path);
    if (it == files.end()) {
      error = "texture asset '" + asset.id + "': file '" + asset.path + "' is missing";
      out.clear();
      return false;
    }
    Image img;
    std::string why;
    // The format is chosen by content, not by the file name: the PNG signature selects the PNG codec, anything else is
    // handed to the PPM decoder (which rejects what it does not understand).
    static const std::uint8_t kPngSignature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    const bool is_png = it->second.size() >= 8 && std::equal(kPngSignature, kPngSignature + 8, it->second.begin());
    if (is_png) {
      lumora::image::PngResult png = lumora::image::decode_png(it->second.data(), it->second.size());
      if (!png.ok()) {
        error = "texture asset '" + asset.id + "': png: " + png.message;
        out.clear();
        return false;
      }
      img.width = png.image.width;
      img.height = png.image.height;
      img.rgba = std::move(png.image.rgba);
    } else if (!decode_ppm(it->second.data(), it->second.size(), img, why)) {
      error = "texture asset '" + asset.id + "': " + why;
      out.clear();
      return false;
    }
    out[asset.id] = std::move(img);
  }
  return true;
}

Image render_scene(const lumora::scene::Scene& scene, const TextureMap& textures, const View& view) {
  if (view.width == 0 || view.height == 0 || view.width > kMaxViewSide || view.height > kMaxViewSide) return {};
  Image img;
  img.width = view.width;
  img.height = view.height;
  img.rgba.resize(std::size_t{view.width} * view.height * 4);
  for (std::size_t i = 0; i < img.rgba.size(); i += 4) {
    for (int k = 0; k < 4; ++k) img.rgba[i + k] = view.clear_rgba[k];
  }

  // Depth-first pre-order with an explicit stack, so a deep tree cannot overflow the call stack.
  struct Item {
    const lumora::scene::Node* node;
    Affine parent;
  };
  std::vector<Item> stack;
  stack.push_back({&scene.root, Affine{}});
  while (!stack.empty()) {
    const Item item = stack.back();
    stack.pop_back();
    const Affine world = compose(item.parent, local_affine(item.node->transform));
    if (!item.node->layer_asset.empty()) {
      const auto it = textures.find(item.node->layer_asset);
      if (it != textures.end() && it->second.valid()) {
        Affine m = world;
        const float k = (1.0f - item.node->parallax_depth) * kParallaxRange;
        m.tx += view.offset_x * k;
        m.ty += view.offset_y * k;
        draw_layer(img, it->second, m);
      }
    }
    const auto& kids = item.node->children;
    for (auto c = kids.rbegin(); c != kids.rend(); ++c) stack.push_back({&*c, world});
  }
  return img;
}

std::vector<std::uint8_t> encode_ppm(const Image& image) {
  if (!image.valid()) return {};
  const std::string header = "P6\n" + std::to_string(image.width) + " " + std::to_string(image.height) + "\n255\n";
  std::vector<std::uint8_t> out(header.begin(), header.end());
  const std::size_t pixels = std::size_t{image.width} * image.height;
  out.reserve(out.size() + pixels * 3);
  for (std::size_t i = 0; i < pixels; ++i) {
    out.push_back(image.rgba[i * 4 + 0]);
    out.push_back(image.rgba[i * 4 + 1]);
    out.push_back(image.rgba[i * 4 + 2]);
  }
  return out;
}

}  // namespace lumora::render2d
