// Monotonic stack and deque queries over an array of u32: nearest smaller neighbours, the largest rectangle under a histogram and the leftmost
// minimum of every sliding window.
// Pure integer: no floating point, no libm, no 128-bit types (no __int128, no compiler builtins or intrinsics: MSVC), no allocation, no recursion, no
// global state, no large automatic arrays (working memory is caller scratch). Every function must take O(n) steps (each element enters and leaves the
// stack or deque at most once); a quadratic method is not acceptable for the largest input. Evidence label: unit tests only.
#pragma once
#include <cstdint>
namespace lumora::monostack {
constexpr std::uint32_t kMaxN = 1048576u;       // largest array length (2^20)
constexpr std::uint32_t kNone = 0xFFFFFFFFu;    // "no such index"
enum class StackResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, ScratchTooSmall = 3, BadWindow = 4 };
// Every entry of a is any u32. Indices are 0 .. n-1. All scratch sizes below are in u32 words; scratch is working memory with unspecified contents
// afterwards (not tested) and needs only the alignment of u32. Output arrays and scratch must not overlap a or each other (outside the contract, not
// tested). Nothing is written on any failure.
//
// monostack_scratch_words: *out_words = n (the scratch size every function below needs; 0 for n = 0). Check order: a null out_words gives
// NullArgument; then n above kMaxN gives TooLarge (nothing written).
StackResult monostack_scratch_words(std::uint32_t n, std::uint32_t* out_words);
// monostack_bounds: for every i, left[i] = the largest j below i with a[j] < a[i] (strictly smaller), or kNone if there is none; right[i] = the
// smallest j above i with a[j] <= a[i] (smaller or EQUAL), or kNone if there is none. left and right each hold n entries.
// Check order: a null pointer among a, scratch, left and right (each only while n is above 0) gives NullArgument; then n above kMaxN gives TooLarge
// (the condition uses the given n even when above the limit); then, only while n is above 0, scratch_words below n gives ScratchTooSmall. n = 0 is
// Ok and writes nothing.
StackResult monostack_bounds(const std::uint32_t* a, std::uint32_t n, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* left,
                             std::uint32_t* right);
// monostack_max_rect: the histogram has bars a[0] .. a[n-1] of width 1. A rectangle is a range l .. r (inclusive, 0 <= l <= r < n) whose height is the
// smallest entry of the range, so its area is (r - l + 1) * min(a[l .. r]), at most 2^20 * (2^32 - 1) and fitting u64. *out_area = the largest area
// over all ranges; among ranges with that area, (*out_l, *out_r) is the one with the smallest l and, among those, the smallest r. For n = 0 the result
// is *out_area = 0, *out_l = 0, *out_r = 0. When every entry is 0 the area is 0 and the range is (0, 0).
// Check order: a null out_area, out_l or out_r (always), or a or scratch (only while n is above 0), gives NullArgument; then n above kMaxN gives
// TooLarge; then, only while n is above 0, scratch_words below n gives ScratchTooSmall.
StackResult monostack_max_rect(const std::uint32_t* a, std::uint32_t n, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint64_t* out_area,
                               std::uint32_t* out_l, std::uint32_t* out_r);
// monostack_window_min: for every window start s = 0 .. n - k (n - k + 1 windows), out[s] = the index of the LEFTMOST minimum of a[s .. s + k - 1].
// out holds n - k + 1 entries. A window size k of 0, or above n while n is above 0, gives BadWindow; for n = 0 every k is accepted and nothing is
// written.
// Check order: a null pointer among a, scratch and out (each only while n is above 0) gives NullArgument; then n above kMaxN gives TooLarge; then, only
// while n is above 0, k equal to 0 or above n gives BadWindow; then, only while n is above 0, scratch_words below n gives ScratchTooSmall.
StackResult monostack_window_min(const std::uint32_t* a, std::uint32_t n, std::uint32_t k, std::uint32_t* scratch, std::uint32_t scratch_words,
                                 std::uint32_t* out);
// Decisions: the method is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not an
// interpretation.
}  // namespace lumora::monostack
