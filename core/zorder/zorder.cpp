// Morton (Z-order) codes in 2D and 3D, and the Hilbert curve in 2D. See zorder.h for the contract. Integer only: no allocation, no
// recursion, no global state.
#include "zorder.h"

namespace lumora::zorder {

namespace {

using u32 = std::uint32_t;

// Spreads the 16 low bits of v so that bit i moves to bit 2i.
u32 spread2(u32 v) {
  v &= 0x0000FFFFu;
  v = (v | (v << 8)) & 0x00FF00FFu;
  v = (v | (v << 4)) & 0x0F0F0F0Fu;
  v = (v | (v << 2)) & 0x33333333u;
  v = (v | (v << 1)) & 0x55555555u;
  return v;
}

// Inverse of spread2: gathers the even bits of v into the low 16 bits.
u32 gather2(u32 v) {
  v &= 0x55555555u;
  v = (v | (v >> 1)) & 0x33333333u;
  v = (v | (v >> 2)) & 0x0F0F0F0Fu;
  v = (v | (v >> 4)) & 0x00FF00FFu;
  v = (v | (v >> 8)) & 0x0000FFFFu;
  return v;
}

// Spreads the 10 low bits of v so that bit i moves to bit 3i.
u32 spread3(u32 v) {
  v &= 0x000003FFu;
  v = (v | (v << 16)) & 0x030000FFu;
  v = (v | (v << 8)) & 0x0300F00Fu;
  v = (v | (v << 4)) & 0x030C30C3u;
  v = (v | (v << 2)) & 0x09249249u;
  return v;
}

// Inverse of spread3: gathers bits 0, 3, 6, ... of v into the low 10 bits.
u32 gather3(u32 v) {
  v &= 0x09249249u;
  v = (v | (v >> 2)) & 0x030C30C3u;
  v = (v | (v >> 4)) & 0x0300F00Fu;
  v = (v | (v >> 8)) & 0x030000FFu;
  v = (v | (v >> 16)) & 0x000003FFu;
  return v;
}

// One rotate-and-flip step of the Hilbert routines on a square of side n.
void rotate(u32 n, u32& x, u32& y, u32 rx, u32 ry) {
  if (ry == 0) {
    if (rx == 1) {
      x = n - 1u - x;
      y = n - 1u - y;
    }
    const u32 t = x;
    x = y;
    y = t;
  }
}

}  // namespace

std::uint32_t morton2_encode(std::uint16_t x, std::uint16_t y) { return spread2(x) | (spread2(y) << 1); }

ZResult morton2_decode(std::uint32_t code, std::uint16_t* x, std::uint16_t* y) {
  if (x == nullptr || y == nullptr) return ZResult::NullArgument;
  *x = static_cast<std::uint16_t>(gather2(code));
  *y = static_cast<std::uint16_t>(gather2(code >> 1));
  return ZResult::Ok;
}

ZResult morton3_encode(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t* out) {
  if (out == nullptr) return ZResult::NullArgument;
  if (x > kMax3dCoord || y > kMax3dCoord || z > kMax3dCoord) return ZResult::OutOfRange;
  *out = spread3(x) | (spread3(y) << 1) | (spread3(z) << 2);
  return ZResult::Ok;
}

ZResult morton3_decode(std::uint32_t code, std::uint32_t* x, std::uint32_t* y, std::uint32_t* z) {
  if (x == nullptr || y == nullptr || z == nullptr) return ZResult::NullArgument;
  if ((code >> 30) != 0) return ZResult::OutOfRange;
  *x = gather3(code);
  *y = gather3(code >> 1);
  *z = gather3(code >> 2);
  return ZResult::Ok;
}

ZResult hilbert_xy2d(std::uint32_t order, std::uint32_t x, std::uint32_t y, std::uint32_t* out_d) {
  if (out_d == nullptr) return ZResult::NullArgument;
  if (order == 0 || order > kMaxHilbertOrder) return ZResult::BadOrder;
  const u32 n = 1u << order;
  if (x >= n || y >= n) return ZResult::OutOfRange;
  u32 d = 0;
  for (u32 s = n / 2u; s > 0; s /= 2u) {
    const u32 rx = (x & s) != 0 ? 1u : 0u;
    const u32 ry = (y & s) != 0 ? 1u : 0u;
    d += s * s * ((3u * rx) ^ ry);
    rotate(n, x, y, rx, ry);
  }
  *out_d = d;
  return ZResult::Ok;
}

ZResult hilbert_d2xy(std::uint32_t order, std::uint32_t d, std::uint32_t* out_x, std::uint32_t* out_y) {
  if (out_x == nullptr || out_y == nullptr) return ZResult::NullArgument;
  if (order == 0 || order > kMaxHilbertOrder) return ZResult::BadOrder;
  if (order < kMaxHilbertOrder && (d >> (2u * order)) != 0) return ZResult::OutOfRange;
  const u32 n = 1u << order;
  u32 x = 0;
  u32 y = 0;
  u32 t = d;
  for (u32 s = 1; s < n; s *= 2u) {
    const u32 rx = 1u & (t / 2u);
    const u32 ry = 1u & (t ^ rx);
    rotate(s, x, y, rx, ry);
    x += s * rx;
    y += s * ry;
    t /= 4u;
  }
  *out_x = x;
  *out_y = y;
  return ZResult::Ok;
}

}  // namespace lumora::zorder
