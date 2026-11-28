// The Pisano period: the length of the period of the Fibonacci numbers modulo m (F(0) = 0, F(1) = 1, F(n + 2) = F(n + 1) + F(n)), that is the smallest n > 0
// with F(n) = 0 and F(n + 1) = 1 modulo m.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no arrays. A call takes O(sqrt(m) + (number of prime factors) * log m) steps or better, with O(1) automatic memory; walking the sequence
// until it repeats (O(period) steps, the period can be 6 m) is not acceptable for m = kMaxM.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::pisano {
constexpr std::uint32_t kMaxM = 4294967295u;  // largest modulus m (2^32 - 1, every u32 above 0)
enum class PisanoResult : std::uint8_t { Ok = 0, NullArgument = 1, BadModulus = 2 };
// pisano_period: *out = the Pisano period pi(m) for every m from 1 to kMaxM (a u64: pi(m) <= 6 m can exceed 2^32). pi(1) = 1. Examples: pi(2) = 3, pi(3) = 8,
// pi(4) = 6, pi(5) = 20, pi(7) = 16, pi(10) = 60, pi(11) = 10, pi(13) = 28, pi(100) = 300, pi(1000) = 1500, pi(10^6) = 1500000.
// Check order: a null out gives NullArgument; then m = 0 gives BadModulus. Nothing is written to *out on a failure.
PisanoResult pisano_period(std::uint32_t m, std::uint64_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::pisano
