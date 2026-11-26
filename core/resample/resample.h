// Lumora CPU image resampling and mip chains (scope R). Pure CPU, deterministic, no I/O, no threads, no floating point at all.
// The interface in this header is fixed; the contributor implements core/resample/resample.cpp only (plus tests and docs).
// Results must be bit-identical on Linux, Windows and Android: integer arithmetic only (64-bit where needed), no libm, no
// floats, no unordered containers, no static mutable state, no pointer values in any decision. Purpose: texture scaling and mip
// generation for the wallpaper engine (M4), the CPU reference for GPU paths. Evidence label: software (CPU) only.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::resample {

// Pixels are RGBA8 with premultiplied alpha, bytes R, G, B, A, as in core/composite. Sizes are 1..kMaxSize per side.
constexpr std::uint32_t kMaxSize = 8192;

struct ConstSurface {
  const std::uint8_t* data;
  std::uint32_t width, height, stride;  // stride in bytes, >= width * 4
};
struct Surface {
  std::uint8_t* data;
  std::uint32_t width, height, stride;
};

enum class Filter : std::uint8_t {
  Nearest = 0,   // source pixel containing the destination pixel centre
  Bilinear = 1,  // 2 x 2 taps at the destination pixel centre, edge clamped
  Box = 2,       // exact area average of the source footprint (for downscaling; equals Nearest at identical size)
  CatmullRom = 3 // 4 x 4 taps, Catmull-Rom weights, edge clamped, results clamped to 0..255
};

enum class Status : std::uint8_t {
  Ok = 0,
  NullBuffer,  // a data pointer is null
  BadSize,     // a width or height is 0 or > kMaxSize, or a stride is below width * 4
  BadFilter,   // Filter outside the enum
  Overlap,     // src and dst byte ranges intersect (the rows of both, full width, compared as integer address ranges)
  BufferTooSmall  // build_mip_chain: capacity is smaller than mip_chain_bytes
};
// Lower-case spaced enum names ("ok", "null buffer", "bad size", "bad filter", "overlap", "buffer too small"), "unknown" otherwise.
const char* status_text(Status s);

// Destination pixel (i, j) of a W x H destination from a w x h source. Centres are mapped exactly in integers:
//   source position (Q16) sx = ((2 * i + 1) * w * 65536) / (2 * W) - 32768, with the division of non-negative integers
//   rounded down; sy likewise. Nearest: x = floor(((2 * i + 1) * w) / (2 * W)) (and y likewise), always inside 0..w-1.
//   Bilinear: x0 = floor(sx / 65536) (floor toward minus infinity), fraction fx = sx - x0 * 65536 in 0..65535, taps x0 and
//   x0 + 1 clamped into 0..w-1, weights (65536 - fx) and fx; the 2D result is sum / 2^32 rounded half up, ONE rounding.
//   Box: dst pixel i covers the source interval [i * w / W, (i + 1) * w / W); the weight of source pixel k is the exact overlap,
//   in units of 1 / W source pixels, computed as integers; the 2D weight is the product of both axes; the result is
//   sum(weight * value) / (w * h) rounded half up, ONE rounding, exact in 64 bits.
//   CatmullRom: taps x0 - 1 .. x0 + 2 with weights from the Catmull-Rom cubic evaluated EXACTLY in integers at fx (the weight
//   formulas, their Q16 rounding and the rule that the four weights sum to exactly 65536 are defined by the contributor in
//   docs/resample.md, with a proof of the bounds); the 2D result is the double sum / 2^32 rounded half up, then clamped to 0..255.
// For every filter the last step clamps each colour channel to the result alpha, so the output satisfies R, G, B <= A. The clamp
// wins over any identity: Nearest, Box and Bilinear at identical size are byte exact copies only for valid premultiplied pixels
// (R, G, B <= A); a malformed pixel is clamped by every filter alike (so Box equals Nearest at identical size in all cases).
// Catmull-Rom weights: the symmetric rule is used, the rounding correction that makes the sum exactly 65536 goes into the
// centre tap w1 for fx <= 32768 and into w2 otherwise; the docs must bound the correction and prove |sum| fits 64 bits.
// Checks in enum order (NullBuffer, BadSize, BadFilter, Overlap). Nothing is written on error. Stride padding is never written.
Status resize(const ConstSurface& src, const Surface& dst, Filter filter);

// Mip chain. Level 0 is the source size; level n + 1 has size max(1, w >> 1) x max(1, h >> 1) and is the Box resize of level n.
// mip_count(w, h) is 1 + floor(log2(max(w, h))) (0 for a zero or oversized size). mip_level_size writes the size of one level
// and returns false when the level is out of range. mip_chain_bytes is the total of w * h * 4 over all levels (0 on bad size).
std::uint32_t mip_count(std::uint32_t width, std::uint32_t height);
bool mip_level_size(std::uint32_t width, std::uint32_t height, std::uint32_t level, std::uint32_t* out_w, std::uint32_t* out_h);
std::uint64_t mip_chain_bytes(std::uint32_t width, std::uint32_t height);

// Writes all levels tightly packed (stride = level width * 4), level 0 first as a literal byte copy of the source rows (no
// clamp); every later level is the Box resize of the previous tight level, clamped as above. Overlap is decided on the
// bytes [out, out + mip_chain_bytes) against the source rows, whatever the capacity, and comes before BufferTooSmall. Checks: NullBuffer
// (src.data or out null), BadSize, Overlap (out range against src rows), BufferTooSmall (capacity < mip_chain_bytes).
Status build_mip_chain(const ConstSurface& src, std::uint8_t* out, std::size_t capacity);

// FNV-1a 64 over the bytes of every row, width * 4 bytes per row, top to bottom (stride padding excluded). A surface with null
// data or a zero width or height hashes zero bytes (the FNV offset basis).
std::uint64_t digest_surface(const ConstSurface& s);

}  // namespace lumora::resample
