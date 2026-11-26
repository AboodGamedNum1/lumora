// Copies an 8-bit coverage bitmap (a rasterized glyph) into an 8-bit atlas buffer at a position chosen by core/shelfpack, and clears the
// padding cells that core/shelfpack keeps free. Integer only: no floating point, no libm, no 128-bit types (MSVC), no allocation, no
// global state. It does not draw, blend or convert pixels. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::glyphblit {

constexpr std::uint32_t kMaxDim = 4096;      // largest width or height of a bitmap or of the atlas
constexpr std::uint32_t kMaxStride = 65536;  // largest row stride in bytes

enum class BlitResult : std::uint8_t { Ok = 0, NullArgument = 1, BadSize = 2, BadStride = 3, OutOfBounds = 4, Overlap = 5 };

// Copies the w x h bitmap src (row r starts at src + r * src_stride) into dst (a dst_w x dst_h atlas, row r starts at dst + r * dst_stride)
// with its top left pixel at (x, y). The caller guarantees that src has at least (h - 1) * src_stride + w readable bytes and dst has at
// least (dst_h - 1) * dst_stride + dst_w writable bytes (the function cannot check this).
// Check order, each failure writes nothing: src or dst null gives NullArgument; w, h, dst_w or dst_h equal to 0 or above kMaxDim gives
// BadSize; src_stride < w, dst_stride < dst_w, or either stride above kMaxStride gives BadStride; x + w > dst_w or y + h > dst_h (computed
// in uint64) gives OutOfBounds; then Overlap.
// Overlap: with wp = min(w + 1, dst_w - x) and hp = min(h + 1, dst_h - y), the source span is the half-open byte range
// [src, src + (h - 1) * src_stride + w) and the destination span is [dst + y * dst_stride + x, dst + (y + hp - 1) * dst_stride + x + wp),
// both compared as uintptr_t in 64-bit arithmetic. Spans that only touch are allowed; any intersection is Overlap, even if the rows
// would not really collide (the destination span is a conservative bounding range).
// On Ok exactly these dst bytes are written and no others: the w x h block takes the src pixels; then, if x + w < dst_w, the cell right of
// each of the h rows (column x + w) is set to 0; then, if y + h < dst_h, the row below (row y + h, columns x to x + wp - 1, which includes
// the corner cell when x + w < dst_w) is set to 0. x and y have no range check of their own: any uint32 is accepted until the uint64 sums x + w and y + h decide
// OutOfBounds, so x = UINT32_MAX is OutOfBounds, never a wrap. The padded size wp and hp shrinks at the right and bottom atlas edge, so
// no cell outside the atlas is part of the destination span or written. Overlap is decided only after OutOfBounds, by comparing integer
// addresses (no pointer outside a buffer is ever formed). image_digest reads only the bytes through (h - 1) * stride + w (never stride
// padding or extra capacity) and returns the offset basis for any invalid argument even when pixels is not null.
BlitResult blit_coverage(const std::uint8_t* src, std::uint32_t w, std::uint32_t h, std::uint32_t src_stride, std::uint8_t* dst,
                         std::uint32_t dst_w, std::uint32_t dst_h, std::uint32_t dst_stride, std::uint32_t x, std::uint32_t y);

// FNV-1a 64 (byte-wise, offset basis start, prime 0x100000001b3) over: w then h as 32-bit little-endian words (each least significant byte
// first), then the w pixel bytes of each row from the top; stride padding is never hashed. A null pointer, w or h equal to 0 or above
// kMaxDim, or stride < w or above kMaxStride gives the offset basis. The caller guarantees (h - 1) * stride + w readable bytes.
std::uint64_t image_digest(const std::uint8_t* pixels, std::uint32_t w, std::uint32_t h, std::uint32_t stride);

}  // namespace lumora::glyphblit
