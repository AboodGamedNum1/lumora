// Tests for core/particles. Pure CPU. The golden digest below is also written in docs/particles.md; it must match on
// every CI host because the simulation uses no libm function.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "particles.h"

using Catch::Approx;
using namespace lumora::particles;

namespace {

constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr double kPi = 3.14159265358979323846;

bool particle_finite(const Particle& p) {
  const float all[] = {p.x, p.y, p.vx, p.vy, p.age, p.lifetime, p.size, p.color[0], p.color[1], p.color[2], p.color[3]};
  for (float v : all) {
    if (!std::isfinite(v)) return false;
  }
  if (p.size < 0.0f) return false;
  for (float c : p.color) {
    if (c < 0.0f || c > 1.0f) return false;
  }
  return p.age >= 0.0f && p.lifetime > 0.0f && p.age < p.lifetime;
}

bool all_finite(const Emitter& e) {
  if (!std::isfinite(e.emit_carry) || e.emit_carry < 0.0 || e.emit_carry >= 1.0) return false;
  for (const Particle& p : e.particles) {
    if (!particle_finite(p)) return false;
  }
  return true;
}

// Number of particles spawned so far. Every spawn consumes exactly five splitmix64 draws and each draw adds the
// golden-ratio increment to rng_state, so the count follows from the state (documented in docs/particles.md).
std::uint64_t spawned(const Emitter& e) {
  const std::uint64_t gamma = 0x9E3779B97F4A7C15ull;
  std::uint64_t inv = gamma;  // Newton iteration for the inverse of an odd number modulo 2^64
  for (int i = 0; i < 6; ++i) inv *= 2ull - gamma * inv;
  return (e.rng_state - e.config.seed) * inv / 5ull;
}

Emitter make(const EmitterConfig& c) {
  Emitter e;
  REQUIRE(emitter_init(c, e));
  return e;
}

// The configuration used by the golden digest test.
EmitterConfig golden_config() {
  EmitterConfig c;
  c.position[0] = 3.5f;
  c.position[1] = -2.25f;
  c.emit_radius = 4.0f;
  c.rate = 300.0f;
  c.max_particles = 500;
  c.lifetime_min = 0.5f;
  c.lifetime_max = 2.5f;
  c.speed_min = 1.0f;
  c.speed_max = 6.0f;
  c.angle_degrees = 100.0f;
  c.spread_degrees = 70.0f;
  c.gravity[0] = 0.5f;
  c.gravity[1] = -9.8f;
  c.drag = 0.4f;
  c.size_start = 2.0f;
  c.size_end = 0.25f;
  c.color_start[0] = 1.0f; c.color_start[1] = 0.5f; c.color_start[2] = 0.25f; c.color_start[3] = 1.0f;
  c.color_end[0] = 0.0f; c.color_end[1] = 0.125f; c.color_end[2] = 1.0f; c.color_end[3] = 0.0f;
  c.seed = 0x1234ABCDull;
  return c;
}

constexpr std::uint64_t kGoldenDigest = 0x49e1a918c46ffb08ull;

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("validate_config: every status in enum order", "[particles][config]") {
  EmitterConfig ok;
  CHECK(validate_config(ok) == ConfigStatus::Ok);
  CHECK(std::string(config_status_text(ConfigStatus::Ok)) == "ok");

  auto status = [](EmitterConfig c) { return validate_config(c); };
  auto with = [](auto f) { EmitterConfig c; f(c); return c; };

  for (float bad : {kNaN, kInf, -kInf, 1.0000001e6f, -1.0000001e6f}) {
    CHECK(status(with([&](EmitterConfig& c) { c.position[0] = bad; })) == ConfigStatus::BadPosition);
    CHECK(status(with([&](EmitterConfig& c) { c.position[1] = bad; })) == ConfigStatus::BadPosition);
  }
  CHECK(status(with([](EmitterConfig& c) { c.position[0] = 1e6f; c.position[1] = -1e6f; })) == ConfigStatus::Ok);

  for (float bad : {kNaN, kInf, -0.001f, 1.0000001e6f}) {
    CHECK(status(with([&](EmitterConfig& c) { c.emit_radius = bad; })) == ConfigStatus::BadRadius);
  }
  CHECK(status(with([](EmitterConfig& c) { c.emit_radius = 1e6f; })) == ConfigStatus::Ok);

  for (float bad : {kNaN, kInf, -1.0f, 100000.01f}) {
    CHECK(status(with([&](EmitterConfig& c) { c.rate = bad; })) == ConfigStatus::BadRate);
  }
  CHECK(status(with([](EmitterConfig& c) { c.rate = 100000.0f; })) == ConfigStatus::Ok);
  CHECK(status(with([](EmitterConfig& c) { c.rate = 0.0f; })) == ConfigStatus::Ok);

  CHECK(status(with([](EmitterConfig& c) { c.max_particles = 0; })) == ConfigStatus::BadMaxParticles);
  CHECK(status(with([](EmitterConfig& c) { c.max_particles = 65537; })) == ConfigStatus::BadMaxParticles);
  CHECK(status(with([](EmitterConfig& c) { c.max_particles = 0xFFFFFFFFu; })) == ConfigStatus::BadMaxParticles);
  CHECK(status(with([](EmitterConfig& c) { c.max_particles = 1; })) == ConfigStatus::Ok);
  CHECK(status(with([](EmitterConfig& c) { c.max_particles = 65536; })) == ConfigStatus::Ok);

  CHECK(status(with([](EmitterConfig& c) { c.lifetime_min = 0.0f; })) == ConfigStatus::BadLifetime);
  CHECK(status(with([](EmitterConfig& c) { c.lifetime_min = -1.0f; })) == ConfigStatus::BadLifetime);
  CHECK(status(with([](EmitterConfig& c) { c.lifetime_min = kNaN; })) == ConfigStatus::BadLifetime);
  CHECK(status(with([](EmitterConfig& c) { c.lifetime_max = kNaN; })) == ConfigStatus::BadLifetime);
  CHECK(status(with([](EmitterConfig& c) { c.lifetime_max = kInf; })) == ConfigStatus::BadLifetime);
  CHECK(status(with([](EmitterConfig& c) { c.lifetime_min = 3.0f; c.lifetime_max = 2.0f; })) == ConfigStatus::BadLifetime);
  CHECK(status(with([](EmitterConfig& c) { c.lifetime_max = 3600.01f; })) == ConfigStatus::BadLifetime);
  CHECK(status(with([](EmitterConfig& c) { c.lifetime_min = 3600.0f; c.lifetime_max = 3600.0f; })) == ConfigStatus::Ok);
  CHECK(status(with([](EmitterConfig& c) { c.lifetime_min = 1e-45f; c.lifetime_max = 1e-45f; })) == ConfigStatus::Ok);

  CHECK(status(with([](EmitterConfig& c) { c.speed_min = -1.0f; })) == ConfigStatus::BadSpeed);
  CHECK(status(with([](EmitterConfig& c) { c.speed_max = kNaN; })) == ConfigStatus::BadSpeed);
  CHECK(status(with([](EmitterConfig& c) { c.speed_min = 2.0f; c.speed_max = 1.0f; })) == ConfigStatus::BadSpeed);
  CHECK(status(with([](EmitterConfig& c) { c.speed_max = 1.0000001e6f; })) == ConfigStatus::BadSpeed);
  CHECK(status(with([](EmitterConfig& c) { c.speed_min = 1e6f; c.speed_max = 1e6f; })) == ConfigStatus::Ok);

  CHECK(status(with([](EmitterConfig& c) { c.angle_degrees = kNaN; })) == ConfigStatus::BadAngle);
  CHECK(status(with([](EmitterConfig& c) { c.angle_degrees = kInf; })) == ConfigStatus::BadAngle);
  CHECK(status(with([](EmitterConfig& c) { c.angle_degrees = 1e30f; })) == ConfigStatus::Ok);
  CHECK(status(with([](EmitterConfig& c) { c.angle_degrees = -12345.0f; })) == ConfigStatus::Ok);

  CHECK(status(with([](EmitterConfig& c) { c.spread_degrees = -0.1f; })) == ConfigStatus::BadSpread);
  CHECK(status(with([](EmitterConfig& c) { c.spread_degrees = 360.01f; })) == ConfigStatus::BadSpread);
  CHECK(status(with([](EmitterConfig& c) { c.spread_degrees = kNaN; })) == ConfigStatus::BadSpread);

  CHECK(status(with([](EmitterConfig& c) { c.gravity[0] = 1.0000001e6f; })) == ConfigStatus::BadGravity);
  CHECK(status(with([](EmitterConfig& c) { c.gravity[1] = -kInf; })) == ConfigStatus::BadGravity);
  CHECK(status(with([](EmitterConfig& c) { c.gravity[1] = kNaN; })) == ConfigStatus::BadGravity);

  CHECK(status(with([](EmitterConfig& c) { c.drag = -0.1f; })) == ConfigStatus::BadDrag);
  CHECK(status(with([](EmitterConfig& c) { c.drag = 100.01f; })) == ConfigStatus::BadDrag);
  CHECK(status(with([](EmitterConfig& c) { c.drag = kNaN; })) == ConfigStatus::BadDrag);
  CHECK(status(with([](EmitterConfig& c) { c.drag = 100.0f; })) == ConfigStatus::Ok);

  CHECK(status(with([](EmitterConfig& c) { c.size_start = -1.0f; })) == ConfigStatus::BadSize);
  CHECK(status(with([](EmitterConfig& c) { c.size_end = 1.0000001e6f; })) == ConfigStatus::BadSize);
  CHECK(status(with([](EmitterConfig& c) { c.size_end = kNaN; })) == ConfigStatus::BadSize);

  for (int i = 0; i < 4; ++i) {
    CHECK(status(with([&](EmitterConfig& c) { c.color_start[i] = 1.01f; })) == ConfigStatus::BadColor);
    CHECK(status(with([&](EmitterConfig& c) { c.color_start[i] = -0.01f; })) == ConfigStatus::BadColor);
    CHECK(status(with([&](EmitterConfig& c) { c.color_end[i] = kNaN; })) == ConfigStatus::BadColor);
    CHECK(status(with([&](EmitterConfig& c) { c.color_end[i] = kInf; })) == ConfigStatus::BadColor);
  }

  // The first failing field in enum order wins.
  EmitterConfig many;
  many.drag = -1;
  many.size_start = -1;
  many.color_end[2] = 2;
  CHECK(validate_config(many) == ConfigStatus::BadDrag);
  many.drag = 0;
  CHECK(validate_config(many) == ConfigStatus::BadSize);
  many.size_start = 1;
  CHECK(validate_config(many) == ConfigStatus::BadColor);
  many.position[0] = kNaN;
  many.rate = -1;
  CHECK(validate_config(many) == ConfigStatus::BadPosition);
  many.position[0] = 0;
  CHECK(validate_config(many) == ConfigStatus::BadRate);

  // Texts are fixed, short and distinct.
  std::vector<std::string> texts;
  for (int s = 0; s <= static_cast<int>(ConfigStatus::BadColor); ++s) {
    const std::string t = config_status_text(static_cast<ConfigStatus>(s));
    CHECK_FALSE(t.empty());
    for (const std::string& o : texts) CHECK(o != t);
    texts.push_back(t);
  }
  CHECK(std::string(config_status_text(static_cast<ConfigStatus>(99))) == "unknown status");
}

TEST_CASE("emitter_init and invalid inputs", "[particles][init]") {
  Emitter e;
  EmitterConfig bad;
  bad.rate = -1;
  e.steps = 77;
  CHECK_FALSE(emitter_init(bad, e));
  CHECK(e.steps == 77);  // out untouched
  EmitterConfig cfg;
  cfg.max_particles = 4096;
  REQUIRE(emitter_init(cfg, e));
  CHECK(e.particles.empty());
  CHECK(e.particles.capacity() >= 4096);
  CHECK(e.emit_carry == 0.0);
  CHECK(e.steps == 0);
  CHECK(e.rng_state == cfg.seed);

  const std::uint64_t d = emitter_digest(e);
  for (float dt : {0.0f, -0.1f, -0.0f, kNaN, kInf, -kInf, 1.0001f, 2.0f, 1e30f}) {
    CHECK_FALSE(emitter_step(e, dt));
    CHECK(emitter_digest(e) == d);  // nothing changed
  }
  CHECK(emitter_step(e, 1.0f));
  CHECK(emitter_step(e, 1e-30f));
  CHECK(emitter_step(e, std::numeric_limits<float>::denorm_min()));
  // An emitter whose config was edited into an invalid one is not stepped.
  e.config.rate = -5;
  const std::uint64_t d2 = emitter_digest(e);
  CHECK_FALSE(emitter_step(e, 0.1f));
  CHECK(emitter_digest(e) == d2);
}

TEST_CASE("seed zero is valid and seeds differ", "[particles][rng]") {
  EmitterConfig c;
  c.seed = 0;
  c.rate = 640;
  Emitter z = make(c);
  for (int i = 0; i < 10; ++i) REQUIRE(emitter_step(z, 1.0f / 64));
  CHECK(z.particles.size() > 0);
  CHECK(all_finite(z));
  Emitter z2 = make(c);
  for (int i = 0; i < 10; ++i) REQUIRE(emitter_step(z2, 1.0f / 64));
  CHECK(emitter_digest(z) == emitter_digest(z2));
  c.seed = 1;
  Emitter o = make(c);
  for (int i = 0; i < 10; ++i) REQUIRE(emitter_step(o, 1.0f / 64));
  CHECK(emitter_digest(o) != emitter_digest(z));
  CHECK(z.particles[0].x != o.particles[0].x);
  // Max seed.
  c.seed = ~0ull;
  Emitter m = make(c);
  for (int i = 0; i < 10; ++i) REQUIRE(emitter_step(m, 1.0f / 64));
  CHECK(all_finite(m));
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("substep grid: splitting time in multiples of 1/64 s gives identical states", "[particles][time]") {
  EmitterConfig c = golden_config();
  Emitter a = make(c), b = make(c), d = make(c), f = make(c);
  for (int i = 0; i < 256; ++i) REQUIRE(emitter_step(a, 1.0f / 64));  // four seconds in 1/64 steps
  for (int i = 0; i < 128; ++i) REQUIRE(emitter_step(b, 2.0f / 64));
  for (int i = 0; i < 8; ++i) REQUIRE(emitter_step(d, 0.5f));          // 32 substeps each
  for (int i = 0; i < 4; ++i) REQUIRE(emitter_step(f, 1.0f));
  CHECK(a.steps == 256);  // steps counts substeps
  CHECK(emitter_digest(a) == emitter_digest(b));
  CHECK(emitter_digest(a) == emitter_digest(d));
  CHECK(emitter_digest(a) == emitter_digest(f));
  // Mixed multiples of 1/64.
  Emitter m = make(c);
  const int pattern[] = {1, 3, 64, 7, 2, 5, 11, 63, 40, 60};  // sums to 256
  int sum = 0;
  for (int k : pattern) {
    REQUIRE(emitter_step(m, static_cast<float>(k) / 64.0f));
    sum += k;
  }
  CHECK(sum == 256);
  CHECK(emitter_digest(m) == emitter_digest(a));
}

TEST_CASE("substep grid: other dt values are deterministic and use ceil(64 dt) equal substeps", "[particles][time]") {
  EmitterConfig c = golden_config();
  Emitter a = make(c), b = make(c);
  for (int i = 0; i < 40; ++i) {
    REQUIRE(emitter_step(a, 0.1f));
    REQUIRE(emitter_step(b, 0.1f));
  }
  CHECK(emitter_digest(a) == emitter_digest(b));
  CHECK(a.steps == 40 * 7);  // ceil(6.4) = 7 substeps per call
  Emitter t = make(c);
  REQUIRE(emitter_step(t, 1.0f / 1000.0f));
  CHECK(t.steps == 1);
  REQUIRE(emitter_step(t, 1.0f / 64.0f + 1e-5f));
  CHECK(t.steps == 3);  // just above one grid step runs two substeps
  REQUIRE(emitter_step(t, 1.0f));
  CHECK(t.steps == 67);
  CHECK(all_finite(t));
}

TEST_CASE("fractional emission carry: exact long-run average over 10 simulated minutes", "[particles][carry]") {
  // Short lifetimes keep the pool small; the number of spawned particles is read from the generator state.
  EmitterConfig c;
  c.rate = 100.0f;
  c.max_particles = 1000;
  c.lifetime_min = 0.5f;
  c.lifetime_max = 0.5f;
  c.speed_max = 0.0f;
  Emitter e = make(c);
  for (int i = 0; i < 600; ++i) REQUIRE(emitter_step(e, 1.0f));
  CHECK(spawned(e) == 60000);

  // A rate that is not a multiple of the substep: 7.3 per second for 600 s.
  c.rate = 7.3f;
  Emitter f = make(c);
  for (int i = 0; i < 600; ++i) REQUIRE(emitter_step(f, 1.0f));
  const double expect = static_cast<double>(7.3f) * 600.0;
  CHECK(std::fabs(static_cast<double>(spawned(f)) - expect) <= 1.0);
  CHECK(f.emit_carry >= 0.0);
  CHECK(f.emit_carry < 1.0);

  // Another split (0.375 s calls, 24 substeps of exactly 1/64) gives the same total.
  Emitter g = make(c);
  for (int i = 0; i < 1600; ++i) REQUIRE(emitter_step(g, 0.375f));
  CHECK(spawned(g) == spawned(f));
  // Rate below one per second still emits: 0.25/s for 10 minutes is 150.
  c.rate = 0.25f;
  Emitter h = make(c);
  for (int i = 0; i < 600; ++i) REQUIRE(emitter_step(h, 1.0f));
  CHECK(spawned(h) == 150);
  // Rate zero emits nothing.
  c.rate = 0.0f;
  Emitter z = make(c);
  for (int i = 0; i < 100; ++i) REQUIRE(emitter_step(z, 1.0f));
  CHECK(spawned(z) == 0);
  CHECK(z.particles.empty());
}

TEST_CASE("pool cap: emission stops while full and resumes as particles die", "[particles][pool]") {
  EmitterConfig c;
  c.rate = 100000.0f;
  c.max_particles = 1000;
  c.lifetime_min = 0.5f;
  c.lifetime_max = 0.5f;
  Emitter e = make(c);
  const std::size_t cap0 = e.particles.capacity();
  REQUIRE(emitter_step(e, 1.0f / 64));
  CHECK(e.particles.size() == 1000);  // filled at once
  CHECK(e.emit_carry < 1.0);          // no backlog from the dropped emission
  for (int i = 0; i < 200; ++i) {
    REQUIRE(emitter_step(e, 1.0f / 64));
    CHECK(e.particles.size() <= 1000);
  }
  // All 1000 die together after 0.5 s (32 substeps) and are replaced in the same substep.
  CHECK(e.particles.size() == 1000);
  CHECK(e.particles.capacity() == cap0);
  // Stop the source: the pool empties and stays empty.
  e.config.rate = 0.0f;
  for (int i = 0; i < 40; ++i) REQUIRE(emitter_step(e, 1.0f / 64));
  CHECK(e.particles.empty());
  // Restart at a modest rate: no burst from earlier dropped emission.
  e.config.rate = 64.0f;
  REQUIRE(emitter_step(e, 1.0f / 64));
  CHECK(e.particles.size() == 1);
  // Gradual death with spread lifetimes: the pool refills as particles die and never exceeds the cap.
  c.lifetime_min = 0.1f;
  c.lifetime_max = 2.0f;
  c.rate = 5000.0f;
  Emitter g = make(c);
  for (int i = 0; i < 640; ++i) {
    REQUIRE(emitter_step(g, 1.0f / 64));
    REQUIRE(g.particles.size() <= 1000);
  }
  CHECK(g.particles.size() > 500);
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("physics: semi-implicit Euler, drag and death order", "[particles][physics]") {
  EmitterConfig c;
  c.rate = 64.0f;                       // exactly one particle per substep
  c.max_particles = 100;
  c.lifetime_min = c.lifetime_max = 1.0f;
  c.speed_min = c.speed_max = 0.0f;
  c.gravity[0] = 0.0f;
  c.gravity[1] = -10.0f;
  c.drag = 0.0f;
  c.size_start = 4.0f; c.size_end = 0.0f;
  Emitter e = make(c);
  REQUIRE(emitter_step(e, 1.0f / 64));
  REQUIRE(e.particles.size() == 1);
  CHECK(e.particles[0].age == 0.0f);
  CHECK(e.particles[0].size == 4.0f);
  CHECK(e.particles[0].x == 0.0f);
  REQUIRE(emitter_step(e, 1.0f / 64));
  // One substep: v = 0 + g h = -10/64 = -0.15625 (exact), then y = 0 + v h = -0.00244140625 (exact).
  CHECK(e.particles[0].vy == -0.15625f);
  CHECK(e.particles[0].y == -0.00244140625f);
  CHECK(e.particles[0].age == 0.015625f);
  CHECK(e.particles[0].size == Approx(4.0 * (1.0 - 0.015625)).epsilon(1e-6));
  // Particles stay in creation order: ages strictly decrease along the vector.
  for (int i = 0; i < 20; ++i) REQUIRE(emitter_step(e, 1.0f / 64));
  for (std::size_t i = 1; i < e.particles.size(); ++i) CHECK(e.particles[i - 1].age > e.particles[i].age);
  // Death at age >= lifetime: after exactly 64 substeps the first particle (age 64/64 = 1.0) is gone.
  Emitter d = make(c);
  for (int i = 0; i < 64; ++i) REQUIRE(emitter_step(d, 1.0f / 64));
  CHECK(d.particles.size() == 64);  // the first one is born in substep 1 and reaches age 63/64 in substep 64
  REQUIRE(emitter_step(d, 1.0f / 64));
  CHECK(d.particles.size() == 64);  // the oldest reaches age 1.0 and dies, one new one is born
  CHECK(d.particles.front().age == Approx(63.0 / 64.0).epsilon(1e-6));
  CHECK(d.particles.back().age == 0.0f);

  // Drag: v = (v + g h) / (1 + drag h). With speed only in x and no gravity: vx halves-ish each unit of time.
  EmitterConfig w;
  w.rate = 64.0f; w.max_particles = 4; w.lifetime_min = w.lifetime_max = 100.0f;
  w.speed_min = w.speed_max = 10.0f; w.angle_degrees = 0.0f; w.spread_degrees = 0.0f; w.drag = 64.0f;
  Emitter m = make(w);
  REQUIRE(emitter_step(m, 1.0f / 64));
  CHECK(m.particles[0].vx == Approx(10.0).epsilon(1e-6));
  CHECK(m.particles[0].vy == Approx(0.0).margin(1e-6));
  REQUIRE(emitter_step(m, 1.0f / 64));
  CHECK(m.particles[0].vx == Approx(5.0).epsilon(1e-6));  // 1 + 64 / 64 = 2
  CHECK(m.particles[0].x == Approx(5.0 / 64.0).epsilon(1e-6));
}

TEST_CASE("size and colour interpolate linearly with age / lifetime", "[particles][attributes]") {
  EmitterConfig c;
  c.rate = 64.0f; c.max_particles = 2; c.lifetime_min = c.lifetime_max = 2.0f;
  c.speed_max = 0.0f;
  c.size_start = 10.0f; c.size_end = 2.0f;
  c.color_start[0] = 1.0f; c.color_start[1] = 0.0f; c.color_start[2] = 0.5f; c.color_start[3] = 1.0f;
  c.color_end[0] = 0.0f; c.color_end[1] = 1.0f; c.color_end[2] = 0.5f; c.color_end[3] = 0.0f;
  Emitter e = make(c);
  REQUIRE(emitter_step(e, 1.0f / 64));
  CHECK(e.particles[0].size == 10.0f);
  CHECK(e.particles[0].color[0] == 1.0f);
  // After 64 substeps the first particle has age 1 = half its lifetime.
  for (int i = 0; i < 64; ++i) REQUIRE(emitter_step(e, 1.0f / 64));
  const Particle& p = e.particles.front();
  CHECK(p.age == Approx(1.0).epsilon(1e-6));
  CHECK(p.size == Approx(6.0).epsilon(1e-6));
  CHECK(p.color[0] == Approx(0.5).epsilon(1e-6));
  CHECK(p.color[1] == Approx(0.5).epsilon(1e-6));
  CHECK(p.color[2] == 0.5f);
  CHECK(p.color[3] == Approx(0.5).epsilon(1e-6));
  // Size may grow, too, and a zero size is allowed.
  EmitterConfig g = c;
  g.size_start = 0.0f; g.size_end = 8.0f;
  Emitter h = make(g);
  for (int i = 0; i < 65; ++i) REQUIRE(emitter_step(h, 1.0f / 64));
  CHECK(h.particles.front().size == Approx(4.0).epsilon(1e-6));
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("statistics: initial positions are uniform over the disc area", "[particles][stats]") {
  EmitterConfig c;
  c.position[0] = 100.0f; c.position[1] = -50.0f;
  c.emit_radius = 10.0f;
  c.rate = 6400.0f;
  c.max_particles = 65536;
  c.lifetime_min = c.lifetime_max = 3600.0f;
  c.speed_max = 0.0f;
  c.seed = 99;
  Emitter e = make(c);
  for (int i = 0; i < 640; ++i) REQUIRE(emitter_step(e, 1.0f / 64));  // 10 s, 64000 particles
  const std::size_t n = e.particles.size();
  REQUIRE(n > 60000);
  double mx = 0, my = 0;
  int rings[4] = {0, 0, 0, 0};
  int quadrants[4] = {0, 0, 0, 0};
  for (const Particle& p : e.particles) {
    const double dx = p.x - 100.0, dy = p.y + 50.0;
    mx += dx;
    my += dy;
    const double r = std::sqrt(dx * dx + dy * dy);
    REQUIRE(r <= 10.0 + 1e-4);
    // Equal-area rings: radii 10 * sqrt(k / 4).
    int ring = static_cast<int>(std::floor(4.0 * r * r / 100.0));
    if (ring > 3) ring = 3;
    ++rings[ring];
    ++quadrants[(dx >= 0 ? 1 : 0) + (dy >= 0 ? 2 : 0)];
  }
  CHECK(std::fabs(mx / static_cast<double>(n)) < 0.1);  // mean within 1% of the radius
  CHECK(std::fabs(my / static_cast<double>(n)) < 0.1);
  for (int k = 0; k < 4; ++k) {
    CHECK(std::fabs(rings[k] - static_cast<double>(n) / 4.0) < 0.03 * static_cast<double>(n));
    CHECK(std::fabs(quadrants[k] - static_cast<double>(n) / 4.0) < 0.03 * static_cast<double>(n));
  }
  // Radius zero: every particle starts exactly at the centre.
  c.emit_radius = 0.0f;
  Emitter z = make(c);
  for (int i = 0; i < 4; ++i) REQUIRE(emitter_step(z, 1.0f / 64));
  for (const Particle& p : z.particles) {
    CHECK(p.x == 100.0f);
    CHECK(p.y == -50.0f);
  }
}

TEST_CASE("statistics: directions are uniform within the cone, speed and lifetime are uniform", "[particles][stats]") {
  EmitterConfig c;
  c.rate = 6400.0f;
  c.max_particles = 65536;
  c.lifetime_min = 100.0f; c.lifetime_max = 300.0f;
  c.speed_min = 2.0f; c.speed_max = 6.0f;
  c.angle_degrees = 30.0f + 360.0f * 5.0f;  // wrapped
  c.spread_degrees = 90.0f;
  c.seed = 5;
  Emitter e = make(c);
  for (int i = 0; i < 640; ++i) REQUIRE(emitter_step(e, 1.0f / 64));
  const double n = static_cast<double>(e.particles.size());
  REQUIRE(n > 60000);
  int bins[8] = {0};
  double dev_min = 1e9, dev_max = -1e9, speed_sum = 0, life_sum = 0, smin = 1e9, smax = 0, lmin = 1e9, lmax = 0;
  for (const Particle& p : e.particles) {
    const double speed = std::sqrt(static_cast<double>(p.vx) * p.vx + static_cast<double>(p.vy) * p.vy);
    // Velocity only changes by gravity and drag, both zero here.
    double dev = std::atan2(p.vy, p.vx) * 180.0 / kPi - 30.0;
    while (dev > 180.0) dev -= 360.0;
    while (dev < -180.0) dev += 360.0;
    dev_min = std::min(dev_min, dev);
    dev_max = std::max(dev_max, dev);
    int bin = static_cast<int>(std::floor((dev + 45.0) / 90.0 * 8.0));
    if (bin < 0) bin = 0;
    if (bin > 7) bin = 7;
    ++bins[bin];
    speed_sum += speed;
    life_sum += p.lifetime;
    smin = std::min(smin, speed); smax = std::max(smax, speed);
    lmin = std::min(lmin, static_cast<double>(p.lifetime)); lmax = std::max(lmax, static_cast<double>(p.lifetime));
  }
  CHECK(dev_min >= -45.0 - 1e-3);
  CHECK(dev_max <= 45.0 + 1e-3);
  CHECK(dev_min < -44.0);  // the cone is covered to its edges
  CHECK(dev_max > 44.0);
  for (int b : bins) CHECK(std::fabs(b - n / 8.0) < 0.05 * n / 8.0 * 2.0);
  CHECK(speed_sum / n == Approx(4.0).epsilon(0.01));
  CHECK(life_sum / n == Approx(200.0).epsilon(0.01));
  CHECK(smin >= 2.0 - 1e-5);
  CHECK(smax <= 6.0 + 1e-5);
  CHECK(smin < 2.01);
  CHECK(smax > 5.99);
  CHECK(lmin >= 100.0);
  CHECK(lmax <= 300.0);
  CHECK(lmin < 100.1);
  CHECK(lmax > 299.9);

  // Spread 0: every direction is the emission angle. Spread 360: all four quadrants are used about equally.
  c.spread_degrees = 0.0f;
  Emitter z = make(c);
  for (int i = 0; i < 10; ++i) REQUIRE(emitter_step(z, 1.0f / 64));
  for (const Particle& p : z.particles) {
    const double dev = std::atan2(p.vy, p.vx) * 180.0 / kPi - 30.0;
    CHECK(std::fabs(dev) < 1e-3);
  }
  c.spread_degrees = 360.0f;
  Emitter f = make(c);
  for (int i = 0; i < 640; ++i) REQUIRE(emitter_step(f, 1.0f / 64));
  int quads[4] = {0, 0, 0, 0};
  for (const Particle& p : f.particles) ++quads[(p.vx >= 0 ? 1 : 0) + (p.vy >= 0 ? 2 : 0)];
  for (int q : quads) CHECK(std::fabs(q - static_cast<double>(f.particles.size()) / 4.0) < 0.03 * static_cast<double>(f.particles.size()));
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("hostile values stay finite after every step", "[particles][hostile]") {
  EmitterConfig c;
  c.position[0] = 1e6f; c.position[1] = -1e6f;
  c.emit_radius = 1e6f;
  c.rate = 100000.0f;
  c.max_particles = 2000;
  c.lifetime_min = 3000.0f; c.lifetime_max = 3600.0f;
  c.speed_min = 1e6f; c.speed_max = 1e6f;
  c.angle_degrees = 1e30f;
  c.spread_degrees = 360.0f;
  c.gravity[0] = 1e6f; c.gravity[1] = -1e6f;
  c.drag = 0.0f;
  c.size_start = 1e6f; c.size_end = 1e6f;
  Emitter e = make(c);
  for (int i = 0; i < 400; ++i) {
    REQUIRE(emitter_step(e, 1.0f));
    REQUIRE(all_finite(e));
  }
  CHECK(e.particles.size() == 2000);

  // Maximum drag and small and large dt.
  c.drag = 100.0f;
  c.gravity[0] = -1e6f;
  Emitter d = make(c);
  for (float dt : {1.0f, 1.0f / 64, 1e-6f, 0.999f, 0.3f, 1e-20f}) {
    for (int i = 0; i < 30; ++i) {
      REQUIRE(emitter_step(d, dt));
      REQUIRE(all_finite(d));
    }
  }
  // Subnormal lifetimes are accepted; such a particle dies on its first substep after birth.
  EmitterConfig s;
  s.lifetime_min = s.lifetime_max = std::numeric_limits<float>::denorm_min();
  s.rate = 6400.0f;
  s.max_particles = 500;
  Emitter u = make(s);
  for (int i = 0; i < 50; ++i) {
    REQUIRE(emitter_step(u, 1.0f / 64));
    REQUIRE(all_finite(u));
    CHECK(u.particles.size() == 100);  // only the particles born in the latest substep are alive
  }
  // The smallest and largest lifetime ranges.
  s.lifetime_min = std::numeric_limits<float>::denorm_min();
  s.lifetime_max = 3600.0f;
  Emitter v = make(s);
  for (int i = 0; i < 200; ++i) {
    REQUIRE(emitter_step(v, 1.0f / 64));
    REQUIRE(all_finite(v));
  }
  // Hand-edited state: a broken particle is dropped or repaired, never propagated.
  Emitter w = make(EmitterConfig{});
  Particle bad;
  bad.x = kNaN; bad.vx = kInf; bad.age = kNaN; bad.lifetime = 1.0f;
  w.particles.push_back(bad);
  Particle odd;
  odd.x = 1e38f; odd.vx = 3e38f; odd.age = 0.0f; odd.lifetime = 5.0f;
  w.particles.push_back(odd);
  REQUIRE(emitter_step(w, 1.0f / 64));
  for (const Particle& p : w.particles) CHECK(std::isfinite(p.x));
  CHECK(w.particles.size() <= 2);
}

TEST_CASE("65536 particles for 1000 steps: no growth, no non-finite value, time cap", "[particles][perf]") {
  EmitterConfig c;
  c.rate = 100000.0f;
  c.max_particles = 65536;
  c.lifetime_min = 20.0f; c.lifetime_max = 30.0f;
  c.speed_min = 1.0f; c.speed_max = 50.0f;
  c.emit_radius = 5.0f;
  c.gravity[1] = -9.8f;
  c.drag = 0.5f;
  c.seed = 2024;
  Emitter e = make(c);
  const std::size_t cap = e.particles.capacity();
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 1000; ++i) {
    REQUIRE(emitter_step(e, 1.0f / 64));
    REQUIRE(e.particles.size() <= 65536);
    if (i >= 60) REQUIRE(e.particles.size() == 65536);
    REQUIRE(all_finite(e));
  }
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  INFO("1000 steps took " << secs << " s");
  CHECK(e.particles.capacity() == cap);
  CHECK(e.steps == 1000);
  CHECK(secs < 30.0);
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("digest: stable, sensitive and matches the golden value after 100 steps", "[particles][digest]") {
  Emitter e = make(golden_config());
  const std::uint64_t before = emitter_digest(e);
  CHECK(before == emitter_digest(e));
  for (int i = 0; i < 100; ++i) REQUIRE(emitter_step(e, 1.0f / 64));
  const std::uint64_t after = emitter_digest(e);
  CHECK(after != before);
  CHECK(e.particles.size() > 100);
  INFO("digest after 100 steps: 0x" << std::hex << after);
  CHECK(after == kGoldenDigest);

  // The digest reacts to every part of the state.
  Emitter m = e;
  m.steps += 1;
  CHECK(emitter_digest(m) != after);
  m = e; m.rng_state ^= 1; CHECK(emitter_digest(m) != after);
  m = e; m.emit_carry += 0.125; CHECK(emitter_digest(m) != after);
  m = e; m.config.seed ^= 1; CHECK(emitter_digest(m) != after);
  m = e; m.config.drag += 1.0f; CHECK(emitter_digest(m) != after);
  m = e; m.particles[3].color[2] = std::nextafterf(m.particles[3].color[2], 2.0f); CHECK(emitter_digest(m) != after);
  m = e; m.particles.pop_back(); CHECK(emitter_digest(m) != after);
  m = e; std::swap(m.particles[0], m.particles[1]); CHECK(emitter_digest(m) != after);
  // -0.0 and +0.0 have different bit patterns.
  Emitter a = make(EmitterConfig{}), b = make(EmitterConfig{});
  a.config.position[0] = 0.0f;
  b.config.position[0] = -0.0f;
  CHECK(emitter_digest(a) != emitter_digest(b));
  // A copy has the same digest, and an independent run reproduces it.
  Emitter copy = e;
  CHECK(emitter_digest(copy) == after);
  Emitter again = make(golden_config());
  for (int i = 0; i < 100; ++i) REQUIRE(emitter_step(again, 1.0f / 64));
  CHECK(emitter_digest(again) == after);
}
