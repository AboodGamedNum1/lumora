// Generational handle allocator with 256 slots (stale-handle detection for scene resources). It stores no payload, only which slots are
// in use and their generation. Pure integer: no floating point, no libm, no 128-bit types (MSVC), no allocation, no global state, no
// locking. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::slotmap {

constexpr std::uint32_t kSlots = 256;
constexpr std::uint32_t kMaxGeneration = 0xFFFFFF;  // generations are 24 bits, valid values 1 .. kMaxGeneration

struct SlotMap {
  std::uint32_t gen[kSlots];  // current generation of each slot
  std::uint8_t used[kSlots];  // 0 free, anything else in use
  std::uint32_t count = 0;    // reported by size(); updated by allocate and release
};

// A handle is (generation << 8) | index, so index = handle & 255 and generation = handle >> 8. A handle whose generation is 0 is never
// live (so 0 is a safe "no handle" value).

// Marks every slot free with generation 1 and sets count to 0. A null map is ignored. Works on any previous content.
void init(SlotMap* map);

// Allocates the LOWEST free slot index (the first i in 0 .. 255 with used[i] == 0, judged by the used flags only) and returns true with
// *handle = (gen[i] << 8) | i, sets used[i] = 1 and increments count. Returns false and changes nothing when map or handle is null, when
// count is above kSlots (corrupt), or when no slot is free. The generation of a slot is not re-checked: a slot with generation 0 or above
// kMaxGeneration (only possible in a hand-built map) is handed out as stored, masked to 24 bits in the handle.
bool allocate(SlotMap* map, std::uint32_t* handle);

// Releases a live handle: returns true, sets used[i] = 0, replaces gen[i] by gen[i] + 1, or by 1 when gen[i] was kMaxGeneration, and
// decrements count when count is above 0. Returns false and changes nothing when map is null, count is above kSlots, the handle is not
// live (see is_live), including a second release of the same handle.
bool release(SlotMap* map, std::uint32_t handle);

// True when map is not null, count is not above kSlots, the handle's generation field is not 0, used[index] != 0 and gen[index] ==
// the handle's generation field.
bool is_live(const SlotMap* map, std::uint32_t handle);

// count, or 0 for a null map or a corrupt one (count above kSlots).
std::uint32_t size(const SlotMap* map);

// Decisions: count is informational for size() and for the corrupt check; allocation and liveness use the used flags, so a hand-built map
// whose count disagrees with its flags is handled slot by slot and release never underflows count. A header ambiguity found later is a
// question to the maintainer, not an interpretation.

}  // namespace lumora::slotmap
