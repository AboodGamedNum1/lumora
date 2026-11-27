// Space-filling curve indexes over integer grids: Morton (Z-order) in 2D and 3D, and the Hilbert curve in 2D. Pure integer: no floating point,
// no libm, no 128-bit types or builtins (MSVC), no allocation, no recursion, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::zorder {

constexpr std::uint32_t kMaxHilbertOrder = 16;  // a Hilbert grid is 2^order by 2^order cells, so the index has at most 32 bits
constexpr std::uint32_t kMax3dCoord = 1023;     // 3D Morton coordinates have 10 bits each

enum class ZResult : std::uint8_t { Ok = 0, NullArgument = 1, BadOrder = 2, OutOfRange = 3 };

// MORTON 2D: bit i of x goes to bit 2i of the code, bit i of y to bit 2i + 1 (x is the low bit of each pair). Every 16-bit x and y is valid.
std::uint32_t morton2_encode(std::uint16_t x, std::uint16_t y);

// Inverse of morton2_encode: every 32-bit code is valid. NullArgument when x or y is null (nothing written).
ZResult morton2_decode(std::uint32_t code, std::uint16_t* x, std::uint16_t* y);

// MORTON 3D: bit i of x goes to bit 3i, of y to bit 3i + 1, of z to bit 3i + 2; the code uses the low 30 bits. A coordinate above kMax3dCoord
// gives OutOfRange (checked after null checks; the out pointer is left untouched).
ZResult morton3_encode(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t* out);

// Inverse of morton3_encode. A code with any of bits 30 and 31 set gives OutOfRange. NullArgument when an out pointer is null.
ZResult morton3_decode(std::uint32_t code, std::uint32_t* x, std::uint32_t* y, std::uint32_t* z);

// HILBERT CURVE of the given order (1 .. kMaxHilbertOrder) on the grid 0 .. 2^order - 1 in x and y, with the usual convention in which, for
// order 1, the distances 0, 1, 2, 3 visit the cells (0,0), (0,1), (1,1), (1,0) in this order (x first). The curve of order k is the standard
// one used by the well-known xy2d and d2xy routines (the rotation and flip step per level); adjacent distances are always adjacent cells.
// Check order: NullArgument (out pointers), BadOrder (order 0 or above kMaxHilbertOrder), OutOfRange (a coordinate at or above 2^order, or a
// distance at or above 4^order). On failure out pointers are left untouched.
ZResult hilbert_xy2d(std::uint32_t order, std::uint32_t x, std::uint32_t y, std::uint32_t* out_d);
ZResult hilbert_d2xy(std::uint32_t order, std::uint32_t d, std::uint32_t* out_x, std::uint32_t* out_y);

// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.

}  // namespace lumora::zorder
