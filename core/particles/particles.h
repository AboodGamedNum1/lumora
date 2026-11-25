// CPU particle emitter simulation (milestone M4 groundwork). Deterministic, fixed-step, no GPU, no file system, no
// dependencies, no global state. The same config, seed and sequence of dt values give the same particles bit for bit on
// every host (to the documented libm tolerance, see docs/particles.md written by the contributor).
// Total functions: no exception, no out-of-bounds access, no NaN or infinity in any particle.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lumora::particles {

struct EmitterConfig {
  float position[2] = {0, 0};     // emitter centre, finite, |value| <= 1e6
  float emit_radius = 0;          // particles start at a random point inside this disc, 0 to 1e6
  float rate = 10;                // particles per second, 0 to 100000
  std::uint32_t max_particles = 1024;  // 1 to 65536 alive at once; emission stops while the pool is full
  float lifetime_min = 1, lifetime_max = 2;  // seconds, 0 < min <= max <= 3600
  float speed_min = 0, speed_max = 1;        // units per second, 0 <= min <= max <= 1e6
  float angle_degrees = 90;       // emission direction, finite, any value (wrapped)
  float spread_degrees = 360;     // full cone width, 0 to 360
  float gravity[2] = {0, 0};      // units per second squared, |value| <= 1e6
  float drag = 0;                 // 1/second velocity damping, 0 to 100
  float size_start = 1, size_end = 1;        // 0 to 1e6, linear over the lifetime
  float color_start[4] = {1, 1, 1, 1};       // straight RGBA, each 0 to 1, linear over the lifetime
  float color_end[4] = {1, 1, 1, 0};
  std::uint64_t seed = 1;         // any value, including 0
};

enum class ConfigStatus {
  Ok,
  BadPosition,
  BadRadius,
  BadRate,
  BadMaxParticles,
  BadLifetime,
  BadSpeed,
  BadAngle,
  BadSpread,
  BadGravity,
  BadDrag,
  BadSize,
  BadColor
};

struct Particle {
  float x = 0, y = 0;
  float vx = 0, vy = 0;
  float age = 0, lifetime = 1;
  float size = 1;
  float color[4] = {1, 1, 1, 1};
};

struct Emitter {
  EmitterConfig config;
  std::vector<Particle> particles;  // alive particles, in a documented deterministic order
  double emit_carry = 0;            // fractional particles not yet emitted
  std::uint64_t rng_state = 0;      // the contributor chooses the generator (PCG32 or splitmix64) and documents it
  std::uint64_t steps = 0;
};

ConfigStatus validate_config(const EmitterConfig& config);  // first failing field in the enum order above
const char* config_status_text(ConfigStatus status);        // short fixed text

// Starts an emitter with no particles. False (out untouched) unless validate_config is Ok.
bool emitter_init(const EmitterConfig& config, Emitter& out);

// Advances the simulation by dt seconds. dt must be finite and in (0, 1]; otherwise false and nothing changes.
// A long dt is split into fixed substeps (the contributor documents the substep size, at most 1/30 s) so the result
// does not depend on how the caller splits time except through the documented substep grid.
bool emitter_step(Emitter& e, float dt);

// Stable 64-bit digest of the whole state (config, particles, carry, generator, steps) for determinism tests.
std::uint64_t emitter_digest(const Emitter& e);

}  // namespace lumora::particles
