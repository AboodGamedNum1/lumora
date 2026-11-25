// Easing curves, CSS cubic Bezier and a tween helper. Formulas, name table, tolerance and every edge case:
// docs/easing.md. All math runs in double and is rounded to float once at the end. Only sin, cos and pow use libm
// (results can differ in the last bits between C libraries, see the documented tolerance). Nothing depends on the
// floating point rounding mode or on fast-math.
#include "easing.h"

#include <cmath>
#include <limits>

// Ask compilers that support it not to fuse multiply-add, which would change the last bits between hosts.
#if defined(_MSC_VER) && !defined(__clang__)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma clang fp contract(off)
#endif

namespace lumora::anim {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kBackC1 = 1.70158;
constexpr double kBackC2 = kBackC1 * 1.525;
constexpr double kBackC3 = kBackC1 + 1.0;
constexpr double kElasticC4 = (2.0 * kPi) / 3.0;
constexpr double kElasticC5 = (2.0 * kPi) / 4.5;
constexpr double kBounceN1 = 7.5625;
constexpr double kBounceD1 = 2.75;
constexpr double kFloatMax = static_cast<double>(std::numeric_limits<float>::max());

constexpr int kNewtonIterations = 8;
constexpr int kBisectionIterations = 60;
constexpr double kBezierSolveTolerance = 1e-9;  // far below the documented 1e-6 accuracy in x

double to_unit_clamped(float t) {
  if (!(t == t)) return 0.0;  // NaN
  if (t <= 0.0f) return 0.0;  // includes -infinity
  if (t >= 1.0f) return 1.0;  // includes +infinity
  return static_cast<double>(t);
}

float to_float(double v) {
  if (!(v == v)) return 0.0f;
  if (v > kFloatMax) return std::numeric_limits<float>::max();
  if (v < -kFloatMax) return -std::numeric_limits<float>::max();
  return static_cast<float>(v);
}

double bounce_out(double t) {
  if (t < 1.0 / kBounceD1) return kBounceN1 * t * t;
  if (t < 2.0 / kBounceD1) {
    t -= 1.5 / kBounceD1;
    return kBounceN1 * t * t + 0.75;
  }
  if (t < 2.5 / kBounceD1) {
    t -= 2.25 / kBounceD1;
    return kBounceN1 * t * t + 0.9375;
  }
  t -= 2.625 / kBounceD1;
  return kBounceN1 * t * t + 0.984375;
}

// t is strictly inside (0, 1) here.
double ease_inner(Ease kind, double t) {
  switch (kind) {
    case Ease::Linear: return t;

    case Ease::QuadIn: return t * t;
    case Ease::QuadOut: return 1.0 - (1.0 - t) * (1.0 - t);
    case Ease::QuadInOut: {
      if (t < 0.5) return 2.0 * t * t;
      const double a = -2.0 * t + 2.0;
      return 1.0 - a * a / 2.0;
    }

    case Ease::CubicIn: return t * t * t;
    case Ease::CubicOut: {
      const double a = 1.0 - t;
      return 1.0 - a * a * a;
    }
    case Ease::CubicInOut: {
      if (t < 0.5) return 4.0 * t * t * t;
      const double a = -2.0 * t + 2.0;
      return 1.0 - a * a * a / 2.0;
    }

    case Ease::SineIn: return 1.0 - std::cos(t * kPi / 2.0);
    case Ease::SineOut: return std::sin(t * kPi / 2.0);
    case Ease::SineInOut: return -(std::cos(kPi * t) - 1.0) / 2.0;

    case Ease::ExpoIn: return std::pow(2.0, 10.0 * t - 10.0);
    case Ease::ExpoOut: return 1.0 - std::pow(2.0, -10.0 * t);
    case Ease::ExpoInOut:
      if (t < 0.5) return std::pow(2.0, 20.0 * t - 10.0) / 2.0;
      return (2.0 - std::pow(2.0, -20.0 * t + 10.0)) / 2.0;

    case Ease::BackIn: return kBackC3 * t * t * t - kBackC1 * t * t;
    case Ease::BackOut: {
      const double a = t - 1.0;
      return 1.0 + kBackC3 * a * a * a + kBackC1 * a * a;
    }
    case Ease::BackInOut:
      if (t < 0.5) {
        const double a = 2.0 * t;
        return (a * a * ((kBackC2 + 1.0) * a - kBackC2)) / 2.0;
      } else {
        const double a = 2.0 * t - 2.0;
        return (a * a * ((kBackC2 + 1.0) * a + kBackC2) + 2.0) / 2.0;
      }

    case Ease::ElasticIn: return -std::pow(2.0, 10.0 * t - 10.0) * std::sin((10.0 * t - 10.75) * kElasticC4);
    case Ease::ElasticOut: return std::pow(2.0, -10.0 * t) * std::sin((10.0 * t - 0.75) * kElasticC4) + 1.0;
    case Ease::ElasticInOut:
      if (t < 0.5) return -(std::pow(2.0, 20.0 * t - 10.0) * std::sin((20.0 * t - 11.125) * kElasticC5)) / 2.0;
      return (std::pow(2.0, -20.0 * t + 10.0) * std::sin((20.0 * t - 11.125) * kElasticC5)) / 2.0 + 1.0;

    case Ease::BounceIn: return 1.0 - bounce_out(1.0 - t);
    case Ease::BounceOut: return bounce_out(t);
    case Ease::BounceInOut:
      if (t < 0.5) return (1.0 - bounce_out(1.0 - 2.0 * t)) / 2.0;
      return (1.0 + bounce_out(2.0 * t - 1.0)) / 2.0;
  }
  return t;  // an enum value outside the list behaves like Linear
}

struct NameEntry {
  Ease kind;
  const char* name;
};
constexpr NameEntry kNames[] = {
    {Ease::Linear, "linear"},
    {Ease::QuadIn, "quad-in"},       {Ease::QuadOut, "quad-out"},       {Ease::QuadInOut, "quad-in-out"},
    {Ease::CubicIn, "cubic-in"},     {Ease::CubicOut, "cubic-out"},     {Ease::CubicInOut, "cubic-in-out"},
    {Ease::SineIn, "sine-in"},       {Ease::SineOut, "sine-out"},       {Ease::SineInOut, "sine-in-out"},
    {Ease::ExpoIn, "expo-in"},       {Ease::ExpoOut, "expo-out"},       {Ease::ExpoInOut, "expo-in-out"},
    {Ease::BackIn, "back-in"},       {Ease::BackOut, "back-out"},       {Ease::BackInOut, "back-in-out"},
    {Ease::ElasticIn, "elastic-in"}, {Ease::ElasticOut, "elastic-out"}, {Ease::ElasticInOut, "elastic-in-out"},
    {Ease::BounceIn, "bounce-in"},   {Ease::BounceOut, "bounce-out"},   {Ease::BounceInOut, "bounce-in-out"},
};

// Bezier coordinate with control values p1, p2 and end points 0 and 1.
double bez(double p1, double p2, double s) {
  const double u = 1.0 - s;
  return 3.0 * u * u * s * p1 + 3.0 * u * s * s * p2 + s * s * s;
}
double bez_derivative(double p1, double p2, double s) {
  const double u = 1.0 - s;
  return 3.0 * u * u * p1 + 6.0 * u * s * (p2 - p1) + 3.0 * s * s * (1.0 - p2);
}

}  // namespace

