// Lumora CPU rectangle clipping (scope V). Integer only, no heap, no recursion, no mutable statics. See docs/clip.md.
#include "clip.h"

namespace lumora::clip {

namespace {

using i64 = std::int64_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

bool coord_bad(std::int32_t v) { return v < -kMaxCoord || v > kMaxCoord; }
bool point_bad(const Point& p) { return coord_bad(p.x) || coord_bad(p.y); }
bool rect_bad(const Rect& r) {
  return r.x0 > r.x1 || r.y0 > r.y1 || coord_bad(r.x0) || coord_bad(r.y0) || coord_bad(r.x1) || coord_bad(r.y1);
}

// Floor division for a positive divisor (never truncation toward zero on a negative dividend).
i64 floor_div(i64 a, i64 b) {
  i64 q = a / b;
  if (a % b < 0) --q;
  return q;
}

// Nearest integer of d * t / m, ties toward positive infinity: floor((2 * d * t + m) / (2 * m)). Requires m > 0, |d| <= 2^29,
// |t| <= 2^29, m <= 2^29, so |2 * d * t + m| <= 2^59 + 2^29 and 2 * m <= 2^30: exact in int64.
i64 round_ratio(i64 d, i64 t, i64 m) { return floor_div(2 * d * t + m, 2 * m); }

std::int32_t clamp_to(i64 v, std::int32_t lo, std::int32_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return static_cast<std::int32_t>(v);
}

// A fraction n / m with m > 0.
struct Frac {
  i64 n, m;
};
// a < b for fractions with positive denominators (cross products are at most 2^58).
bool frac_less(const Frac& a, const Frac& b) { return a.n * b.m < b.n * a.m; }

// Exact Liang-Barsky clip without argument checks. Writes *a and *b only when the result is visible.
bool clip_segment_raw(const Rect& r, const Point& p0, const Point& p1, Point* a, Point* b) {
  const i64 dx = static_cast<i64>(p1.x) - p0.x, dy = static_cast<i64>(p1.y) - p0.y;
  const i64 ps[4] = {-dx, dx, -dy, dy};
  const i64 qs[4] = {static_cast<i64>(p0.x) - r.x0, static_cast<i64>(r.x1) - p0.x, static_cast<i64>(p0.y) - r.y0,
                     static_cast<i64>(r.y1) - p0.y};
  Frac te{0, 1}, tl{1, 1};
  for (int k = 0; k < 4; ++k) {
    const i64 p = ps[k], q = qs[k];
    if (p == 0) {
      if (q < 0) return false;  // parallel to this edge and outside it
      continue;
    }
    if (p < 0) {  // entering: t = q / p = (-q) / (-p)
      const Frac t{-q, -p};
      if (frac_less(te, t)) te = t;
    } else {  // leaving: t = q / p
      const Frac t{q, p};
      if (frac_less(t, tl)) tl = t;
    }
  }
  if (frac_less(tl, te)) return false;
  const Point s{clamp_to(static_cast<i64>(p0.x) + round_ratio(dx, te.n, te.m), r.x0, r.x1),
                clamp_to(static_cast<i64>(p0.y) + round_ratio(dy, te.n, te.m), r.y0, r.y1)};
  const Point e{clamp_to(static_cast<i64>(p0.x) + round_ratio(dx, tl.n, tl.m), r.x0, r.x1),
                clamp_to(static_cast<i64>(p0.y) + round_ratio(dy, tl.n, tl.m), r.y0, r.y1)};
  *a = s;
  *b = e;
  return true;
}

// g(m) = m + floor(m / 2): the most points one Sutherland-Hodgman stage can write for m input points.
u32 grow(u32 m) { return m + m / 2; }

bool count_ok(u32 n, u32 lo) { return n >= lo && n <= kMaxPoints; }

// Overlap of two touched byte ranges. Same modular address rule as core/composite, core/resample, core/sdf and core/tess: clear the
// top byte, take the unsigned difference modulo 2^64, answer "no overlap" at |delta| >= 2^47 before any other arithmetic, then
// compare the exact ranges. Both lengths are positive.
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

struct Extent {
  const void* p;
  u64 bytes;
};
template <std::size_t N>
bool any_overlap(const Extent (&e)[N]) {
  for (std::size_t i = 0; i < N; ++i)
    for (std::size_t j = i + 1; j < N; ++j)
      if (ranges_overlap(e[i].p, e[i].bytes, e[j].p, e[j].bytes)) return true;
  return false;
}

// Intersection of the edge p -> q with the line (axis coordinate == c). The other coordinate is rounded by the header rule.
// The edge strictly crosses the line, so m != 0.
Point intersect(const Point& p, const Point& q, bool on_x, i64 c) {
  const i64 pa = on_x ? p.x : p.y, qa = on_x ? q.x : q.y;
  const i64 po = on_x ? p.y : p.x, qo = on_x ? q.y : q.x;
  i64 m = qa - pa, t = c - pa;
  if (m < 0) {
    m = -m;
    t = -t;
  }
  const i64 v = po + round_ratio(qo - po, t, m);
  const std::int32_t cc = static_cast<std::int32_t>(c), vv = static_cast<std::int32_t>(v);
  return on_x ? Point{cc, vv} : Point{vv, cc};
}

// One Sutherland-Hodgman stage. Inside is closed: coordinate >= c (keep_ge) or <= c. Returns the number of points written. The
// caller guarantees room for grow(count) points.
u32 stage(const Point* in, u32 count, Point* dst, bool on_x, bool keep_ge, std::int32_t c) {
  const auto inside = [&](const Point& p) {
    const std::int32_t v = on_x ? p.x : p.y;
    return keep_ge ? v >= c : v <= c;
  };
  u32 n = 0;
  for (u32 i = 0; i < count; ++i) {
    const Point& prev = in[i == 0 ? count - 1 : i - 1];
    const Point& cur = in[i];
    const bool pin = inside(prev), cin = inside(cur);
    if (pin && cin) {
      dst[n++] = cur;
    } else if (pin) {
      dst[n++] = intersect(prev, cur, on_x, c);
    } else if (cin) {
      dst[n++] = intersect(prev, cur, on_x, c);
      dst[n++] = cur;
    }
  }
  return n;
}

}  // namespace

const char* status_text(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::NullBuffer: return "null buffer";
    case Status::BadCount: return "bad count";
    case Status::BadRect: return "bad rect";
    case Status::BadPoint: return "bad point";
    case Status::BufferTooSmall: return "buffer too small";
    case Status::Overlap: return "overlap";
  }
  return "unknown";
}

