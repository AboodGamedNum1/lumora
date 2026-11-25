// Animation clips: keyframe tracks and a deterministic sampler (milestone M4 groundwork). Pure CPU math, no GPU,
// no file system, no dependencies. Draft: the clip JSON schema is proposed in docs/animation.md and is not part of the
// frozen package format until the owner accepts it.
// Every function must be total: no exception, no out-of-bounds read, no NaN or infinity in any output, and the same
// inputs give the same bits on every host (no fast-math, no platform math whose result differs between libms beyond
// the documented tolerance; the contributor documents the tolerance for sin/cos/acos used by slerp).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lumora::anim {

enum class Interp { Step, Linear, CubicSpline };  // CubicSpline follows glTF 2.0: in-tangent, value, out-tangent per key
enum class Loop { Clamp, Repeat, PingPong };

struct Track {
  std::string target;       // free text path such as "layer/hills/position"; 1 to 128 bytes, no control characters
  int components = 1;       // 1, 2, 3 or 4 floats per value
  bool is_rotation = false; // requires components == 4 (x, y, z, w quaternion): interpolated with normalized slerp
  Interp interp = Interp::Linear;
  std::vector<float> times;   // seconds, strictly increasing, finite, >= 0
  std::vector<float> values;  // Step/Linear: times.size() * components. CubicSpline: times.size() * components * 3
};

struct Clip {
  std::string name;
  std::vector<Track> tracks;
};

enum class TrackStatus {
  Ok,
  NoKeys,
  TooManyKeys,
  BadComponents,        // not 1..4, or is_rotation with components != 4
  BadValueCount,
  TimesNotIncreasing,   // equal, decreasing, negative or non-finite times
  NonFinite,            // a NaN or infinite value
  ZeroQuaternion,       // a rotation key with length zero
  BadTarget
};

constexpr std::size_t kMaxKeysPerTrack = 1u << 20;
constexpr std::size_t kMaxTracksPerClip = 4096;

TrackStatus validate_track(const Track& t);
const char* track_status_text(TrackStatus s);  // short fixed text, never input bytes

// Time of the last key of the track (0 for an invalid or empty track), and of the longest track of the clip.
float track_duration(const Track& t);
float clip_duration(const Clip& c);

// Maps an absolute time to a time inside [0, duration]. Clamp holds the ends, Repeat wraps, PingPong mirrors.
// A non-finite time, a negative time or a zero or negative duration gives 0. Never returns NaN.
float apply_loop(float time, float duration, Loop mode);

// Writes t.components floats to out. Returns false (out untouched) unless validate_track(t) is Ok.
// Before the first key and after the last key the value is held (no extrapolation). Rotations are returned normalized.
bool sample_track(const Track& t, float time, float* out);

struct ParseClipResult {
  bool ok = false;
  std::string message;  // short fixed text with a JSON path, never input bytes
  Clip clip;
};

// Parses and validates a clip from JSON text (schema in docs/animation.md) using core/scene/json_value.h.
// The text is untrusted: depth, key and track counts are limited, and every track is validated.
ParseClipResult parse_clip_json(const std::string& text);

}  // namespace lumora::anim
