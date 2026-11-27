// Set of disjoint half-open intervals [lo, hi) of 32-bit unsigned integers, stored in two caller arrays in one fully specified (canonical)
// form. Pure integer: no floating point, no libm, no 128-bit types or builtins (MSVC), no allocation, no recursion, no global state.
// Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::ivset {

constexpr std::uint32_t kMaxCapacity = 1u << 24;  // largest number of intervals the arrays may hold

enum class IvResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, BadCount = 3, InvalidRange = 4, Full = 5 };

// STATE: starts[0 .. *count) and ends[0 .. *count); interval i is [starts[i], ends[i]). Values range over 0 .. 0xFFFFFFFE (hi is exclusive, so
// 0xFFFFFFFF can be an end but never a member). CANONICAL FORM, specified and tested exactly: every interval is non-empty (starts[i] <
// ends[i]); intervals are sorted ascending; and consecutive intervals are separated by a gap: ends[i] < starts[i + 1] (touching intervals
// such as [1, 3) and [3, 5) are always merged into one, [1, 5)). An empty set has *count 0 (the caller sets it). The state is made only by
// these functions; an array in any other form is not valid and the behaviour on it is not specified and not tested. Entries at index *count
// and above are never read, and are never written except as stated below.
//
// Check order for every function: a null pointer argument (every array and every out pointer, *count included) gives NullArgument; capacity
// above kMaxCapacity gives TooLarge; *count above capacity gives BadCount; lo above hi gives InvalidRange (insert, remove); then the
// operation. On any failure nothing is written. The arrays must have capacity elements each and must not overlap each other or *count.

// Adds the integers lo .. hi-1 to the set. lo == hi adds nothing and is Ok. Intervals that overlap or touch [lo, hi) are merged with it. The
// resulting count may be one more than before (no neighbour touched), the same, or smaller. Full when the resulting count would exceed
// capacity (nothing is written then). Writes only at indexes below max(old count, new count).
IvResult ivset_insert(std::uint32_t* starts, std::uint32_t* ends, std::uint32_t* count, std::uint32_t capacity, std::uint32_t lo,
                      std::uint32_t hi);

// Removes the integers lo .. hi-1 from the set (a member inside the range is dropped, an interval is trimmed or split in two). lo == hi
// removes nothing and is Ok. Full when the resulting count would exceed capacity (a split of an interval at a full set), nothing written.
// Writes only at indexes below max(old count, new count).
IvResult ivset_remove(std::uint32_t* starts, std::uint32_t* ends, std::uint32_t* count, std::uint32_t capacity, std::uint32_t lo,
                      std::uint32_t hi);

// *out = true when x is a member (starts[i] <= x < ends[i] for some i). Reads only; count 0 gives false.
IvResult ivset_contains(const std::uint32_t* starts, const std::uint32_t* ends, const std::uint32_t* count, std::uint32_t capacity,
                        std::uint32_t x, bool* out);

// Decisions: the algorithm is free as long as the contract above holds. A header ambiguity found later is a question to the maintainer, not
// an interpretation.

}  // namespace lumora::ivset
