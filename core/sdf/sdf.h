// Lumora CPU signed distance fields (scope S). Pure CPU, deterministic, no I/O, no threads, no floating point at all.
// The interface in this header is fixed; the contributor implements core/sdf/sdf.cpp only (plus tests and docs).
// Results must be bit-identical on Linux, Windows and Android: integer arithmetic only (64-bit where needed), no libm, no
// floats, no unordered containers, no static mutable state, no pointer values in any decision. Purpose: SDF textures for crisp
// scalable text, shapes and UI from 8-bit masks (for example core/raster masks). Evidence label: software (CPU) only.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::sdf {

constexpr std::uint32_t kMaxSize = 4096;  // width and height
constexpr std::uint32_t kMaxSpread = 127; // pixels

enum class Status : std::uint8_t {
  Ok = 0,
  NullBuffer,      // mask, out or scratch is null
  BadSize,         // width or height is 0 or > kMaxSize, or a stride is below width
  BadSpread,       // spread is 0 or > kMaxSpread
  ScratchTooSmall, // scratch_count < width * height
  Overlap          // touched byte ranges intersect (mask, out, scratch; integer address ranges as in core/composite)
};
// Lower-case spaced enum names ("ok", "null buffer", "bad size", "bad spread", "scratch too small", "overlap"), "unknown" otherwise.
const char* status_text(Status s);

// Exact squared Euclidean distance transform (no approximation, no chamfer). A pixel is SET when mask >= threshold. For every
// pixel (x, y) out[y * width + x] is the squared distance, in pixel units, between the pixel centre and the nearest SET pixel
// centre; 0 for a SET pixel. When no pixel is set every value is kNoSet. Row-major, tightly packed, width * height values.
// The algorithm is free (a two pass exact method such as Felzenszwalb and Huttenlocher is expected), the result is not: it must
// equal the brute force minimum over all SET pixels, which the tests compare on random and structured inputs.
// Checks in enum order: NullBuffer, BadSize (mask stride included), Overlap (the touched mask rows against the width * height * 4
// bytes of out, decided as in make_sdf). kNoSet is fixed so the output is portable.
constexpr std::uint32_t kNoSet = 0xFFFFFFFFu;
Status distance_squared(const std::uint8_t* mask, std::uint32_t width, std::uint32_t height, std::uint32_t stride,
                        std::uint8_t threshold, std::uint32_t* out);

// Integer square root: floor(sqrt(v)), exact for every uint64.
std::uint32_t isqrt(std::uint64_t v);

// Signed distance field as an 8-bit image. For each pixel: inside = (mask >= 128). d_out = distance to the nearest inside
// pixel (for outside pixels), d_in = distance to the nearest outside pixel (for inside pixels), both from distance_squared
// and isqrt in Q8 (1/256 pixel): q = isqrt(d2 * 65536). The signed value s is +q for inside pixels and -q for outside pixels.
// If there is no pixel of the other class the distance is taken as spread * 256 (the maximum). Encoding: out = clamp(128 +
// round_half_away(s * 127 / (spread * 256)), 1, 255), with the division rounded to nearest, ties away from zero, exactly as
// docs/sdf.md defines. 128 is the zero level but is never produced: distances are between pixel centres, at least one pixel,
// so the smallest magnitude is round(127 / spread) >= 1 step. A set pixel next to the edge is at least 129 and an unset one at
// most 127. 255 means spread pixels or more inside, 1 means spread pixels or more outside.
// `scratch` holds at least width * height uint32 values (scratch_count) and is overwritten. `out` is width * height bytes at
// out_stride. Checks in enum order: NullBuffer, BadSize (strides included), BadSpread, ScratchTooSmall, Overlap. Overlap is
// reported when any two of these byte ranges intersect: the touched mask rows, the touched out rows, and the first
// width * height * 4 bytes of scratch. Nothing is written on error. Stride padding of out is never written.
Status make_sdf(const std::uint8_t* mask, std::uint32_t width, std::uint32_t height, std::uint32_t mask_stride,
                std::uint8_t* out, std::uint32_t out_stride, std::uint32_t spread, std::uint32_t* scratch,
                std::size_t scratch_count);

// Reading an SDF back: the coverage of a pixel value v with an anti-aliasing half width aa (in value steps, 1..127).
// Defined exactly as: t = (int(v) - 128) * 255 + int(aa) * 255; c = floor(t / (2 * aa)) with floor division for negative t;
// result = clamp(c, 0, 255). For aa outside 1..127 the result is 0 for v < 128 and 255 for v >= 128.
std::uint8_t edge_coverage(std::uint8_t v, std::uint32_t aa);

// FNV-1a 64 over bytes of an 8-bit image, rows top to bottom, width bytes per row; null or zero size hashes zero bytes.
std::uint64_t digest_image(const std::uint8_t* data, std::uint32_t width, std::uint32_t height, std::uint32_t stride);

}  // namespace lumora::sdf
