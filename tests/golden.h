// Golden-image helper (Section 16.1): compare a rendered RGBA8 image with a stored baseline
// within a per-channel tolerance. Baselines are binary PPM (P6, RGB) files stored per renderer
// under tests/golden/<renderer>/. On mismatch or a missing baseline, the actual image and a diff
// image are written to the output directory so CI can upload them as artifacts.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace golden {

struct Image {
  unsigned w = 0, h = 0;
  std::vector<std::uint8_t> rgb;  // w*h*3
};

inline Image from_rgba(const std::uint8_t* rgba, unsigned w, unsigned h) {
  Image im{w, h, std::vector<std::uint8_t>(std::size_t{w} * h * 3)};
  for (std::size_t i = 0; i < std::size_t{w} * h; ++i)
    for (int c = 0; c < 3; ++c) im.rgb[i * 3 + c] = rgba[i * 4 + c];
  return im;
}

inline bool write_ppm(const std::filesystem::path& p, const Image& im) {
  std::filesystem::create_directories(p.parent_path());
  std::ofstream f(p, std::ios::binary);
  if (!f) return false;
  f << "P6\n" << im.w << " " << im.h << "\n255\n";
  f.write(reinterpret_cast<const char*>(im.rgb.data()), static_cast<std::streamsize>(im.rgb.size()));
  return static_cast<bool>(f);
}

inline bool read_ppm(const std::filesystem::path& p, Image& im) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  std::string magic;
  unsigned w = 0, h = 0, maxv = 0;
  f >> magic >> w >> h >> maxv;
  if (magic != "P6" || maxv != 255 || w == 0 || h == 0) return false;
  f.get();  // single whitespace after the header
  im.w = w;
  im.h = h;
  im.rgb.resize(std::size_t{w} * h * 3);
  f.read(reinterpret_cast<char*>(im.rgb.data()), static_cast<std::streamsize>(im.rgb.size()));
  return static_cast<bool>(f);
}

struct Result {
  bool ok = false;
  unsigned max_delta = 0;
  std::size_t bad_pixels = 0;
  std::string message;
};

// Pure comparison: every channel of every pixel must be within `tolerance`.
inline Result compare(const Image& a, const Image& b, unsigned tolerance, Image* diff = nullptr) {
  Result r;
  if (a.w != b.w || a.h != b.h) {
    r.message = "size mismatch";
    return r;
  }
  if (diff) *diff = Image{a.w, a.h, std::vector<std::uint8_t>(a.rgb.size(), 0)};
  for (std::size_t i = 0; i < a.rgb.size(); i += 3) {
    unsigned worst = 0;
    for (int c = 0; c < 3; ++c) {
      unsigned d = static_cast<unsigned>(std::abs(int{a.rgb[i + c]} - int{b.rgb[i + c]}));
      worst = std::max(worst, d);
    }
    r.max_delta = std::max(r.max_delta, worst);
    if (worst > tolerance) {
      ++r.bad_pixels;
      if (diff) diff->rgb[i] = 255;
    }
  }
  r.ok = r.bad_pixels == 0;
  r.message = r.ok ? "match" : "pixels differ beyond tolerance";
  return r;
}

// Compare `actual` against <golden_dir>/<name>.ppm. Always writes the actual image to
// <out_dir>/<name>.actual.ppm. Set LUMORA_BLESS_GOLDEN=1 to overwrite the baseline.
inline Result check(const std::string& name, const Image& actual, const std::string& golden_dir,
                    const std::string& out_dir, unsigned tolerance) {
  namespace fs = std::filesystem;
  write_ppm(fs::path(out_dir) / (name + ".actual.ppm"), actual);
  const fs::path base = fs::path(golden_dir) / (name + ".ppm");
  if (const char* b = std::getenv("LUMORA_BLESS_GOLDEN"); b && *b == '1') {
    write_ppm(base, actual);
    return {true, 0, 0, "baseline written"};
  }
  Image expected;
  if (!read_ppm(base, expected)) return {false, 0, 0, "baseline missing: " + base.string()};
  Image diff;
  Result r = compare(actual, expected, tolerance, &diff);
  if (!r.ok) write_ppm(fs::path(out_dir) / (name + ".diff.ppm"), diff);
  return r;
}

}  // namespace golden
