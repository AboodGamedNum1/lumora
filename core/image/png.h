// Dependency-free PNG decoder and encoder for scene textures (the product's texture source before KTX2).
// Security-sensitive: decode_png runs on untrusted input from .lumora packages. It must never read or write out of
// bounds, never allocate more than the limits allow (checked from the header values before any allocation),
// never loop without consuming input, and reject every malformed file with a status, never by crashing.
// The PNG stream uses zlib framing around raw DEFLATE; the raw codec is core/package/deflate.h (already in the
// repository, written and tested separately). The loader wiring (render2d texture loading) is done by the project
// owner after the module lands; the contributor does not edit it.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lumora::image {

struct Rgba8 {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::vector<std::uint8_t> rgba;  // width * height * 4, straight (non-premultiplied) alpha
  bool valid() const { return width > 0 && height > 0 && rgba.size() == std::size_t{width} * height * 4; }
};

enum class PngStatus {
  Ok,
  NotPng,       // missing or wrong 8-byte signature
  Truncated,    // data ends inside a chunk or before IEND
  BadChunk,     // bad length, chunk order (IHDR first, one PLTE before IDAT, IDAT run contiguous, IEND last), unknown
                // critical chunk, duplicate IHDR/PLTE/tRNS where forbidden
  BadCrc,       // chunk CRC-32 mismatch (checked for every chunk, critical or not)
  BadHeader,    // invalid IHDR: zero size, bad colour type or bit depth combination, bad compression, filter or
                // interlace method
  BadPalette,   // missing PLTE for colour type 3, bad PLTE length, index beyond the palette, bad tRNS length
  BadZlib,      // bad zlib header, FDICT set, Adler-32 mismatch, or the DEFLATE stream is malformed
  BadFilter,    // filter type above 4, or decoded size does not match the image
  TooLarge      // width or height above max_dim, or pixel count above max_pixels (checked from the header; inflated data beyond the size the header implies is BadFilter)
};

struct PngLimits {
  std::uint32_t max_dim = 8192;            // per side
  std::uint64_t max_pixels = 16ull << 20;  // width * height
};

struct PngResult {
  PngStatus status = PngStatus::Truncated;
  std::string message;  // short reason, never contains input bytes
  Rgba8 image;          // valid only when status is Ok
  bool ok() const { return status == PngStatus::Ok; }
};

// Decodes every standard PNG into 8-bit RGBA: colour types 0, 2, 3, 4, 6; bit depths 1, 2, 4, 8, 16 as allowed per
// colour type (16-bit samples are reduced to 8 bits by taking the high byte); Adam7 interlacing; tRNS for types 0,
// 2 and 3. Gamma, colour profiles (gAMA, iCCP, sRGB, cHRM) and other ancillary chunks are checked for CRC and
// ignored. Animated PNG (acTL, fcTL, fdAT) decodes as its default image. Trailing bytes after IEND are an error.
PngResult decode_png(const std::uint8_t* data, std::size_t size, const PngLimits& limits = {});

// Encodes 8-bit RGBA as a non-interlaced PNG (colour type 6, bit depth 8) with per-row filter selection (smallest
// sum of absolute differences) and the DEFLATE codec at `level` (0 to 9). Deterministic: the same image and level
// give the same bytes. Returns an empty vector for an invalid image.
std::vector<std::uint8_t> encode_png(const Rgba8& image, int level = 6);

}  // namespace lumora::image
