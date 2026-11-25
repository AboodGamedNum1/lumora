// Easing curves and a tween helper (milestone M4 groundwork). Pure CPU math, no dependencies, total functions:
// no exception, no NaN or infinity in any output, deterministic (same documented tolerance rules as animation.h).
// Exact formulas, tolerance and name table are written by the contributor in docs/easing.md.
#pragma once
#include <string>

#include "animation.h"  // Loop

namespace lumora::anim {

enum class Ease {
  Linear,
  QuadIn, QuadOut, QuadInOut,
  CubicIn, CubicOut, CubicInOut,
  SineIn, SineOut, SineInOut,
  ExpoIn, ExpoOut, ExpoInOut,
  BackIn, BackOut, BackInOut,
  ElasticIn, ElasticOut, ElasticInOut,
  BounceIn, BounceOut, BounceInOut
};

// Eased progress for t in [0,1]. t is clamped to [0,1]; NaN and -infinity give 0, +infinity gives 1.
// ease(kind, 0) is exactly 0 and ease(kind, 1) is exactly 1 for every kind. Back, Elastic may overshoot between.
float ease(Ease kind, float t);

// Lower-case names: "linear", "quad-in", "quad-out", "quad-in-out", "cubic-in", ... "bounce-in-out".
const char* ease_name(Ease kind);                    // fixed text for every value
bool ease_from_name(const std::string& name, Ease& out);  // false (out untouched) for an unknown name

// CSS-style cubic Bezier timing function through (0,0), (x1,y1), (x2,y2), (1,1). Needs x1 and x2 in [0,1] and all
// four numbers finite, else returns false (out untouched). Solves x(s) = t with a bounded iteration (documented),
// accurate to 1e-6 in x. t is clamped like ease().
bool cubic_bezier(float x1, float y1, float x2, float y2, float t, float& out);

struct Tween {
  float from = 0.0f;
  float to = 1.0f;
  float duration = 1.0f;  // seconds, must be finite and > 0
  float delay = 0.0f;     // seconds before the tween starts, finite and >= 0
  Ease easing = Ease::Linear;
  Loop loop = Loop::Clamp;
};

// Value of the tween at an absolute time. Before the delay the value is `from`. Invalid tweens (non-finite numbers,
// duration <= 0, negative delay) return false and leave `out` untouched. The result is finite: overflow clamps to the
// largest finite float. Loop modes map time after the delay through apply_loop with the tween duration.
bool tween_value(const Tween& tween, float time, float& out);

}  // namespace lumora::anim
