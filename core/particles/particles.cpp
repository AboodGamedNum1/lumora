// CPU particle emitter. Rules, generator, substep grid, particle order and digest layout: docs/particles.md.
// Determinism: the generator is splitmix64, sin and cos are an own polynomial (only +, -, *, / and floor), and square
// root is an exact IEEE operation, so no libm function is called and results are bit-identical on every host that
// does IEEE double arithmetic without fused multiply-add. Compilers are asked not to fuse; the project must not enable
// fast-math.
#include "particles.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#if defined(_MSC_VER) && !defined(__clang__)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

namespace lumora::particles {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kHalfPi = kPi / 2.0;
constexpr double kSubstepsPerSecond = 64.0;  // substep length 1/64 s (at most 1/30 s)
constexpr double kFloatMax = static_cast<double>(std::numeric_limits<float>::max());

// ---------------------------------------------------------------------------------------------------------------
// Generator: splitmix64. The state is rng_state, the first output of a fresh emitter comes from state = seed.
std::uint64_t next_u64(std::uint64_t& state) {
  state += 0x9E3779B97F4A7C15ull;
  std::uint64_t z = state;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

// Uniform in [0, 1) with 24 random bits (exactly representable as float).
double next_unit(std::uint64_t& state) { return static_cast<double>(next_u64(state) >> 40) * (1.0 / 16777216.0); }

// ---------------------------------------------------------------------------------------------------------------
// sin and cos of an angle given in turns (1 turn = 360 degrees), no libm. The angle is wrapped to [0, 1), split into
// a quadrant and an angle in [0, pi/4] (using the complement above pi/4), and evaluated with Taylor series that are
// accurate to about 1e-16 on that range.
void sincos_taylor(double a, double& s, double& c) {
  const double a2 = a * a;
  // sin a = a (1 - a2/3! (1 - a2/(4*5) (1 - a2/(6*7) ...))) written as nested products, 10 terms
  double ps = 1.0;
  double pc = 1.0;
  for (int k = 9; k >= 1; --k) {
    const double i = static_cast<double>(2 * k);
    ps = 1.0 - ps * a2 / ((i) * (i + 1.0));
    pc = 1.0 - pc * a2 / ((i - 1.0) * (i));
  }
  s = a * ps;
  c = pc;
}

void sincos_turns(double turns, double& s, double& c) {
  double t = turns - std::floor(turns);  // [0, 1)
  if (!(t >= 0.0 && t < 1.0)) t = 0.0;
  const double q4 = t * 4.0;
  const double qf = std::floor(q4);
  const int q = static_cast<int>(qf) & 3;
  const double r = q4 - qf;  // [0, 1) within the quadrant
  double s0, c0;
  if (r <= 0.5) {
    sincos_taylor(r * kHalfPi, s0, c0);
  } else {
    double sb, cb;
    sincos_taylor((1.0 - r) * kHalfPi, sb, cb);
    s0 = cb;
    c0 = sb;
  }
  switch (q) {
    case 0: s = s0; c = c0; break;
    case 1: s = c0; c = -s0; break;
    case 2: s = -s0; c = -c0; break;
    default: s = -c0; c = s0; break;
  }
}

// ---------------------------------------------------------------------------------------------------------------
float to_float(double v) {
  if (!(v == v)) return 0.0f;
  if (v > kFloatMax) return std::numeric_limits<float>::max();
  if (v < -kFloatMax) return -std::numeric_limits<float>::max();
  return static_cast<float>(v);
}

float clamp01(double v) {
  if (!(v > 0.0)) return 0.0f;
  if (v > 1.0) return 1.0f;
  return static_cast<float>(v);
}

bool in_range(float v, float lo, float hi) { return std::isfinite(v) && v >= lo && v <= hi; }

// Linear value a + (b - a) * f for f in [0, 1].
double lerp(double a, double b, double f) { return a + (b - a) * f; }

void update_attributes(const EmitterConfig& cfg, Particle& p) {
  double f = static_cast<double>(p.age) / static_cast<double>(p.lifetime);
  if (!(f > 0.0)) f = 0.0;
  if (f > 1.0) f = 1.0;
  const double size = lerp(cfg.size_start, cfg.size_end, f);
  p.size = size > 0.0 ? to_float(size) : 0.0f;
  for (int c = 0; c < 4; ++c) p.color[c] = clamp01(lerp(cfg.color_start[c], cfg.color_end[c], f));
}

void spawn(const EmitterConfig& cfg, std::uint64_t& rng, Particle& p) {
  // Fixed draw order, always five numbers per particle: radius, disc angle, direction, speed, lifetime.
  const double u_radius = next_unit(rng);
  const double u_disc = next_unit(rng);
  const double u_dir = next_unit(rng);
  const double u_speed = next_unit(rng);
  const double u_life = next_unit(rng);

  double s, c;
  const double radius = static_cast<double>(cfg.emit_radius) * std::sqrt(u_radius);  // uniform over the disc area
  sincos_turns(u_disc, s, c);
  p.x = to_float(static_cast<double>(cfg.position[0]) + radius * c);
  p.y = to_float(static_cast<double>(cfg.position[1]) + radius * s);

  const double turns = static_cast<double>(cfg.angle_degrees) / 360.0 +
                       (u_dir - 0.5) * (static_cast<double>(cfg.spread_degrees) / 360.0);
  sincos_turns(turns, s, c);
  const double speed = lerp(cfg.speed_min, cfg.speed_max, u_speed);
  p.vx = to_float(c * speed);
  p.vy = to_float(s * speed);

  p.age = 0.0f;
  p.lifetime = to_float(lerp(cfg.lifetime_min, cfg.lifetime_max, u_life));
  if (!(p.lifetime > 0.0f)) p.lifetime = cfg.lifetime_min;  // rounding guard, stays > 0
  update_attributes(cfg, p);
}

// ---------------------------------------------------------------------------------------------------------------
// FNV-1a 64 over explicit little-endian bytes.
struct Fnv {
  std::uint64_t h = 0xCBF29CE484222325ull;
  void byte(std::uint8_t b) {
    h ^= b;
    h *= 0x100000001B3ull;
  }
  void u32(std::uint32_t v) {
    for (int i = 0; i < 4; ++i) byte(static_cast<std::uint8_t>(v >> (8 * i)));
  }
  void u64(std::uint64_t v) {
    for (int i = 0; i < 8; ++i) byte(static_cast<std::uint8_t>(v >> (8 * i)));
  }
  void f32(float v) {
    std::uint32_t bits;
    std::memcpy(&bits, &v, sizeof bits);
    u32(bits);
  }
  void f64(double v) {
    std::uint64_t bits;
    std::memcpy(&bits, &v, sizeof bits);
    u64(bits);
  }
};

}  // namespace

ConfigStatus validate_config(const EmitterConfig& c) {
  if (!in_range(c.position[0], -1e6f, 1e6f) || !in_range(c.position[1], -1e6f, 1e6f)) return ConfigStatus::BadPosition;
  if (!in_range(c.emit_radius, 0.0f, 1e6f)) return ConfigStatus::BadRadius;
  if (!in_range(c.rate, 0.0f, 100000.0f)) return ConfigStatus::BadRate;
  if (c.max_particles < 1 || c.max_particles > 65536) return ConfigStatus::BadMaxParticles;
  if (!std::isfinite(c.lifetime_min) || !std::isfinite(c.lifetime_max) || !(c.lifetime_min > 0.0f) ||
      !(c.lifetime_min <= c.lifetime_max) || !(c.lifetime_max <= 3600.0f)) {
    return ConfigStatus::BadLifetime;
  }
  if (!in_range(c.speed_min, 0.0f, 1e6f) || !in_range(c.speed_max, 0.0f, 1e6f) || !(c.speed_min <= c.speed_max)) {
    return ConfigStatus::BadSpeed;
  }
  if (!std::isfinite(c.angle_degrees)) return ConfigStatus::BadAngle;
  if (!in_range(c.spread_degrees, 0.0f, 360.0f)) return ConfigStatus::BadSpread;
  if (!in_range(c.gravity[0], -1e6f, 1e6f) || !in_range(c.gravity[1], -1e6f, 1e6f)) return ConfigStatus::BadGravity;
  if (!in_range(c.drag, 0.0f, 100.0f)) return ConfigStatus::BadDrag;
  if (!in_range(c.size_start, 0.0f, 1e6f) || !in_range(c.size_end, 0.0f, 1e6f)) return ConfigStatus::BadSize;
  for (int i = 0; i < 4; ++i) {
    if (!in_range(c.color_start[i], 0.0f, 1.0f) || !in_range(c.color_end[i], 0.0f, 1.0f)) return ConfigStatus::BadColor;
  }
  return ConfigStatus::Ok;
}

const char* config_status_text(ConfigStatus s) {
  switch (s) {
    case ConfigStatus::Ok: return "ok";
    case ConfigStatus::BadPosition: return "position must be finite and within 1e6";
    case ConfigStatus::BadRadius: return "emit radius must be 0 to 1e6";
    case ConfigStatus::BadRate: return "rate must be 0 to 100000";
    case ConfigStatus::BadMaxParticles: return "max particles must be 1 to 65536";
    case ConfigStatus::BadLifetime: return "lifetime needs 0 < min <= max <= 3600";
    case ConfigStatus::BadSpeed: return "speed needs 0 <= min <= max <= 1e6";
    case ConfigStatus::BadAngle: return "angle must be finite";
    case ConfigStatus::BadSpread: return "spread must be 0 to 360";
    case ConfigStatus::BadGravity: return "gravity must be finite and within 1e6";
    case ConfigStatus::BadDrag: return "drag must be 0 to 100";
    case ConfigStatus::BadSize: return "size must be 0 to 1e6";
    case ConfigStatus::BadColor: return "color values must be 0 to 1";
  }
  return "unknown status";
}

bool emitter_init(const EmitterConfig& config, Emitter& out) {
  if (validate_config(config) != ConfigStatus::Ok) return false;
  Emitter e;
  e.config = config;
  e.rng_state = config.seed;
  e.particles.reserve(config.max_particles);  // the only allocation: the pool never grows past max_particles
  out = std::move(e);
  return true;
}

bool emitter_step(Emitter& e, float dt) {
  if (!std::isfinite(dt) || !(dt > 0.0f) || !(dt <= 1.0f)) return false;
  if (validate_config(e.config) != ConfigStatus::Ok) return false;
  const EmitterConfig& cfg = e.config;

  // The substep grid: n equal substeps of dt / n, n = ceil(64 dt). A dt that is k / 64 runs k substeps of exactly 1/64.
  const double dtd = static_cast<double>(dt);
  const double n_exact = std::ceil(dtd * kSubstepsPerSecond);
  const int n = n_exact < 1.0 ? 1 : (n_exact > 64.0 ? 64 : static_cast<int>(n_exact));
  const double h = dtd / static_cast<double>(n);
  const double damp = 1.0 / (1.0 + static_cast<double>(cfg.drag) * h);
  const double gx = static_cast<double>(cfg.gravity[0]) * h;
  const double gy = static_cast<double>(cfg.gravity[1]) * h;
  const std::size_t cap = cfg.max_particles;

  for (int step = 0; step < n; ++step) {
    // 1. Age and move the alive particles (semi-implicit Euler), drop the dead ones keeping the order.
    std::size_t kept = 0;
    for (std::size_t i = 0; i < e.particles.size(); ++i) {
      Particle p = e.particles[i];
      const double age = static_cast<double>(p.age) + h;
      p.age = to_float(age);
      if (!(static_cast<double>(p.age) < static_cast<double>(p.lifetime))) continue;  // dead (also for NaN)
      const double vx = (static_cast<double>(p.vx) + gx) * damp;
      const double vy = (static_cast<double>(p.vy) + gy) * damp;
      p.vx = to_float(vx);
      p.vy = to_float(vy);
      p.x = to_float(static_cast<double>(p.x) + static_cast<double>(p.vx) * h);
      p.y = to_float(static_cast<double>(p.y) + static_cast<double>(p.vy) * h);
      update_attributes(cfg, p);
      e.particles[kept++] = p;
    }
    e.particles.resize(kept);

    // 2. Emit. The fraction that does not fit in the pool is dropped, so a full pool leaves no backlog.
    e.emit_carry += static_cast<double>(cfg.rate) * h;
    const double whole = std::floor(e.emit_carry);
    e.emit_carry -= whole;
    const std::size_t room = e.particles.size() < cap ? cap - e.particles.size() : 0;
    const std::size_t count = whole >= static_cast<double>(room) ? room : static_cast<std::size_t>(whole);
    for (std::size_t i = 0; i < count; ++i) {
      Particle p;
      spawn(cfg, e.rng_state, p);
      e.particles.push_back(p);  // never reallocates: capacity was reserved for max_particles
    }
    ++e.steps;
  }
  return true;
}

std::uint64_t emitter_digest(const Emitter& e) {
  Fnv f;
  const char tag[8] = {'L', 'U', 'M', 'P', 'A', 'R', '0', '1'};
  for (char c : tag) f.byte(static_cast<std::uint8_t>(c));
  const EmitterConfig& c = e.config;
  f.f32(c.position[0]);
  f.f32(c.position[1]);
  f.f32(c.emit_radius);
  f.f32(c.rate);
  f.u32(c.max_particles);
  f.f32(c.lifetime_min);
  f.f32(c.lifetime_max);
  f.f32(c.speed_min);
  f.f32(c.speed_max);
  f.f32(c.angle_degrees);
  f.f32(c.spread_degrees);
  f.f32(c.gravity[0]);
  f.f32(c.gravity[1]);
  f.f32(c.drag);
  f.f32(c.size_start);
  f.f32(c.size_end);
  for (int i = 0; i < 4; ++i) f.f32(c.color_start[i]);
  for (int i = 0; i < 4; ++i) f.f32(c.color_end[i]);
  f.u64(c.seed);
  f.u64(static_cast<std::uint64_t>(e.particles.size()));
  for (const Particle& p : e.particles) {
    f.f32(p.x);
    f.f32(p.y);
    f.f32(p.vx);
    f.f32(p.vy);
    f.f32(p.age);
    f.f32(p.lifetime);
    f.f32(p.size);
    for (int i = 0; i < 4; ++i) f.f32(p.color[i]);
  }
  f.f64(e.emit_carry);
  f.u64(e.rng_state);
  f.u64(e.steps);
  return f.h;
}

}  // namespace lumora::particles
