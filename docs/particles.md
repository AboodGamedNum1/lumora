# Particle emitter

`core/particles/particles.h` and `core/particles/particles.cpp` implement a deterministic CPU particle emitter. There is
no GPU, no file system and no libm call in the simulation. The interface is in `particles.h`; this page records the
rules the implementation follows.

## Configuration and statuses

`validate_config` checks fields in the order of the `ConfigStatus` enum and returns the first failure. For each field
the finite check comes first, then the range. Ranges are inclusive.

| Status | Field | Rule |
| --- | --- | --- |
| `BadPosition` | `position` | both finite, absolute value at most 1e6 |
| `BadRadius` | `emit_radius` | finite, 0 to 1e6 |
| `BadRate` | `rate` | finite, 0 to 100000 particles per second |
| `BadMaxParticles` | `max_particles` | 1 to 65536 |
| `BadLifetime` | `lifetime_min`, `lifetime_max` | finite, `0 < min <= max <= 3600` |
| `BadSpeed` | `speed_min`, `speed_max` | finite, `0 <= min <= max <= 1e6` |
| `BadAngle` | `angle_degrees` | finite only; any value is allowed and wrapped |
| `BadSpread` | `spread_degrees` | finite, 0 to 360 |
| `BadGravity` | `gravity` | both finite, absolute value at most 1e6 |
| `BadDrag` | `drag` | finite, 0 to 100 |
| `BadSize` | `size_start`, `size_end` | finite, 0 to 1e6 |
| `BadColor` | `color_start`, `color_end` | every component finite, 0 to 1 |

Messages from `config_status_text` are short fixed texts. Subnormal lifetimes are accepted because the header range is
`0 < min`. A particle with such a lifetime dies on its first substep.

## Time and the substep grid

`emitter_step(e, dt)` returns false and changes nothing when `dt` is not finite or not in (0, 1], or when
`validate_config(e.config)` is not `Ok`. Otherwise the call runs `n = ceil(64 * dt)` equal substeps of `dt / n` seconds
(at most 64, so a substep is at most 1/64 s). `steps` counts substeps, not calls.

A `dt` that is a multiple `k/64` runs `k` substeps of exactly 1/64 s. Splitting time into such calls therefore gives
the same state and digest: 256 calls of 1/64, 128 of 2/64, 8 of 0.5 and 4 of 1.0 are identical, and the tests check
this. Other `dt` values are deterministic but a different split of the same total time can differ, because the grid
differs.

## One substep

In this order:

1. Every alive particle: `age += h`. If `!(age < lifetime)` it dies (death at `age >= lifetime`).
2. Semi-implicit Euler with drag: `v = (v + gravity * h) / (1 + drag * h)`, then `x += v * h`. There is no `exp`.
3. Size and colour are recomputed as a linear blend from start to end by `age / lifetime`, clamped to [0, 1]. Colours
   stay in [0, 1], size is finite and not negative.
4. Dead particles are removed by stable compaction.
5. Emission: `carry += rate * h`, `whole = floor(carry)`, `carry -= whole`, then `min(whole, free slots)` particles are
   spawned at age 0. The part that does not fit is dropped, so a full pool leaves no backlog and emission resumes as
   particles die. The carry makes the long-run average exact (tests: 100 per second for 600 s gives 60000).

## Particle order

`particles` is in creation order: oldest first. Compaction keeps the relative order, so the order is stable. The vector
is reserved once in `emitter_init` for `max_particles` and never grows beyond it.

## Generator and spawning

The generator is splitmix64 on `rng_state`, which starts at `seed` (seed 0 is valid). Each draw does
`state += 0x9E3779B97F4A7C15`, then the two multiply steps and `z ^ (z >> 31)`. A unit value is the top 24 bits of the
output times 2^-24, so it lies in [0, 1).

Each spawn consumes exactly five draws, in this order: `u_radius`, `u_disc`, `u_dir`, `u_speed`, `u_life`.

- Position: the centre plus `emit_radius * sqrt(u_radius)` in the direction of `u_disc` turns. This is uniform over the
  disc area.
- Direction in turns: `angle / 360 + (u_dir - 0.5) * spread / 360`. This is uniform inside the cone.
- Speed and lifetime: uniform between their min and max.

Sine and cosine come from an own routine (quadrant reduction and a Taylor series, error below 1e-14), using only
`sqrt`, `floor` and the four basic operations. Source pragmas ask compilers not to fuse multiply-add, so results do not
depend on the host.

## Digest

`emitter_digest` is FNV-1a 64 (offset 0xcbf29ce484222325, prime 0x100000001b3) over these bytes. All numbers are
little endian; floats are their IEEE 754 bit patterns.

1. 8 tag bytes `LUMPAR01`.
2. Config: `position[2]`, `emit_radius`, `rate` (f32); `max_particles` (u32); `lifetime_min`, `lifetime_max`,
   `speed_min`, `speed_max`, `angle_degrees`, `spread_degrees`, `gravity[2]`, `drag`, `size_start`, `size_end`,
   `color_start[4]`, `color_end[4]` (f32); `seed` (u64).
3. Particle count (u64).
4. Per particle: `x`, `y`, `vx`, `vy`, `age`, `lifetime`, `size`, `color[4]` (f32).
5. `emit_carry` (f64 bits), `rng_state` (u64), `steps` (u64).

Golden value: the emitter of `golden_config()` in `tests/particles/particles_tests.cpp` after 100 calls of
`emitter_step(e, 1.0f / 64)` has digest `0x49e1a918c46ffb08`. That test checks the same value.

## Usage

```cpp
lumora::particles::Emitter e;
if (!lumora::particles::emitter_init(config, e)) { /* validate_config tells which field */ }
while (running) {
  lumora::particles::emitter_step(e, dt);  // dt in (0, 1]
  for (const auto& p : e.particles) { /* draw p.x, p.y, p.size, p.color */ }
}
```

## Evidence limits

The tests are CPU only. Results measured on a local sanitized Linux build are local results. Only the CI jobs of the
pull request count as CI evidence, and no GPU, device or other-compiler claim is made here.
