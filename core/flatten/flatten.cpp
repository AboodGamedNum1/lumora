// Lumora CPU Bezier flattener (scope X). Integer only, no heap, no recursion, no mutable statics. See docs/flatten.md.
#include "flatten.h"

namespace lumora::flatten {

namespace {

using i64 = std::int64_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

bool coord_bad(std::int32_t v) { return v < -kMaxCoord || v > kMaxCoord; }
bool point_bad(const Point& p) { return coord_bad(p.x) || coord_bad(p.y); }
bool tol_bad(std::int32_t t) { return t < 1 || t > kMaxTolerance; }

// Squared norm of (p0 - 2 c + p1)-style vectors: components are at most 4 * 2^24 = 2^26, so the sum of two squares is at most 2^53.
i64 norm2(i64 x, i64 y) { return x * x + y * y; }

// Smallest n in 1 .. kMaxSegments with k * M <= 4 * T where T = tol * n * n, k = 1 (quadratic) or 3 (cubic). The test is the
// literal "k^2 M^2 <= 16 T^2" without forming 16 T^2 above the proved threshold: M <= 2^26 * sqrt(2) < 2^26.5, so k * M < 2^28.1
// for k = 3 and < 2^26.5 for k = 1. If 4 T >= 2^29 (cubic) or 4 T >= 2^27 (quadratic) the answer is true. Below the threshold
// T < 2^27 (cubic) or T < 2^25 (quadratic), so 16 T^2 < 2^58 resp. 2^54 and k^2 M^2 <= 9 * 2^53 < 2^57: no overflow.
u32 segments(i64 m2, i64 k2, i64 tol, i64 t_threshold) {
  for (i64 n = 1; n < static_cast<i64>(kMaxSegments); ++n) {
    const i64 t = tol * n * n;  // at most 2^16 * 2^16 = 2^32
    if (t >= t_threshold) return static_cast<u32>(n);
    if (k2 * m2 <= 16 * t * t) return static_cast<u32>(n);
  }
  return kMaxSegments;
}

// Nearest integer of num / den for den > 0, ties away from zero: sign(num) * ((2 * |num| + den) / (2 * den)). |num| <= 2^48,
// den <= 2^24, so 2 * |num| + den fits int64.
i64 round_away(i64 num, i64 den) {
  const i64 a = num < 0 ? -num : num;
  const i64 q = (2 * a + den) / (2 * den);
  return num < 0 ? -q : q;
}

// Overlap of two touched byte ranges. Same modular address rule as core/composite, core/resample, core/sdf, core/tess, core/clip and
// core/stroke: clear the top byte, take the unsigned difference modulo 2^64, answer "no overlap" at |delta| >= 2^47 before any
// other arithmetic, then compare the exact ranges. Both lengths are positive.
bool ranges_overlap(const void* a, u64 a_len, const void* b, u64 b_len) {
  const u64 kTagMask = 0x00FFFFFFFFFFFFFFull;
  const u64 ua = static_cast<u64>(reinterpret_cast<std::uintptr_t>(a)) & kTagMask;
  const u64 ub = static_cast<u64>(reinterpret_cast<std::uintptr_t>(b)) & kTagMask;
  const u64 diff = ua - ub;
  const bool negative = diff >= (static_cast<u64>(1) << 63);
  const u64 absd = negative ? (static_cast<u64>(0) - diff) : diff;
  if (absd >= (static_cast<u64>(1) << 47)) return false;
  const i64 delta = negative ? -static_cast<i64>(absd) : static_cast<i64>(absd);  // start of A relative to the start of B
  return delta < static_cast<i64>(b_len) && delta + static_cast<i64>(a_len) > 0;
}

Status check_common(std::int32_t tol, const Point* pts, int count, const Point* out, std::uint32_t cap, const std::uint32_t* out_count) {
  if (out == nullptr || out_count == nullptr) return Status::NullBuffer;
  if (tol_bad(tol)) return Status::BadTolerance;
  for (int i = 0; i < count; ++i)
    if (point_bad(pts[i])) return Status::BadPoint;
  if (cap < kMaxOutputPoints) return Status::BufferTooSmall;
  if (ranges_overlap(out, static_cast<u64>(kMaxOutputPoints) * sizeof(Point), out_count, sizeof(std::uint32_t)))
    return Status::Overlap;
  return Status::Ok;
}

}  // namespace

const char* status_text(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::NullBuffer: return "null buffer";
    case Status::BadTolerance: return "bad tolerance";
    case Status::BadPoint: return "bad point";
    case Status::BufferTooSmall: return "buffer too small";
    case Status::Overlap: return "overlap";
  }
  return "unknown";
}