Status clip_segment(const Rect& r, const Point& p0, const Point& p1, Point* a, Point* b, bool* visible) {
  if (a == nullptr || b == nullptr || visible == nullptr) return Status::NullBuffer;
  if (rect_bad(r)) return Status::BadRect;
  if (point_bad(p0) || point_bad(p1)) return Status::BadPoint;
  Point s{0, 0}, e{0, 0};
  const bool vis = clip_segment_raw(r, p0, p1, &s, &e);
  *visible = vis;
  if (vis) {
    *a = s;
    *b = e;
  }
  return Status::Ok;
}

std::uint32_t polygon_bound(std::uint32_t point_count) {
  if (!count_ok(point_count, 3)) return 0;
  return grow(grow(grow(grow(point_count))));
}

std::uint32_t scratch_size(std::uint32_t point_count) {
  if (!count_ok(point_count, 3)) return 0;
  const u32 g1 = grow(point_count), g2 = grow(g1), g3 = grow(g2);
  return g3 + g2;
}

Status clip_polygon(const Rect& r, const Point* in, std::uint32_t point_count, Point* out, std::uint32_t out_capacity, Point* scratch,
                    std::uint32_t scratch_count, std::uint32_t* out_count) {
  if (in == nullptr || out == nullptr || scratch == nullptr || out_count == nullptr) return Status::NullBuffer;
  if (!count_ok(point_count, 3)) return Status::BadCount;
  if (rect_bad(r)) return Status::BadRect;
  for (u32 i = 0; i < point_count; ++i)
    if (point_bad(in[i])) return Status::BadPoint;
  const u32 bound = polygon_bound(point_count), sneed = scratch_size(point_count);
  if (out_capacity < bound || scratch_count < sneed) return Status::BufferTooSmall;
  const Extent ext[4] = {{in, static_cast<u64>(point_count) * sizeof(Point)},
                         {scratch, static_cast<u64>(sneed) * sizeof(Point)},
                         {out, static_cast<u64>(bound) * sizeof(Point)},
                         {out_count, sizeof(u32)}};
  if (any_overlap(ext)) return Status::Overlap;

  const u32 g1 = grow(point_count), g2 = grow(g1), g3 = grow(g2);
  (void)g1;
  Point* const first = scratch;       // stage 1 and stage 3 results, room for g3 points
  Point* const second = scratch + g3;  // stage 2 result, room for g2 points
  u32 n = stage(in, point_count, first, true, true, r.x0);
  n = stage(first, n, second, false, true, r.y0);
  n = stage(second, n, first, true, false, r.x1);
  n = stage(first, n, out, false, false, r.y1);
  *out_count = n;
  return Status::Ok;
}

