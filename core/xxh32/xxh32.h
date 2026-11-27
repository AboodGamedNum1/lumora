// xxHash32 (32-bit non-cryptographic hash) for cache keys and content ids, one-shot and streaming. Pure integer: no floating point, no
// libm, no 128-bit types (MSVC), no allocation, no global state. Evidence label: unit tests only. Not for security.
#pragma once
#include <cstdint>

namespace lumora::xxh32 {

// Algorithm (all arithmetic modulo 2^32, input words read little endian by explicit byte assembly, never by pointer cast):
//   P1 = 2654435761, P2 = 2246822519, P3 = 3266489917, P4 = 668265263, P5 = 374761393; rotl(x, r) rotates left.
//   If the total length is at least 16: v1 = seed + P1 + P2, v2 = seed + P2, v3 = seed, v4 = seed - P1. Each 16-byte stripe is four
//   little-endian words w1..w4 and updates vi = rotl(vi + wi * P2, 13) * P1. After the last whole stripe h = rotl(v1,1) + rotl(v2,7) +
//   rotl(v3,12) + rotl(v4,18). Below 16 bytes h = seed + P5.
//   Then h += total length (as uint32). The remaining tail (fewer than 16 bytes): each whole 4-byte little-endian word w gives
//   h = rotl(h + w * P3, 17) * P4; then each remaining single byte b gives h = rotl(h + b * P5, 11) * P1.
//   Finally h ^= h >> 15; h *= P2; h ^= h >> 13; h *= P3; h ^= h >> 16.
// Known values: the hash of the empty input is 0x02CC5D05 for seed 0 and 0x36B78AE7 for seed 2654435761 (P1).

constexpr std::uint32_t kMaxStreamLength = 0xFFFFFFFFu;  // total bytes accepted by one stream

// One-shot hash of len bytes. Returns false and leaves *out unchanged when out is null, or data is null with len > 0 (len 0 with a null
// data pointer is the empty input and is fine). The same bytes in one call or split over any update calls give the same value.
bool hash(const std::uint8_t* data, std::uint32_t len, std::uint32_t seed, std::uint32_t* out);

struct State {
  std::uint32_t v[4] = {};         // lane accumulators, meaningful once total_len >= 16
  std::uint32_t total_len = 0;     // bytes consumed so far
  std::uint32_t seed = 0;
  std::uint8_t buf[16] = {};       // pending bytes, buf[0 .. buf_len) used
  std::uint32_t buf_len = 0;       // 0 .. 15
};

// A state is corrupt when buf_len is above 15. update and digest on a null or corrupt state return false and change nothing.

// Starts a stream: lanes initialised as above, total_len 0, buf_len 0, seed stored. A null state is ignored. Any previous content,
// corrupt or not, is replaced.
void reset(State* state, std::uint32_t seed);

// Adds len bytes. Returns false and changes nothing when state is null or corrupt, data is null with len > 0, or total_len + len would
// exceed kMaxStreamLength (computed in 64 bits). len 0 returns true and changes nothing. Whole 16-byte stripes are consumed as soon as
// they are complete, so buf_len stays at most 15 after every call and a stripe that is completed exactly at the end of an update is
// consumed in that call (buf_len 0, not 16).
bool update(State* state, const std::uint8_t* data, std::uint32_t len);

// The hash of everything added so far; does not change the state, so the stream can continue. Returns false and leaves *out unchanged
// when state or out is null or the state is corrupt. The large-input rule (four lanes) applies when total_len >= 16.
bool digest(const State* state, std::uint32_t* out);

// Decisions: aliasing of data with the state's buffer is not promised, not checked and not tested. A header ambiguity found later is a
// question to the maintainer, not an interpretation.

}  // namespace lumora::xxh32
