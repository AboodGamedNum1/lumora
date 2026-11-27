// Rank and select over a bit vector: how many bits are set before a position, and where the k-th set bit is. Pure integer: no floating point,
// no libm, no 128-bit types, NO compiler builtins or intrinsics (no popcount builtin: MSVC), no allocation, no recursion, no global state.
// The directory is caller storage. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::bitrank {

constexpr std::uint32_t kMaxBits = 1u << 24;  // largest bit count

enum class BitResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, OutOfRange = 4 };

// Bit layout: the vector is bits[0 .. (nbits + 63) / 64) of 64-bit words; bit position p (0 to nbits-1) is bit (p % 64) of word p / 64, counted
// from the least significant bit. Bits of the LAST word at positions nbits and above (when nbits is not a multiple of 64) are NOT part of the
// vector: they may hold any value and must never be counted. nbits 0 is the empty vector (no words are read).

// *out_words = the directory size in u32 words: (nbits + 63) / 64 + 1 (so 1 for nbits 0). Check order: a null out_words gives NullArgument;
// then nbits above kMaxBits gives TooLarge (nothing written).
BitResult bitrank_dir_words(std::uint32_t nbits, std::uint32_t* out_words);

// Builds the directory dir[0 .. need), need = the value of bitrank_dir_words. The contents are UNSPECIFIED: use them only through the two
// queries below, and the bit vector must not change between the build and the queries.
// Check order: a null pointer argument (bits only while nbits is above 0, dir always) gives NullArgument; then nbits above kMaxBits gives
// TooLarge; then dir_words below the need gives ScratchTooSmall. On any failure dir is left untouched.
BitResult bitrank_build(const std::uint64_t* bits, std::uint32_t nbits, std::uint32_t* dir, std::uint32_t dir_words);

// *out = the number of set bits among the positions 0 .. i-1 (positions below i). i from 0 to nbits is valid (i = nbits gives the total);
// i above nbits is OutOfRange. Check order: a null pointer argument (bits only while nbits is above 0, dir and out always) gives NullArgument;
// then nbits above kMaxBits gives TooLarge; then dir_words below the need gives ScratchTooSmall; then OutOfRange. On any failure *out is
// untouched. The directory must come from bitrank_build of the same unchanged vector and the same nbits.
BitResult bitrank_rank1(const std::uint64_t* bits, std::uint32_t nbits, const std::uint32_t* dir, std::uint32_t dir_words, std::uint32_t i,
                        std::uint32_t* out);

// *out_pos = the position of the k-th set bit, counting from k = 0 (so k 0 is the lowest set position). k at or above the total number of set
// bits is OutOfRange (every k on an empty or all-zero vector). Same check order, directory rule and failure rule as bitrank_rank1.
BitResult bitrank_select1(const std::uint64_t* bits, std::uint32_t nbits, const std::uint32_t* dir, std::uint32_t dir_words, std::uint32_t k,
                          std::uint32_t* out_pos);

// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.

}  // namespace lumora::bitrank
