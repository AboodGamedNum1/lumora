// Calkin-Wilf tree and Stern's diatomic sequence (contract in calkinwilf.h). Integer only: no heap, no recursion, no mutable static, no array, no 128-bit type, no builtin.
// Method and proofs: docs/calkinwilf.md.
//   fusc: a = 1, b = 0; for the bits of n from the low end, a bit 1 does b += a and a bit 0 does a += b; the result is b (the value s(n)).
//   calkin_wilf: the node n is s(n) / s(n + 1).
//   calkin_wilf_index: walk p / q up to 1/1 in runs (quotients, not single subtractions); a run of t steps with p > q is t one-bits of the index, with p < q t zero-bits; the
//   index is 2^S + (the S path bits, the leaf-most step lowest) where S is the number of steps; it fits a u64 exactly when S <= 63.
#include "calkinwilf.h"

namespace lumora::calkinwilf {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;

u64 fusc_value(u64 n) {
  u64 a = 1u;
  u64 b = 0u;
  for (; n != 0u; n >>= 1) {
    if ((n & 1u) != 0u) {
      b += a;
    } else {
      a += b;
    }
  }
  return b;
}

u32 gcd32(u32 a, u32 b) {
  while (b != 0u) {
    const u32 t = a % b;
    a = b;
    b = t;
  }
  return a;
}
}  // namespace

CwResult fusc(std::uint64_t n, std::uint64_t* out) {
  if (out == nullptr) return CwResult::NullArgument;
  *out = fusc_value(n);
  return CwResult::Ok;
}

CwResult calkin_wilf(std::uint64_t n, std::uint64_t* num, std::uint64_t* den) {
  if (num == nullptr || den == nullptr) return CwResult::NullArgument;
  if (n == 0u) return CwResult::BadArgument;
  if (n > kMaxNode) return CwResult::TooLarge;
  *num = fusc_value(n);
  *den = fusc_value(n + 1u);
  return CwResult::Ok;
}

CwResult calkin_wilf_index(std::uint32_t p, std::uint32_t q, std::uint64_t* out) {
  if (out == nullptr) return CwResult::NullArgument;
  if (p == 0u || q == 0u) return CwResult::BadArgument;
  if (gcd32(p, q) != 1u) return CwResult::NotCoprime;
  u64 x = p;
  u64 y = q;
  u64 steps = 0u;
  u64 bits = 0u;
  while (x != 1u || y != 1u) {
    const bool right = x > y;
    u64 run = 0u;
    if (right) {
      run = (y == 1u) ? x - 1u : x / y;
    } else {
      run = (x == 1u) ? y - 1u : y / x;
    }
    if (run > 63u - steps) return CwResult::TooLarge;
    if (right) {
      bits |= ((u64{1} << run) - 1u) << steps;
      x -= run * y;
    } else {
      y -= run * x;
    }
    steps += run;
  }
  *out = (u64{1} << steps) | bits;
  return CwResult::Ok;
}
}  // namespace lumora::calkinwilf
