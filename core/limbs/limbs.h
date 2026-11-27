// Arbitrary-length unsigned integers as little-endian arrays of 32-bit limbs: compare, add, subtract, multiply, divide by a 32-bit value.
// Pure integer: no floating point, no libm, no 128-bit types or builtins (MSVC; use 64-bit intermediates), no allocation, no recursion, no
// global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::limbs {

constexpr std::uint32_t kMaxLimbs = 1u << 16;  // largest limb count of any one input array

enum class LimbResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, Underflow = 3, BufferTooSmall = 4, DivideByZero = 5 };

// NUMBERS: a number is x[0 .. n), limb 0 is the least significant 32 bits. Inputs may carry zero limbs at the high end (x[n-1] == 0) and n
// may be 0 (the value 0). OUTPUTS ARE NORMALIZED: no zero limb at the high end, so the value 0 has length 0 and writes nothing. The
// output length is a function of the value only. An output array must not overlap an input array or an out pointer.
//
// Check order for every function: a null pointer argument gives NullArgument (an input array counts only while its length is above 0; an
// output array counts only while its capacity is above 0; every out pointer always counts); then any input length above kMaxLimbs gives
// TooLarge; then the function-specific failure in the order listed (Underflow, DivideByZero), then BufferTooSmall when the capacity is below
// the normalized result length. On any failure nothing is written (out pointers are left untouched).

// *out = -1, 0 or 1 when a is less than, equal to or greater than b (as numbers; leading zero limbs do not matter). Reads only.
LimbResult limbs_cmp(const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::int32_t* out);

// dst = a + b. *out_len = normalized length.
LimbResult limbs_add(const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::uint32_t* dst,
                     std::uint32_t dst_cap, std::uint32_t* out_len);

// dst = a - b. Underflow when a is less than b (before BufferTooSmall). *out_len = normalized length.
LimbResult limbs_sub(const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::uint32_t* dst,
                     std::uint32_t dst_cap, std::uint32_t* out_len);

// dst = a * b (any correct method). *out_len = normalized length (0 when either factor is 0).
LimbResult limbs_mul(const std::uint32_t* a, std::uint32_t na, const std::uint32_t* b, std::uint32_t nb, std::uint32_t* dst,
                     std::uint32_t dst_cap, std::uint32_t* out_len);

// q = a / d (integer division, rounded down) and *rem = a % d, for a 32-bit divisor d. DivideByZero when d is 0 (before BufferTooSmall).
// *q_len = normalized length of q.
LimbResult limbs_divmod_u32(const std::uint32_t* a, std::uint32_t na, std::uint32_t d, std::uint32_t* q, std::uint32_t q_cap,
                            std::uint32_t* q_len, std::uint32_t* rem);

// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.

}  // namespace lumora::limbs
