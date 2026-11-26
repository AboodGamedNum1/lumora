// Audio-reactive analysis core (docs/physics-audio.md, "Audio reactivity"): loudness, envelope following and an
// integer FFT power spectrum on 16-bit PCM blocks. Pure functions on caller-owned buffers: no I/O, no platform
// calls, no allocation beyond small local tables, no floating point and no libm in the analysis path, so every
// result is bit-exact on every compiler. The microphone capture and the capability gate live elsewhere
// (core/package/capabilities.h). Evidence label: unit tests only.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::audio {

// ---- Level -------------------------------------------------------------------------------------------------
struct Level {
  std::uint32_t peak = 0;          // largest absolute sample value; -32768 counts as 32768
  std::uint64_t sum_squares = 0;   // exact sum of sample*sample; each term is at most 2^30, so it is exact for up to 2^34-1 samples and wraps modulo 2^64 beyond that (no saturation, no error)
};
// n == 0 gives zeros and returns true (samples may be null then). Null out, or null samples with n > 0, returns
// false and writes nothing.
bool measure_level(const std::int16_t* samples, std::size_t n, Level* out);

// Floor of the square root of v, exact for every 64-bit value.
std::uint32_t isqrt64(std::uint64_t v);

// ---- Envelope follower -------------------------------------------------------------------------------------
// One step of a one-pole follower in integers. coefficients are Q16 (65536 means 1.0); a value above 65536 is
// clamped to 65536. If x > env the attack coefficient is used, otherwise the release coefficient (x == env
// returns env; a zero coefficient holds, 65536 reaches x exactly). The shift floors, so a falling delta of -5 with
// coefficient 1 steps by -1 while a tiny positive delta steps by 0 (intended asymmetry). Let delta = x - env as a signed 64-bit value and step = (delta * coef) >> 16 with an
// arithmetic (flooring) shift; the result is env + step. The result always lies between env and x inclusive.
std::uint32_t envelope_step(std::uint32_t env, std::uint32_t x, std::uint32_t attack_q16, std::uint32_t release_q16);

// ---- FFT power spectrum ------------------------------------------------------------------------------------
constexpr std::size_t kMinFftSize = 64;
constexpr std::size_t kMaxFftSize = 1024;

// Computes the power spectrum of n samples; n must be a power of two from kMinFftSize to kMaxFftSize, otherwise
// the function returns false and writes nothing. out_power receives n/2 bins (bin k is the frequency k/n of the
// sample rate; the Nyquist bin is not reported). Null pointers return false. Exact algorithm (all arithmetic in
// int32 with int64 intermediates; ">>" is an arithmetic flooring shift; round(x) is round half away from zero):
//  1. Window: w[i] = min(32767, round(32768 * (0.5 - 0.5 * cos(2*pi*i/n)))) (Hann, Q15); x[i] = (s[i] * w[i]) >> 15.
//  2. Radix-2 decimation in time on the bit-reversed input, imaginary parts zero. The twiddle for index k of
//     size n is wr = clamp(round(32768 * cos(2*pi*k/n)), -32767, 32767), wi = clamp(round(-32768 * sin(2*pi*k/n)),
//     -32767, 32767). Butterfly on (a, b) with twiddle w: t.re = (b.re*wr - b.im*wi + 16384) >> 15,
//     t.im = (b.re*wi + b.im*wr + 16384) >> 15; a' = (a + t) >> 1, b' = (a - t) >> 1 per component. Stages run
//     from span 1 up to n/2; every stage halves, so the output is scaled by 1/n.
//  3. out_power[k] = re[k]*re[k] + im[k]*im[k] computed in 64 bits and cast to uint32 without saturation (every
//     component stays within about 2^15 after the 1/n scaling, so the sum stays below 2^32; tests cover extremes).
// Tables: a 1024-entry Q15 cosine table c[j] = clamp(round(32768*cos(2*pi*j/1024)), -32767, 32767) and a 1024-entry
// Hann table h[j] = min(32767, round(32768*(0.5-0.5*cos(2*pi*j/1024)))) are embedded in analysis.cpp. Window for size
// n uses h[i*(1024/n)]. In a stage with block size m = 2*span, the twiddle for position j (0 <= j < span) has angle
// 2*pi*j/m, table index t = j*(1024/m): wr = c[t], wi = -c[(t+768) % 1024] (equal to c[(t+256) % 1024]).
// Bit-reverse the windowed samples first, then run spans 1, 2, ... n/2.
// The tests compare exact outputs against an independent Python reference that uses math.cos and math.sin.
bool fft_power(const std::int16_t* samples, std::size_t n, std::uint32_t* out_power);

// ---- Band energies -----------------------------------------------------------------------------------------
// Sums power bins into bands: band b covers bins [edges[b], edges[b+1]). edges has band_count + 1 entries and
// must be non-decreasing with edges[band_count] <= bins; band_count 0 is allowed (edges[0] must still be
// <= bins), and edges is required even for zero bands. Bins outside [edges[0], edges[band_count]) are ignored and
// edges[0] may exceed 0. out_energy is written only when band_count > 0 and may be null only when band_count is 0; power
// may be null only when no band is nonempty. An empty band gives 0. Sums are exact in 64 bits. On any violation, or a null pointer where data is
// needed, returns false and writes nothing.
bool band_energies(const std::uint32_t* power, std::size_t bins, const std::uint32_t* edges, std::size_t band_count,
                   std::uint64_t* out_energy);

}  // namespace lumora::audio
