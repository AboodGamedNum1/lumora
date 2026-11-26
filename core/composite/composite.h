// Lumora CPU compositing and blur (scope P). Pure CPU, deterministic, no I/O, no threads, no floating point at all.
// The interface in this header is fixed; the contributor implements core/composite/composite.cpp only (plus tests and docs).
// Results must be bit-identical on Linux, Windows and Android: integer arithmetic only (64-bit where needed), no libm, no
// floats, no unordered containers, no pointer values in any decision. Purpose: the CPU reference for blend modes, mask
// compositing (for example core/raster masks) and post-effect blur. Evidence label: software (CPU) only.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::composite {

// Pixels are RGBA8 with PREMULTIPLIED alpha, bytes in the order R, G, B, A. Invariant for valid pixels: R, G, B <= A.
// Functions never check that invariant on input; they must still produce defined output for any bytes (see below).
constexpr std::uint32_t kMaxSize = 8192;  // width and height of a surface
constexpr std::uint32_t kMaxRadius = 255;

struct Surface {
  std::uint8_t* data;  // row j starts at data + j * stride; stride is in bytes and >= width * 4
  std::uint32_t width, height, stride;
};
struct ConstSurface {
  const std::uint8_t* data;
  std::uint32_t width, height, stride;
};
struct Mask {  // 8-bit coverage, one byte per pixel
  const std::uint8_t* data;
  std::uint32_t width, height, stride;  // stride >= width
};

enum class Status : std::uint8_t {
  Ok = 0,
  NullBuffer,  // a required data pointer is null
  BadSize,     // width or height is 0 or > kMaxSize, or a stride is too small
  BadMode,     // BlendMode outside the enum
  BadRadius,   // radius > kMaxRadius
  Overlap,     // src and dst memory ranges overlap (compared by address ranges of the rows actually touched)
};
const char* status_text(Status s);

enum class BlendMode : std::uint8_t {
  Src = 0, SrcOver, DstOver, SrcIn, DstIn, SrcOut, DstOut, SrcAtop, DstAtop, Xor, Plus, Multiply, Screen,
};

// Exact rounding division by 255 of v in 0..65025 * 4: div255(v) = (v + 128 + ((v + 128) >> 8)) >> 8. For v = a * b with
// a, b in 0..255 this is the correctly rounded a * b / 255 (half rounds up). Every product of two byte values below uses it,
// one rounding per product, in the order written in docs/composite.md. The function is defined for every uint32 input
// (evaluate the formula in 64 bits); it is the correctly rounded value only up to 65025.
std::uint32_t div255(std::uint32_t v);

// Conversions between straight (non premultiplied) and premultiplied pixels, per pixel, 4 bytes in and out.
// premultiply: c' = div255(c * a). unpremultiply: if a == 0 the result is 0,0,0,0; else c' = min(255, (c * 255 + a / 2) / a).
void premultiply(const std::uint8_t in[4], std::uint8_t out[4]);
void unpremultiply(const std::uint8_t in[4], std::uint8_t out[4]);

// Composites src onto dst with its top-left corner at (dx, dy) in dst coordinates (may be negative or beyond the edges; the
// overlap is clipped exactly). Every channel, alpha included, uses the same per-mode formula with S and D the channel values
// (alpha: S = Sa, D = Da), one div255 per product, products rounded separately, then summed, clamped to 0..255; Multiply and
// Screen use the SrcOver rule for alpha. As a last step every colour channel is clamped to the result alpha, so the output
// always satisfies R, G, B <= A (inputs that break the invariant are used as supplied in the formulas; no read or write is
// out of range). `opacity` (0..255) scales the source first: s' = div255(s * opacity) per channel including
// alpha. Then the mode formula of docs/composite.md is applied with Sa, Da the alpha values and results clamped to 0..255.
// Checks in enum order (NullBuffer, BadSize, BadMode, Overlap). Nothing is written on error. Overlap is decided on the bytes
// actually touched: the byte ranges of the clipped rows of dst and of src. It is reported when any touched dst row range
// intersects any touched src row range (address ranges compared as integers, only for this status); regions that sit side by
// side in one buffer without intersecting are valid. A placement that is clipped away completely returns Ok without writes,
// even when dst and src alias.
// Coordinates dx, dy are int32 and may be anything; a non-overlapping placement is Ok and writes nothing.
Status blend(const Surface& dst, const ConstSurface& src, std::int32_t dx, std::int32_t dy, BlendMode mode, std::uint8_t opacity);

// Fills with one premultiplied color through a coverage mask: the mask value m scales the color like opacity does
// (c' = div255(c * m)), then the same mode formulas apply. The mask is placed at (dx, dy) in dst coordinates, clipped
// exactly; pixels outside the mask are untouched. Check order: NullBuffer, BadSize (including a stride that is too short),
// BadMode, then Overlap, decided like blend on the touched mask bytes and touched dst bytes. The clamp of colour to alpha
// described for blend applies here too. A mask byte of 0 leaves dst unchanged for every mode except where the
// mode formula itself changes dst when Sa = 0 (Src, SrcIn, SrcOut, DstIn, DstAtop clear or change dst; docs/composite.md lists
// each mode at Sa = 0, and the test must cover them).
Status fill_mask(const Surface& dst, const Mask& mask, std::int32_t dx, std::int32_t dy, const std::uint8_t color[4], BlendMode mode);

// Box blur of the whole surface, all four channels independently. Horizontal pass first, then vertical, each with an
// integer window of 2 * r + 1 samples, edge pixels clamped (the first and last pixel repeated outside). Output per sample:
// (sum + window / 2) / window with window = 2 * r + 1 (integer division). Radius 0 in an axis leaves that axis unchanged.
// The intermediate image after the horizontal pass is stored with 8-bit rounding (the rule above), not kept at higher
// precision. `scratch` is reserved: any value, null included, is valid and it is never written. The implementation must not
// need more than O(width + height) bytes of temporary storage, held in fixed size local arrays (a row is at most 8192 * 4
// bytes), so there is no allocation and no failure path. The horizontal pass works in place row by row and the vertical pass
// column by column, which gives the same bytes as a full intermediate image. Its output must equal the naive definition.
// Checks: NullBuffer, BadSize, BadRadius.
Status box_blur(const Surface& s, std::uint32_t radius_x, std::uint32_t radius_y, std::uint8_t* scratch);

// Three successive box blurs with the same radii (a close approximation of a Gaussian). Same rules and checks as box_blur.
Status blur3(const Surface& s, std::uint32_t radius_x, std::uint32_t radius_y, std::uint8_t* scratch);

// FNV-1a 64 over the bytes of every row, width * 4 bytes per row, top to bottom (stride padding excluded), for golden tests.
// A surface with null data or a zero width or height hashes zero bytes (the FNV offset basis).
std::uint64_t digest_surface(const ConstSurface& s);

}  // namespace lumora::composite
