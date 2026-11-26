// PCG32 pseudo random generator (PCG-XSH-RR 64/32, O'Neill) with an unbiased bounded draw and a jump-ahead, for deterministic effects that
// must give the same numbers on Linux, Windows and Android. Pure integer: no floating point, no libm, no 128-bit types (MSVC), no
// allocation, no global state, no clock. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::prng {

constexpr std::uint32_t kMaxDraws = 64;  // most outputs one pcg32_bounded call consumes

struct Pcg32 {
  std::uint64_t state = 0;
  std::uint64_t inc = 1;  // stream selector, always odd after pcg32_seed
};

// One step of the generator from the current state, in this exact form (all arithmetic modulo 2^64 or 2^32):
//   old = state; state = old * 6364136223846793005 + inc;
//   xorshifted = (uint32)(((old >> 18) ^ old) >> 27); rot = (uint32)(old >> 59);
//   output = (xorshifted >> rot) | (xorshifted << ((0 - rot) & 31)).
// Seeding: state = 0; inc = (initseq << 1) | 1; one step (output discarded); state += initstate; one step (output discarded).
// Reference vector: seed(42, 54) gives the first six outputs 0xa15c02b7 0x7b47f409 0xba1d3330 0x83d2f293 0xbfa4784b 0xcbed606e.

// Seeds the generator as above. Returns false and changes nothing when rng is null.
bool pcg32_seed(Pcg32* rng, std::uint64_t initstate, std::uint64_t initseq);

// Returns the next 32-bit output and advances one step. A null rng returns 0. A hand-built even inc is used as stored (no repair).
std::uint32_t pcg32_next(Pcg32* rng);

// Value in [0, bound) by capped rejection sampling with a biased fallback: threshold = (0 - bound) % bound in uint32 arithmetic; draw
// outputs with pcg32_next until one is >= threshold and return that output % bound (unbiased when it happens, which is the case for the
// first draw whenever threshold is 0, for example bounds 1, 2, 4, 8). At most kMaxDraws outputs are drawn; if all are below the threshold
// the 64th output % bound is returned (this fallback can be biased). bound 0 returns 0 and does not advance; bound 1 returns 0 after
// consuming one output. A null rng returns 0.
std::uint32_t pcg32_bounded(Pcg32* rng, std::uint32_t bound);

// Jumps ahead: afterwards the state equals the state after delta calls of pcg32_next (outputs discarded), computed in O(64) steps, not
// delta steps; delta may be any uint64 (including 0, which changes nothing), and the arithmetic wraps modulo 2^64 so a delta of 2^64 - 1
// steps the generator back by one. Returns false and changes nothing when rng is null.
bool pcg32_advance(Pcg32* rng, std::uint64_t delta);

// Decisions: the output is computed from the old state, before stepping. pcg32_seed overwrites both fields; the top bit of initseq is lost
// on purpose in (initseq << 1). pcg32_bounded draws at most kMaxDraws = 64 outputs: if all 64 are below the threshold it stops and returns the 64th
// output % bound, having consumed exactly 64 outputs. No probability bound is claimed for reaching the cap with a seeded stream (the
// outputs of a deterministic generator are not proven to be independent draws); it is not expected in practice. A forged state such as
// state 0 with inc 0 (output 0 forever) reaches the cap for every bound with a positive threshold (for example 3, 5 or 0x80000001) and
// never for a bound with threshold 0 (1, 2, 4, 8, ...), which accepts the first output. This keeps the function terminating for every state. Tests must check the exact state after every bounded call, including the
// rejection path. pcg32_advance with delta 2^64 - 1 steps a seeded stream (odd inc) back by one exactly; for a forged even inc it is
// still defined as delta applications of the step (affine composition modulo 2^64), nothing more. inc is never changed by advance. No
// aliasing and no thread-safety promise is made or tested.

}  // namespace lumora::prng
