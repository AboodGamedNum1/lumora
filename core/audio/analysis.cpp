// Stub: implementation lands by pull request (contributor scope). See analysis.h for the contract.
#include "analysis.h"

namespace lumora::audio {

bool measure_level(const std::int16_t*, std::size_t, Level*) { return false; }
std::uint32_t isqrt64(std::uint64_t) { return 0; }
std::uint32_t envelope_step(std::uint32_t env, std::uint32_t, std::uint32_t, std::uint32_t) { return env; }
bool fft_power(const std::int16_t*, std::size_t, std::uint32_t*) { return false; }
bool band_energies(const std::uint32_t*, std::size_t, const std::uint32_t*, std::size_t, std::uint64_t*) { return false; }

}  // namespace lumora::audio