float ease(Ease kind, float t) {
  const double u = to_unit_clamped(t);
  if (u <= 0.0) return 0.0f;  // exact endpoints for every kind
  if (u >= 1.0) return 1.0f;
  return to_float(ease_inner(kind, u));
}

const char* ease_name(Ease kind) {
  for (const NameEntry& e : kNames) {
    if (e.kind == kind) return e.name;
  }
  return "unknown";
}

bool ease_from_name(const std::string& name, Ease& out) {
  for (const NameEntry& e : kNames) {
    if (name == e.name) {  // std::string comparison: embedded NUL bytes and any extra length never match
      out = e.kind;
      return true;
    }
  }
  return false;
}

bool cubic_bezier(float x1, float y1, float x2, float y2, float t, float& out) {
  if (!std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(x2) || !std::isfinite(y2)) return false;
  if (!(x1 >= 0.0f && x1 <= 1.0f && x2 >= 0.0f && x2 <= 1.0f)) return false;
  const double target = to_unit_clamped(t);
  if (target <= 0.0) {
    out = 0.0f;
    return true;
  }
  if (target >= 1.0) {
    out = 1.0f;
    return true;
  }
  const double px1 = x1, py1 = y1, px2 = x2, py2 = y2;

  // x(s) is non-decreasing on [0, 1] because x1 and x2 are in [0, 1]. Newton steps first (bounded), bisection second.
  double s = target;
  bool solved = false;
  for (int i = 0; i < kNewtonIterations; ++i) {
    const double err = bez(px1, px2, s) - target;
    if (std::fabs(err) < kBezierSolveTolerance) {
      solved = true;
      break;
    }
    const double d = bez_derivative(px1, px2, s);
    if (!(d > 1e-6)) break;  // flat: leave it to bisection
    const double next = s - err / d;
    if (!(next > 0.0 && next < 1.0)) break;  // left the interval: leave it to bisection
    s = next;
  }
  if (!solved) {
    double lo = 0.0, hi = 1.0;
    for (int i = 0; i < kBisectionIterations; ++i) {
      s = 0.5 * (lo + hi);
      const double x = bez(px1, px2, s);
      if (std::fabs(x - target) < kBezierSolveTolerance) break;
      if (x < target) {
        lo = s;
      } else {
        hi = s;
      }
    }
  }
  out = to_float(bez(py1, py2, s));
  return true;
}

bool tween_value(const Tween& tween, float time, float& out) {
  if (!std::isfinite(tween.from) || !std::isfinite(tween.to) || !std::isfinite(tween.duration) ||
      !std::isfinite(tween.delay) || !(tween.duration > 0.0f) || tween.delay < 0.0f) {
    return false;
  }
  float value;
  if (!(time >= tween.delay)) {  // before the delay, and also a NaN time
    value = tween.from;
  } else {
    const double elapsed = static_cast<double>(time) - static_cast<double>(tween.delay);
    // A finite elapsed time fits a float (time is at most FLT_MAX and delay is not negative). An infinite time gives
    // an infinite elapsed time, which apply_loop maps to 0.
    const float loop_time = apply_loop(static_cast<float>(elapsed), tween.duration, tween.loop);
    const double progress = static_cast<double>(loop_time) / static_cast<double>(tween.duration);
    const float eased = ease(tween.easing, static_cast<float>(progress));
    if (eased == 0.0f) {
      value = tween.from;
    } else if (eased == 1.0f) {
      value = tween.to;
    } else {
      const double from = tween.from;
      const double to = tween.to;
      value = to_float(from + (to - from) * static_cast<double>(eased));
    }
  }
  out = value;
  return true;
}

}  // namespace lumora::anim