std::uint32_t quad_segments(Point p0, Point c, Point p1, std::int32_t tolerance) {
  if (tol_bad(tolerance) || point_bad(p0) || point_bad(c) || point_bad(p1)) return 0;
  const i64 dx = static_cast<i64>(p0.x) - 2 * static_cast<i64>(c.x) + p1.x;
  const i64 dy = static_cast<i64>(p0.y) - 2 * static_cast<i64>(c.y) + p1.y;
  return segments(norm2(dx, dy), 1, tolerance, static_cast<i64>(1) << 25);
}

std::uint32_t cubic_segments(Point p0, Point c0, Point c1, Point p1, std::int32_t tolerance) {
  if (tol_bad(tolerance) || point_bad(p0) || point_bad(c0) || point_bad(c1) || point_bad(p1)) return 0;
  const i64 ax = static_cast<i64>(p0.x) - 2 * static_cast<i64>(c0.x) + c1.x;
  const i64 ay = static_cast<i64>(p0.y) - 2 * static_cast<i64>(c0.y) + c1.y;
  const i64 bx = static_cast<i64>(c0.x) - 2 * static_cast<i64>(c1.x) + p1.x;
  const i64 by = static_cast<i64>(c0.y) - 2 * static_cast<i64>(c1.y) + p1.y;
  const i64 a2 = norm2(ax, ay), b2 = norm2(bx, by);
  return segments(a2 > b2 ? a2 : b2, 9, tolerance, static_cast<i64>(1) << 27);
}

Status flatten_quad(Point p0, Point c, Point p1, std::int32_t tolerance, Point* out, std::uint32_t out_capacity,
                    std::uint32_t* out_count) {
  const Point pts[3] = {p0, c, p1};
  const Status st = check_common(tolerance, pts, 3, out, out_capacity, out_count);
  if (st != Status::Ok) return st;
  const i64 n = quad_segments(p0, c, p1, tolerance);
  const i64 den = n * n;
  for (i64 i = 0; i <= n; ++i) {
    const i64 a = n - i;
    const i64 w0 = a * a, w1 = 2 * a * i, w2 = i * i;  // sum is n^2
    const i64 x = w0 * p0.x + w1 * c.x + w2 * p1.x;
    const i64 y = w0 * p0.y + w1 * c.y + w2 * p1.y;
    out[i] = Point{static_cast<std::int32_t>(round_away(x, den)), static_cast<std::int32_t>(round_away(y, den))};
  }
  out[0] = p0;
  out[n] = p1;
  *out_count = static_cast<u32>(n + 1);
  return Status::Ok;
}

Status flatten_cubic(Point p0, Point c0, Point c1, Point p1, std::int32_t tolerance, Point* out, std::uint32_t out_capacity,
                     std::uint32_t* out_count) {
  const Point pts[4] = {p0, c0, c1, p1};
  const Status st = check_common(tolerance, pts, 4, out, out_capacity, out_count);
  if (st != Status::Ok) return st;
  const i64 n = cubic_segments(p0, c0, c1, p1, tolerance);
  const i64 den = n * n * n;
  for (i64 i = 0; i <= n; ++i) {
    const i64 a = n - i;
    const i64 w0 = a * a * a, w1 = 3 * a * a * i, w2 = 3 * a * i * i, w3 = i * i * i;  // sum is n^3
    const i64 x = w0 * p0.x + w1 * c0.x + w2 * c1.x + w3 * p1.x;
    const i64 y = w0 * p0.y + w1 * c0.y + w2 * c1.y + w3 * p1.y;
    out[i] = Point{static_cast<std::int32_t>(round_away(x, den)), static_cast<std::int32_t>(round_away(y, den))};
  }
  out[0] = p0;
  out[n] = p1;
  *out_count = static_cast<u32>(n + 1);
  return Status::Ok;
}

std::uint64_t digest_points(const Point* pts, std::size_t count) {
  u64 h = 0xcbf29ce484222325ull;
  if (pts == nullptr) return h;
  for (std::size_t i = 0; i < count; ++i) {
    const u32 w[2] = {static_cast<u32>(pts[i].x), static_cast<u32>(pts[i].y)};
    for (int k = 0; k < 2; ++k)
      for (int b = 0; b < 4; ++b) {
        h ^= (w[k] >> (8 * b)) & 0xFFu;
        h *= 0x100000001b3ull;
      }
  }
  return h;
}

}  // namespace lumora::flatten
