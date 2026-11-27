// Binary min-heap of (key, value) pairs of 32-bit unsigned integers, stored in caller arrays. Pure integer: no floating point, no libm, no
// 128-bit types or builtins (MSVC), no allocation, no recursion, no global state. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::minheap {

constexpr std::uint32_t kMaxCapacity = 1u << 26;  // largest capacity

enum class HeapResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, Full = 3, Empty = 4, BadSize = 5 };

// The heap lives in keys[0 .. *size) and values[0 .. *size); the pair at index i is (keys[i], values[i]). *size is the only other state. A
// heap starts empty with *size 0 (the caller sets it; contents of the arrays beyond *size are never read). The two arrays must have
// capacity elements each and must not overlap each other or *size.
//
// ORDER: pairs compare lexicographically, key first, then value. The pair that pops first is the smallest by that order, so the sequence
// of popped pairs is fully determined by the pairs pushed (equal pairs are identical, so ties cannot be told apart). The layout of the
// arrays inside the heap is NOT specified and is not tested; only push, pop and peek results are.
//
// Check order for push, pop and peek: a null pointer argument gives NullArgument; capacity above kMaxCapacity gives TooLarge; *size above
// capacity gives BadSize; then the operation itself (Full for a push at *size == capacity, Empty for a pop or peek at *size == 0). On any
// failure nothing is written. For pop and peek the out pointers (out_key, out_value) are also checked for null and left untouched on failure.
// A capacity of 0 is allowed: every push gives Full, every pop gives Empty (the pointers must still be non-null).

// Adds (key, value). On Ok *size grows by one.
HeapResult heap_push(std::uint32_t* keys, std::uint32_t* values, std::uint32_t* size, std::uint32_t capacity, std::uint32_t key,
                     std::uint32_t value);

// Removes the smallest pair and stores it in *out_key and *out_value. On Ok *size shrinks by one.
HeapResult heap_pop_min(std::uint32_t* keys, std::uint32_t* values, std::uint32_t* size, std::uint32_t capacity, std::uint32_t* out_key,
                        std::uint32_t* out_value);

// Stores the smallest pair in *out_key and *out_value without removing it. Arrays and *size are not changed (pointers to const).
HeapResult heap_peek_min(const std::uint32_t* keys, const std::uint32_t* values, const std::uint32_t* size, std::uint32_t capacity,
                         std::uint32_t* out_key, std::uint32_t* out_value);

// Decisions: the algorithm and the internal layout are free as long as the contract above holds. A header ambiguity found later is a
// question to the maintainer, not an interpretation.

}  // namespace lumora::minheap
