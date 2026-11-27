// Range add and range sum over an array of 64-bit values, in caller storage (a lazy segment tree or any method with the same results). Pure
// integer: no floating point, no libm, no 128-bit types, no compiler builtins or intrinsics (MSVC), no allocation, no recursion, no global
// state. Every operation has a worst case of O(log n): no linear method per update or query. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::segtree {
constexpr std::uint32_t kMaxLen = 65536;  // largest array length
enum class SegResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, OutOfRange = 4 };
// Values: the array has n elements, all u64, and every sum and addition WRAPS modulo 2^64 (so the results equal the signed int64 two's
// complement results). The array lives inside the storage after seg_init; the caller passes the same n and the same storage to every later call
// and must not change the storage between calls except through these functions. Contents of the storage are UNSPECIFIED and not tested.
// Ranges are half-open: [l, r) is the elements l .. r-1, valid when l <= r <= n. An empty range (l == r) is valid.
//
// *out_words = the storage size in u64 words that every function below needs: 8 * n (0 for n = 0). Check order: a null out_words gives
// NullArgument; then n above kMaxLen gives TooLarge (nothing written).
SegResult seg_storage_words(std::uint32_t n, std::uint32_t* out_words);
// Stores values[0 .. n) in the storage. A null storage is an error only while the need is above 0; values only while n is above 0.
// Check order: a null pointer argument gives NullArgument; then n above kMaxLen gives TooLarge; then storage_words below the need gives
// ScratchTooSmall. On any failure the storage is untouched. n = 0 is valid and writes nothing.
SegResult seg_init(const std::uint64_t* values, std::uint32_t n, std::uint64_t* storage, std::uint32_t storage_words);
// Adds delta to each element of [l, r) (wrapping). Check order: a null storage (while the need is above 0) gives NullArgument; then n above
// kMaxLen gives TooLarge; then storage_words below the need gives ScratchTooSmall; then a range with l above r or r above n gives OutOfRange.
// On any failure the storage is untouched. An empty valid range is Ok and changes no element.
SegResult seg_range_add(std::uint64_t* storage, std::uint32_t storage_words, std::uint32_t n, std::uint32_t l, std::uint32_t r, std::uint64_t delta);
// *out = the sum (wrapping) of the elements of [l, r); 0 for an empty range. Same check order as seg_range_add, with a null out giving
// NullArgument in the first step. On any failure *out is untouched. The storage must have come from seg_init with the same n.
SegResult seg_range_sum(std::uint64_t* storage, std::uint32_t storage_words, std::uint32_t n, std::uint32_t l, std::uint32_t r, std::uint64_t* out);
// Decisions: the algorithm is free as long as the contract above holds (a query may use the storage as working memory, which is why storage
// is not const). A header ambiguity found later is a question to the maintainer, not an interpretation.
}  // namespace lumora::segtree
