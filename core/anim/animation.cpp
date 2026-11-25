// Animation clips: validation, loop mapping, keyframe sampling and the draft clip JSON parser.
// Rules, formulas, tolerance and the draft schema: docs/animation.md.
// Pure CPU code. All interpolation math runs in double and is rounded to float once at the end. Only slerp calls
// libm (sin, cos is not used; acos and sin), which is why the documented cross-host tolerance exists. fmod, sqrt,
// floor and comparisons are exact IEEE operations. No fast-math: the project build must not enable it.
#include "animation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "json_value.h"

// Fused multiply-add would change the last bits between hosts, so ask compilers that support it not to contract.
#if defined(_MSC_VER) && !defined(__clang__)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma clang fp contract(off)
#endif

namespace lumora::anim {
namespace {

constexpr std::size_t kMaxTargetBytes = 128;
constexpr std::size_t kMaxTextBytes = 4u << 20;       // clip JSON text limit
constexpr std::size_t kMaxTotalNumbers = 1u << 20;    // times plus values over the whole clip
constexpr unsigned kMaxJsonDepth = 8;
constexpr double kFloatMax = static_cast<double>(std::numeric_limits<float>::max());

// ---------------------------------------------------------------------------------------------------------------
// Validation helpers

// UTF-8 text without overlong forms, surrogates, code points above U+10FFFF or control characters (< 0x20, 0x7F).
bool is_clean_utf8(const std::string& s) {
  const std::size_t n = s.size();
  std::size_t i = 0;
  while (i < n) {
    const unsigned c = static_cast<unsigned char>(s[i]);
    if (c < 0x20 || c == 0x7F) return false;
    if (c < 0x80) {
      ++i;
      continue;
    }
    std::size_t extra = 0;
    std::uint32_t cp = 0;
    std::uint32_t min_cp = 0;
    if (c >= 0xC2 && c <= 0xDF) {
      extra = 1;
      cp = c & 0x1Fu;
      min_cp = 0x80;
    } else if (c >= 0xE0 && c <= 0xEF) {
      extra = 2;
      cp = c & 0x0Fu;
      min_cp = 0x800;
    } else if (c >= 0xF0 && c <= 0xF4) {
      extra = 3;
      cp = c & 0x07u;
      min_cp = 0x10000;
    } else {
      return false;
    }
    if (n - i <= extra) return false;  // truncated sequence
    for (std::size_t k = 1; k <= extra; ++k) {
      const unsigned cc = static_cast<unsigned char>(s[i + k]);
      if ((cc & 0xC0u) != 0x80u) return false;
      cp = (cp << 6) | (cc & 0x3Fu);
    }
    if (cp < min_cp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
    i += extra + 1;
  }
  return true;
}

bool valid_target(const std::string& s) { return !s.empty() && s.size() <= kMaxTargetBytes && is_clean_utf8(s); }

bool finite_f(float v) { return std::isfinite(v); }

// Clamps a double that is mathematically inside the float range (or overflowed by rounding) to a finite float.
float to_float(double v) {
  if (!(v == v)) return 0.0f;  // cannot happen for validated input; never emit NaN
  if (v > kFloatMax) return std::numeric_limits<float>::max();
  if (v < -kFloatMax) return -std::numeric_limits<float>::max();
  return static_cast<float>(v);
}

std::size_t floats_per_key(const Track& t) {
  return static_cast<std::size_t>(t.components) * (t.interp == Interp::CubicSpline ? 3u : 1u);
}

// Offset of the value (not a tangent) of key k.
std::size_t value_offset(const Track& t, std::size_t k) {
  const std::size_t c = static_cast<std::size_t>(t.components);
  return t.interp == Interp::CubicSpline ? (k * 3 + 1) * c : k * c;
}

// ---------------------------------------------------------------------------------------------------------------
// Sampling (the track is already validated)

struct Quat {
  double x, y, z, w;
};

double quat_length(const Quat& q) { return std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w); }

Quat quat_at(const Track& t, std::size_t offset) {
  return Quat{t.values[offset], t.values[offset + 1], t.values[offset + 2], t.values[offset + 3]};
}

// Normalizes; falls back to the identity only for a zero or non-finite length, which validation rules out for keys.
Quat normalized(const Quat& q, const Quat& fallback) {
  const double len = quat_length(q);
  if (!(len > 0.0) || !std::isfinite(len)) return fallback;
  return Quat{q.x / len, q.y / len, q.z / len, q.w / len};
}

void write_quat(const Quat& q, float* out) {
  out[0] = to_float(q.x);
  out[1] = to_float(q.y);
  out[2] = to_float(q.z);
  out[3] = to_float(q.w);
}

Quat slerp(const Quat& a, Quat b, double t) {
  double d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  if (d < 0.0) {  // shortest path: q and -q are the same rotation
    b = Quat{-b.x, -b.y, -b.z, -b.w};
    d = -d;
  }
  if (d > 1.0) d = 1.0;
  double wa;
  double wb;
  if (d > 0.9995) {  // nearly parallel: normalized lerp, avoids dividing by a tiny sine
    wa = 1.0 - t;
    wb = t;
  } else {
    const double theta = std::acos(d);
    const double s = std::sin(theta);
    wa = std::sin((1.0 - t) * theta) / s;
    wb = std::sin(t * theta) / s;
  }
  return normalized(Quat{wa * a.x + wb * b.x, wa * a.y + wb * b.y, wa * a.z + wb * b.z, wa * a.w + wb * b.w}, a);
}

// Index of the first key whose time is greater than `time` (binary search). times is strictly increasing.
std::size_t upper_index(const std::vector<float>& times, float time) {
  std::size_t lo = 0;
  std::size_t hi = times.size();
  while (lo < hi) {
    const std::size_t mid = lo + (hi - lo) / 2;
    if (times[mid] > time) {
      hi = mid;
    } else {
      lo = mid + 1;
    }
  }
  return lo;
}

void write_key(const Track& t, std::size_t k, float* out) {
  const std::size_t off = value_offset(t, k);
  if (t.is_rotation) {
    write_quat(normalized(quat_at(t, off), Quat{0, 0, 0, 1}), out);
    return;
  }
  for (int c = 0; c < t.components; ++c) out[c] = t.values[off + static_cast<std::size_t>(c)];
}

void sample_valid(const Track& t, float time, float* out) {
  const std::size_t n = t.times.size();
  if (!(time == time)) time = t.times[0];  // NaN time: first key
  if (n == 1 || time <= t.times[0]) {
    write_key(t, 0, out);
    return;
  }
  if (time >= t.times[n - 1]) {
    write_key(t, n - 1, out);
    return;
  }
  const std::size_t hi = upper_index(t.times, time);  // 1 <= hi <= n - 1 here
  const std::size_t k = hi - 1;
  if (time == t.times[k] || t.interp == Interp::Step) {
    write_key(t, k, out);
    return;
  }
  const double t0 = t.times[k];
  const double dt = static_cast<double>(t.times[hi]) - t0;
  const double u = (static_cast<double>(time) - t0) / dt;  // 0 < u < 1
  const std::size_t c = static_cast<std::size_t>(t.components);

  if (t.interp == Interp::Linear) {
    const std::size_t a = value_offset(t, k);
    const std::size_t b = value_offset(t, hi);
    if (t.is_rotation) {
      const Quat qa = normalized(quat_at(t, a), Quat{0, 0, 0, 1});
      const Quat qb = normalized(quat_at(t, b), Quat{0, 0, 0, 1});
      write_quat(slerp(qa, qb, u), out);
      return;
    }
    for (std::size_t i = 0; i < c; ++i) {
      const double va = t.values[a + i];
      const double vb = t.values[b + i];
      out[i] = to_float(va + (vb - va) * u);
    }
    return;
  }

  // CubicSpline, glTF 2.0 Hermite: values per key are in-tangent, value, out-tangent.
  const double u2 = u * u;
  const double u3 = u2 * u;
  const double h00 = 2.0 * u3 - 3.0 * u2 + 1.0;
  const double h10 = u3 - 2.0 * u2 + u;
  const double h01 = -2.0 * u3 + 3.0 * u2;
  const double h11 = u3 - u2;
  const std::size_t base_a = k * 3 * c;   // key k:   in, value, out
  const std::size_t base_b = hi * 3 * c;  // key k+1: in, value, out
  double r[4] = {0, 0, 0, 0};
  for (std::size_t i = 0; i < c; ++i) {
    const double v0 = t.values[base_a + c + i];
    const double m0 = t.values[base_a + 2 * c + i];  // out-tangent of key k
    const double v1 = t.values[base_b + c + i];
    const double m1 = t.values[base_b + i];  // in-tangent of key k+1
    r[i] = h00 * v0 + h10 * dt * m0 + h01 * v1 + h11 * dt * m1;
  }
  if (t.is_rotation) {
    const Quat fallback = normalized(quat_at(t, base_a + c), Quat{0, 0, 0, 1});
    write_quat(normalized(Quat{r[0], r[1], r[2], r[3]}, fallback), out);
    return;
  }
  for (std::size_t i = 0; i < c; ++i) out[i] = to_float(r[i]);
}

// ---------------------------------------------------------------------------------------------------------------
// Clip JSON

std::string index_path(const char* prefix, std::size_t i, const char* suffix) {
  return std::string(prefix) + "[" + std::to_string(i) + "]" + suffix;
}

ParseClipResult fail(std::string message) {
  ParseClipResult r;
  r.ok = false;
  r.message = std::move(message);
  return r;  // clip stays empty
}

// A JSON number that is a whole number inside [lo, hi].
bool whole_number(const scene::JsonValue& v, int lo, int hi, int* out) {
  if (!v.is_number()) return false;
  const double d = v.as_number();
  if (!(d >= lo && d <= hi) || d != std::floor(d)) return false;
  *out = static_cast<int>(d);
  return true;
}

// Reads a JSON array of numbers into floats. Returns an empty string on success, else a fixed reason.
const char* read_floats(const scene::JsonValue& v, std::size_t* budget, std::vector<float>* out) {
  if (!v.is_array()) return "must be an array of numbers";
  const scene::JsonValue::Array& a = v.as_array();
  if (a.size() > *budget) return "too many numbers in the clip";
  *budget -= a.size();
  out->reserve(a.size());
  for (const scene::JsonValue& e : a) {
    if (!e.is_number()) return "must contain only numbers";
    const double d = e.as_number();
    if (!(std::fabs(d) <= kFloatMax)) return "number outside the float range";
    out->push_back(static_cast<float>(d));
  }
  return "";
}

bool parse_interp(const std::string& s, Interp* out) {
  if (s == "step") {
    *out = Interp::Step;
  } else if (s == "linear") {
    *out = Interp::Linear;
  } else if (s == "cubicspline") {
    *out = Interp::CubicSpline;
  } else {
    return false;
  }
  return true;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// Public interface

TrackStatus validate_track(const Track& t) {
  const std::uint64_t n = t.times.size();
  if (n == 0) return TrackStatus::NoKeys;
  if (n > kMaxKeysPerTrack) return TrackStatus::TooManyKeys;
  if (t.components < 1 || t.components > 4) return TrackStatus::BadComponents;
  if (t.is_rotation && t.components != 4) return TrackStatus::BadComponents;
  // n <= 2^20 and components <= 4, so this product cannot overflow 64 bits.
  const std::uint64_t expected = n * static_cast<std::uint64_t>(floats_per_key(t));
  if (static_cast<std::uint64_t>(t.values.size()) != expected) return TrackStatus::BadValueCount;
  float prev = 0.0f;
  for (std::size_t i = 0; i < t.times.size(); ++i) {
    const float v = t.times[i];
    if (!finite_f(v) || v < 0.0f) return TrackStatus::TimesNotIncreasing;
    if (i > 0 && !(v > prev)) return TrackStatus::TimesNotIncreasing;
    prev = v;
  }
  for (float v : t.values) {
    if (!finite_f(v)) return TrackStatus::NonFinite;
  }
  if (t.is_rotation) {
    for (std::size_t k = 0; k < t.times.size(); ++k) {
      const std::size_t off = value_offset(t, k);
      if (!(quat_length(quat_at(t, off)) > 0.0)) return TrackStatus::ZeroQuaternion;
    }
  }
  if (!valid_target(t.target)) return TrackStatus::BadTarget;
  return TrackStatus::Ok;
}

const char* track_status_text(TrackStatus s) {
  switch (s) {
    case TrackStatus::Ok: return "ok";
    case TrackStatus::NoKeys: return "track has no keys";
    case TrackStatus::TooManyKeys: return "track has too many keys";
    case TrackStatus::BadComponents: return "bad component count";
    case TrackStatus::BadValueCount: return "wrong number of values";
    case TrackStatus::TimesNotIncreasing: return "times must be finite, not negative and strictly increasing";
    case TrackStatus::NonFinite: return "a value is not finite";
    case TrackStatus::ZeroQuaternion: return "a rotation key has length zero";
    case TrackStatus::BadTarget: return "bad target name";
  }
  return "unknown status";
}

float track_duration(const Track& t) {
  if (validate_track(t) != TrackStatus::Ok) return 0.0f;
  return t.times.back();
}

float clip_duration(const Clip& c) {
  float d = 0.0f;
  for (const Track& t : c.tracks) d = std::max(d, track_duration(t));
  return d;
}

float apply_loop(float time, float duration, Loop mode) {
  if (!std::isfinite(time) || !std::isfinite(duration) || !(time > 0.0f) || !(duration > 0.0f)) return 0.0f;
  const double t = time;
  const double d = duration;
  double r;
  switch (mode) {
    case Loop::Clamp:
      r = t < d ? t : d;
      break;
    case Loop::Repeat:
      r = std::fmod(t, d);  // exact; in [0, d), so a multiple of the duration maps to 0
      break;
    case Loop::PingPong: {
      const double m = std::fmod(t, 2.0 * d);  // exact; 2 * d cannot overflow a double
      r = m <= d ? m : 2.0 * d - m;
      break;
    }
    default:
      r = 0.0;
      break;
  }
  if (!(r >= 0.0)) r = 0.0;
  if (r > d) r = d;
  return static_cast<float>(r);
}

bool sample_track(const Track& t, float time, float* out) {
  if (out == nullptr || validate_track(t) != TrackStatus::Ok) return false;
  sample_valid(t, time, out);
  return true;
}

ParseClipResult parse_clip_json(const std::string& text) {
  if (text.size() > kMaxTextBytes) return fail("$: text too large");
  const scene::JsonParseResult doc = scene::parse_json(text, kMaxJsonDepth);
  if (!doc.ok) return fail("$: invalid JSON, too deep or duplicate key");
  const scene::JsonValue& root = doc.value;
  if (!root.is_object()) return fail("$: must be an object");

  Clip clip;
  if (const scene::JsonValue* name = root.find("name")) {
    if (!name->is_string()) return fail("name: must be a string");
    if (name->as_string().size() > kMaxTargetBytes || !is_clean_utf8(name->as_string())) {
      return fail("name: at most 128 bytes of valid UTF-8 without control characters");
    }
    clip.name = name->as_string();
  }
  const scene::JsonValue* tracks = root.find("tracks");
  if (tracks == nullptr || !tracks->is_array()) return fail("tracks: must be an array");
  const scene::JsonValue::Array& list = tracks->as_array();
  if (list.size() > kMaxTracksPerClip) return fail("tracks: too many tracks");

  std::size_t budget = kMaxTotalNumbers;
  clip.tracks.reserve(list.size());
  for (std::size_t i = 0; i < list.size(); ++i) {
    const scene::JsonValue& jt = list[i];
    if (!jt.is_object()) return fail(index_path("tracks", i, ": must be an object"));
    Track t;

    const scene::JsonValue* target = jt.find("target");
    if (target == nullptr || !target->is_string()) return fail(index_path("tracks", i, ".target: must be a string"));
    t.target = target->as_string();

    const scene::JsonValue* comps = jt.find("components");
    if (comps == nullptr || !whole_number(*comps, 1, 4, &t.components)) {
      return fail(index_path("tracks", i, ".components: must be a whole number from 1 to 4"));
    }
    if (const scene::JsonValue* rot = jt.find("rotation")) {
      if (!rot->is_bool()) return fail(index_path("tracks", i, ".rotation: must be true or false"));
      t.is_rotation = rot->as_bool();
    }
    if (const scene::JsonValue* ip = jt.find("interpolation")) {
      if (!ip->is_string() || !parse_interp(ip->as_string(), &t.interp)) {
        return fail(index_path("tracks", i, ".interpolation: must be step, linear or cubicspline"));
      }
    }
    const scene::JsonValue* times = jt.find("times");
    if (times == nullptr) return fail(index_path("tracks", i, ".times: missing"));
    if (times->is_array() && times->as_array().size() > kMaxKeysPerTrack) {
      return fail(index_path("tracks", i, ".times: too many keys"));
    }
    if (const char* why = read_floats(*times, &budget, &t.times); why[0] != '\0') {
      return fail(index_path("tracks", i, std::string(".times: ").append(why).c_str()));
    }
    const scene::JsonValue* values = jt.find("values");
    if (values == nullptr) return fail(index_path("tracks", i, ".values: missing"));
    if (const char* why = read_floats(*values, &budget, &t.values); why[0] != '\0') {
      return fail(index_path("tracks", i, std::string(".values: ").append(why).c_str()));
    }

    const TrackStatus st = validate_track(t);
    if (st != TrackStatus::Ok) {
      return fail(index_path("tracks", i, std::string(": ").append(track_status_text(st)).c_str()));
    }
    clip.tracks.push_back(std::move(t));
  }

  ParseClipResult r;
  r.ok = true;
  r.message = "ok";
  r.clip = std::move(clip);
  return r;
}

}  // namespace lumora::anim
