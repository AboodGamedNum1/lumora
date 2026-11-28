// Gray codes over u64 (contract in graycode.h).
// Integer only; no heap, arrays, tables, recursion, mutable statics, 128-bit types, builtins or intrinsics. The k-ary functions take at most 16 loop steps with one
// division or multiplication each (see docs/graycode.md for the bounds).
#include "graycode.h"

namespace lumora::graycode {
namespace {
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u64 kMax64 = ~static_cast<u64>(0u);

// k^e for e at most 15 (at most 16^15 = 2^60).
u64 power(u32 k, u32 e) {
  u64 p = 1u;
  for (u32 i = 0u; i < e; ++i) p *= k;
  return p;
}

// 1 when every digit of the n-digit code is below k and no bit is set at position 4n or above. The shift by 4n is taken only for n below 16.
bool valid_code(u32 k, u32 n, u64 code) {
  if (n < 16u && (code >> (4u * n)) != 0u) return false;
  for (u32 i = 0u; i < n; ++i) {
    if (((code >> (4u * i)) & 15u) >= k) return false;
  }
  return true;
}

bool bad_args(u32 k, u32 n) { return k < 2u || k > kMaxBase || n < 1u || n > kMaxDigits; }
}  // namespace

std::uint64_t gray_encode(std::uint64_t x) { return x ^ (x >> 1u); }

// Prefix xor from the top: g ^ g >> 1 ^ g >> 2 ... by doubling shifts (the largest shift is 32).
std::uint64_t gray_decode(std::uint64_t g) {
  g ^= g >> 1u;
  g ^= g >> 2u;
  g ^= g >> 4u;
  g ^= g >> 8u;
  g ^= g >> 16u;
  g ^= g >> 32u;
  return g;
}

GrResult kgray_count(std::uint32_t k, std::uint32_t n, std::uint64_t* out) {
  if (out == nullptr) return GrResult::NullArgument;
  if (bad_args(k, n)) return GrResult::BadArgument;
  u64 p = 1u;
  for (u32 i = 0u; i < n; ++i) {
    if (p > kMax64 / k) return GrResult::Overflow;
    p *= k;
  }
  *out = p;
  return GrResult::Ok;
}

// The top digit is rank / k^(n-1) (it must be below k); the lower digits follow the (n-1)-digit sequence forward when it is even and backward (rank r becomes
// k^(n-1) - 1 - r) when it is odd.
GrResult kgray_unrank(std::uint32_t k, std::uint32_t n, std::uint64_t rank, std::uint64_t* out) {
  if (out == nullptr) return GrResult::NullArgument;
  if (bad_args(k, n)) return GrResult::BadArgument;
  u64 pw = power(k, n - 1u);
  if (rank / pw >= k) return GrResult::RankTooLarge;
  u64 r = rank;
  u64 code = 0u;
  for (u32 i = n; i > 0u; --i) {
    const u64 d = r / pw;
    r -= d * pw;
    code |= d << (4u * (i - 1u));
    if ((d & 1u) != 0u) r = pw - 1u - r;
    pw /= k;
  }
  *out = code;
  return GrResult::Ok;
}

// From the top: the rank digit is the code digit when an even number of the code digits above it are odd, and k - 1 minus it otherwise.
GrResult kgray_rank(std::uint32_t k, std::uint32_t n, std::uint64_t code, std::uint64_t* out) {
  if (out == nullptr) return GrResult::NullArgument;
  if (bad_args(k, n)) return GrResult::BadArgument;
  if (!valid_code(k, n, code)) return GrResult::BadCode;
  u64 rank = 0u;
  u32 odd = 0u;
  for (u32 i = n; i > 0u; --i) {
    const u32 g = static_cast<u32>((code >> (4u * (i - 1u))) & 15u);
    const u32 r = odd == 0u ? g : k - 1u - g;
    rank = rank * k + r;
    odd ^= g & 1u;
  }
  *out = rank;
  return GrResult::Ok;
}

// Digit i runs up when the number of odd digits above it is even and down otherwise. The next code moves the lowest digit that is not at the end of its run by 1.
GrResult kgray_next(std::uint32_t k, std::uint32_t n, std::uint64_t code, std::uint64_t* out) {
  if (out == nullptr) return GrResult::NullArgument;
  if (bad_args(k, n)) return GrResult::BadArgument;
  if (!valid_code(k, n, code)) return GrResult::BadCode;
  u32 above = 0u;
  for (u32 i = 0u; i < n; ++i) above ^= static_cast<u32>((code >> (4u * i)) & 1u);
  for (u32 i = 0u; i < n; ++i) {
    const u32 g = static_cast<u32>((code >> (4u * i)) & 15u);
    above ^= g & 1u;
    if (above == 0u) {
      if (g < k - 1u) {
        *out = code + (static_cast<u64>(1u) << (4u * i));
        return GrResult::Ok;
      }
    } else if (g > 0u) {
      *out = code - (static_cast<u64>(1u) << (4u * i));
      return GrResult::Ok;
    }
  }
  return GrResult::NoNext;
}
}  // namespace lumora::graycode