Status clip_polyline(const Rect& r, const Point* in, std::uint32_t point_count, Point* pts, std::uint32_t pts_capacity,
                     std::uint32_t* piece_ends, std::uint32_t piece_capacity, std::uint32_t* piece_count, std::uint32_t* out_points) {
  if (in == nullptr || pts == nullptr || piece_ends == nullptr || piece_count == nullptr || out_points == nullptr)
    return Status::NullBuffer;
  if (!count_ok(point_count, 2)) return Status::BadCount;
  if (rect_bad(r)) return Status::BadRect;
  for (u32 i = 0; i < point_count; ++i)
    if (point_bad(in[i])) return Status::BadPoint;
  const u32 max_pts = 2 * (point_count - 1), max_pieces = point_count - 1;
  if (pts_capacity < max_pts || piece_capacity < max_pieces) return Status::BufferTooSmall;
  const Extent ext[5] = {{in, static_cast<u64>(point_count) * sizeof(Point)},
                         {pts, static_cast<u64>(max_pts) * sizeof(Point)},
                         {piece_ends, static_cast<u64>(max_pieces) * sizeof(u32)},
                         {piece_count, sizeof(u32)},
                         {out_points, sizeof(u32)}};
  if (any_overlap(ext)) return Status::Overlap;

  u32 np = 0, nc = 0;
  bool open = false;
  for (u32 i = 0; i + 1 < point_count; ++i) {
    Point a{0, 0}, b{0, 0};
    if (!clip_segment_raw(r, in[i], in[i + 1], &a, &b)) {
      if (open) piece_ends[nc++] = np;
      open = false;
      continue;
    }
    if (open && pts[np - 1].x == a.x && pts[np - 1].y == a.y) {
      pts[np++] = b;
    } else {
      if (open) piece_ends[nc++] = np;
      pts[np++] = a;
      pts[np++] = b;
      open = true;
    }
  }
  if (open) piece_ends[nc++] = np;
  *piece_count = nc;
  *out_points = np;
  return Status::Ok;
}

std::uint64_t digest_points(const Point* pts, std::size_t count) {
  u64 h = 0xcbf29ce484222325ull;
  if (pts == nullptr) return h;
  for (std::size_t i = 0; i < count; ++i) {
    for (int k = 0; k < 2; ++k) {
      u32 v = static_cast<u32>(k == 0 ? pts[i].x : pts[i].y);
      for (int b = 0; b < 4; ++b) {
        h ^= static_cast<u64>(v & 0xFFu);
        h *= 0x100000001b3ull;
        v >>= 8;
      }
    }
  }
  return h;
}

}  // namespace lumora::clip
