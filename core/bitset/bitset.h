// Fixed-size bit sets over a caller array of 64-bit words (visibility masks, occupancy maps, dirty flags). Pure integer: no floating
// point, no libm, no compiler builtins or intrinsics (MSVC), no 128-bit types, no allocation, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::bitset {

constexpr std::uint32_t kMaxBits = 1u << 30;  // largest set, in bits

// Bit i (0-based, i < nbits) is bit (i % 64) of word (i / 64), where bit 0 is the least significant. The caller supplies words[0 .. W) with
// W = (nbits + 63) / 64 (use words_for). The bits of the last word above nbits are padding: no function below reads them into a result or
// changes them (a write to the last word keeps the padding bits it had). Arguments are valid when nbits is at most kMaxBits and words is
// non-null whenever nbits is above 0. An invalid argument makes the function return false and change nothing, including *out. words
// may be null when nbits is 0.

// (nbits + 63) / 64 for nbits up to kMaxBits; 0 for nbits above kMaxBits.
std::uint32_t words_for(std::uint32_t nbits);

// Sets bit i to value. False when the arguments are invalid or i >= nbits. Every other bit keeps its value.
bool set(std::uint64_t* words, std::uint32_t nbits, std::uint32_t i, bool value);

// Reads bit i into *out. False when the arguments are invalid, out is null, or i >= nbits.
bool test(const std::uint64_t* words, std::uint32_t nbits, std::uint32_t i, bool* out);

// Sets every bit below nbits to value; the padding bits are not changed. False when the arguments are invalid. nbits 0 succeeds.
bool fill(std::uint64_t* words, std::uint32_t nbits, bool value);

// Number of set bits below nbits into *out (padding ignored). False when the arguments are invalid or out is null. nbits 0 gives 0.
bool popcount(const std::uint64_t* words, std::uint32_t nbits, std::uint32_t* out);

// Smallest index j with from <= j < nbits whose bit equals value, into *out; when there is none, *out = nbits and the result is still
// true. from == nbits is valid (nothing to search, *out = nbits); from > nbits is invalid. False also when the arguments are invalid or
// out is null. Only the words that cover [from, nbits) are read.
bool find_next(const std::uint64_t* words, std::uint32_t nbits, std::uint32_t from, bool value, std::uint32_t* out);

// Decisions: words and out must not overlap; not checked and not tested. A header ambiguity found later is a question to the maintainer,
// not an interpretation.

}  // namespace lumora::bitset
