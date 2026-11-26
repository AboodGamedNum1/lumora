// Lumora CPU polyline stroker (scope W). Integer only, no heap, no recursion, no mutable statics. See docs/stroke.md.
#include "stroke.h"

namespace lumora::stroke {

namespace {

using i64 = std::int64_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

bool coord_bad(std::int32_t v) { return v < -kMaxCoord || v > kMaxCoord; }

bool count_ok(u32 n) { return n >= 2 && n <= kMaxPoints; }

struct V {
  i64 x, y;
};

V operator+(const V& a, const V& b) { return {a.x + b.x, a.y + b.y}; }
V operator-(const V& a, const V& b) { return {a.x - b.x, a.y - b.y}; }
V neg(const V& a) { return {-a.x, -a.y}; }
V to_v(const Point& p) { return {p.x, p.y}; }

// Twice the signed area of the triangle (a, b, c). All uses have coordinate differences below 2^30 or exactly the segment
// differences (below 2^29), so every product is below 2^60 and the difference below 2^61.
i64 orient(const V& a, const V& b, const V& c) { return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x); }

i64 cross2(const V& u, const V& v) { return u.x * v.y - u.y * v.x; }

// Nearest integer of num / den for den > 0, ties away from zero: sign(num) * floor((2 * |num| + den) / (2 * den)). The uses
// have |num| <= 2^59 and den <= 2^41, so 2 * |num| + den fits int64.
i64 round_away(i64 num, i64 den) {
  const i64 a = num < 0 ? -num : num;
  const i64 q = (2 * a + den) / (2 * den);
  return num < 0 ? -q : q;
}

// Overlap of two touched byte ranges. Same modular address rule as core/composite, core/resample, core/sdf, core/tess and core/clip:
// clear the top byte, take the unsigned difference modulo 2^64, answer "no overlap" at |delta| >= 2^47 before any other
// arithmetic, then compare the exact ranges. Both lengths are positive.
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

// The collapsed point list (consecutive equal points merged, for a closed line also the last against the first), accessed without
// storage: element j is the input point at the start of the j-th run of equal consecutive points. at() keeps a forward cursor, so
// ascending access costs O(n) in total, and a smaller index restarts from the beginning.
struct Collapsed {
  const Point* in;
  u32 n;
  u32 k;       // collapsed point count
  u32 cur_j;   // cursor: collapsed index
  u32 cur_i;   // cursor: input index of that run start

  Collapsed(const Point* p, u32 count, bool closed) : in(p), n(count), k(0), cur_j(0), cur_i(0) {
    u32 runs = 0, last_start = 0;
    for (u32 i = 0; i < n; ++i) {
      if (i == 0 || !same(in[i], in[i - 1])) {
        ++runs;
        last_start = i;
      }
    }
    k = runs;
    if (closed && runs > 1 && same(in[last_start], in[0])) k = runs - 1;
  }
  static bool same(const Point& a, const Point& b) { return a.x == b.x && a.y == b.y; }
  Point at(u32 j) {
    if (j < cur_j) {
      cur_j = 0;
      cur_i = 0;
    }
    while (cur_j < j) {
      u32 i = cur_i + 1;
      while (i < n && same(in[i], in[i - 1])) ++i;
      cur_i = i;
      ++cur_j;
    }
    return in[cur_i];
  }
};

// Left normal of the segment p -> q: (round(-dy * h / L), round(dx * h / L)). |dy * h| <= 2^29 * 2^19 = 2^48.
V normal_of(const Point& p, const Point& q, i64 h) {
  const i64 dx = static_cast<i64>(q.x) - p.x, dy = static_cast<i64>(q.y) - p.y;
  const i64 len = static_cast<i64>(isqrt_nearest(static_cast<u64>(dx * dx + dy * dy)));
  return {round_away(-dy * h, len), round_away(dx * h, len)};
}

struct Out {
  Point* pts;
  u32* ends;
  u32 np = 0, nc = 0;
  void put(const V& v) { pts[np++] = Point{static_cast<std::int32_t>(v.x), static_cast<std::int32_t>(v.y)}; }
  void close() { ends[nc++] = np; }
  void tri(const V& a, const V& b, const V& c) {
    put(a);
    put(b);
    put(c);
    close();
  }
  void quad(const V& a, const V& b, const V& c, const V& d) {
    put(a);
    put(b);
    put(c);
    put(d);
    close();
  }
};

// Strictly convex counter-clockwise: all four cyclic turns are positive.
bool strictly_convex4(const V& a, const V& b, const V& c, const V& d) {
  return orient(a, b, c) > 0 && orient(b, c, d) > 0 && orient(c, d, a) > 0 && orient(d, a, b) > 0;
}

void emit_join(Out& out, const Point& pp, const V& n1, const V& n2, i64 cross, const Style& st) {
  const i64 h = st.half_width;
  const V p = to_v(pp);
  const V o1 = cross > 0 ? neg(n1) : n1, o2 = cross > 0 ? neg(n2) : n2;
  const V a = p + o1, b = p + o2;
  if (st.join == Join::Miter) {
    const V m = o1 + o2;  // each coordinate is at most 2^20 in magnitude
    const i64 den = m.x * m.x + m.y * m.y;  // at most 2^41
    if (den != 0) {
      const i64 tx = round_away(m.x * 2 * h * h, den), ty = round_away(m.y * 2 * h * h, den);  // |numerator| <= 2^59
      const i64 lim = static_cast<i64>(st.miter_limit) * h;                                     // at most 2^25
      const bool coord_ok = tx >= -lim && tx <= lim && ty >= -lim && ty <= lim;
      if (coord_ok && tx * tx + ty * ty <= lim * lim) {
        const V t = p + V{tx, ty};
        // doubled signed area of the quad (p, a, t, b): cross(a - p, t - p) + cross(t - p, b - p)
        const bool flip = cross2(a - p, t - p) + cross2(t - p, b - p) < 0;
        if (!flip) {
          if (strictly_convex4(p, a, t, b)) {
            out.quad(p, a, t, b);
            return;
          }
        } else if (strictly_convex4(p, b, t, a)) {
          out.quad(p, b, t, a);
          return;
        }
      }
    }
  }
  if (orient(p, a, b) < 0) out.tri(p, b, a);
  else out.tri(p, a, b);
}

}  // namespace

const char* status_text(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::NullBuffer: return "null buffer";
    case Status::BadCount: return "bad count";
    case Status::BadStyle: return "bad style";
    case Status::BadPoint: return "bad point";
    case Status::BufferTooSmall: return "buffer too small";
    case Status::Overlap: return "overlap";
  }
  return "unknown";
}

std::uint32_t contour_bound(std::uint32_t point_count, bool closed) {
  if (!count_ok(point_count)) return 0;
  return closed ? 2 * point_count : 2 * point_count - 1;
}

std::uint32_t point_bound(std::uint32_t point_count, bool closed) { return 4 * contour_bound(point_count, closed); }

std::uint32_t isqrt_nearest(std::uint64_t d2) {
  if (d2 == 0) return 0;
  // floor square root by the bitwise method (no division, no floating point)
  u64 rem = d2, root = 0, bit = static_cast<u64>(1) << 62;
  while (bit > rem) bit >>= 2;
  while (bit != 0) {
    if (rem >= root + bit) {
      rem -= root + bit;
      root = (root >> 1) + bit;
    } else {
      root >>= 1;
    }
    bit >>= 2;
  }
  // d2 lies in [s^2, s^2 + 2 * s]; the nearest integer is s when d2 <= s^2 + s (no ties exist), else s + 1.
  const bool up = rem > root;  // rem = d2 - s^2
  if (up && root == 0xFFFFFFFFull) return 0xFFFFFFFFu;  // beyond the contract (d2 > 2^64 - 2^32): saturate
  return static_cast<u32>(root + (up ? 1 : 0));
}

Status stroke_polyline(const Point* in, std::uint32_t point_count, const Style& style, Point* pts, std::uint32_t pts_capacity,
                       std::uint32_t* contour_ends, std::uint32_t ends_capacity, std::uint32_t* contour_count,
                       std::uint32_t* point_count_out) {
  if (in == nullptr || pts == nullptr || contour_ends == nullptr || contour_count == nullptr || point_count_out == nullptr)
    return Status::NullBuffer;
  if (!count_ok(point_count)) return Status::BadCount;
  const bool bad_join = static_cast<std::uint8_t>(style.join) > static_cast<std::uint8_t>(Join::Miter);
  const bool bad_cap = static_cast<std::uint8_t>(style.cap) > static_cast<std::uint8_t>(Cap::Square);
  if (style.half_width < 1 || style.half_width > kMaxHalfWidth || style.miter_limit < 1 || style.miter_limit > kMaxMiterLimit ||
      bad_join || bad_cap)
    return Status::BadStyle;
  for (u32 i = 0; i < point_count; ++i)
    if (coord_bad(in[i].x) || coord_bad(in[i].y)) return Status::BadPoint;
  const u32 cb = contour_bound(point_count, style.closed), pb = point_bound(point_count, style.closed);
  if (pts_capacity < pb || ends_capacity < cb) return Status::BufferTooSmall;
  const Extent ext[5] = {{in, static_cast<u64>(point_count) * sizeof(Point)},
                         {pts, static_cast<u64>(pb) * sizeof(Point)},
                         {contour_ends, static_cast<u64>(cb) * sizeof(u32)},
                         {contour_count, sizeof(u32)},
                         {point_count_out, sizeof(u32)}};
  if (any_overlap(ext)) return Status::Overlap;

  Out out{pts, contour_ends};
  const i64 h = style.half_width;
  Collapsed c(in, point_count, style.closed);
  const u32 k = c.k;
  if (k < 2) {
    if (style.cap == Cap::Square) {
      const V p = to_v(in[0]);
      out.quad({p.x - h, p.y - h}, {p.x + h, p.y - h}, {p.x + h, p.y + h}, {p.x - h, p.y + h});
    }
    *contour_count = out.nc;
    *point_count_out = out.np;
    return Status::Ok;
  }
  const bool closed = style.closed;
  const u32 segs = closed ? k : k - 1;

  // 1. one quad per segment (p - n, q - n, q + n, p + n)
  {
    Collapsed s(in, point_count, closed);
    Point p = s.at(0);
    for (u32 i = 0; i < segs; ++i) {
      const Point q = (i + 1 < k) ? s.at(i + 1) : in[0];  // the closing segment ends at the first point
      const V n = normal_of(p, q, h);
      const V pv = to_v(p), qv = to_v(q);
      out.quad(pv - n, qv - n, qv + n, pv + n);
      p = q;
    }
  }

  // 2. joins in point order
  {
    Collapsed s(in, point_count, closed);
    const u32 first = closed ? 0 : 1, last = closed ? k - 1 : k - 2;  // inclusive
    Point prev = s.at(first == 0 ? k - 1 : first - 1);
    Point cur = s.at(first);
    Point next = s.at(first + 1 < k ? first + 1 : 0);
    for (u32 j = first; j <= last; ++j) {
      const i64 dx1 = static_cast<i64>(cur.x) - prev.x, dy1 = static_cast<i64>(cur.y) - prev.y;
      const i64 dx2 = static_cast<i64>(next.x) - cur.x, dy2 = static_cast<i64>(next.y) - cur.y;
      const i64 cross = dx1 * dy2 - dy1 * dx2;  // at most 2^59
      if (cross != 0) emit_join(out, cur, normal_of(prev, cur, h), normal_of(cur, next, h), cross, style);
      prev = cur;
      cur = next;
      if (j < last) next = s.at(j + 2 < k ? j + 2 : (j + 2) % k);
    }
  }

  // 3. square caps of an open line: the quads are counter-clockwise by construction (see docs/stroke.md)
  if (!closed && style.cap == Cap::Square) {
    Collapsed s(in, point_count, closed);
    const Point p0 = s.at(0), p1 = s.at(1);
    const V n0 = normal_of(p0, p1, h);
    const V d0{n0.y, -n0.x};
    const V a = to_v(p0);
    out.quad(a - n0, a + n0, a + n0 - d0, a - n0 - d0);
    const Point pa = s.at(k - 2), pe = s.at(k - 1);
    const V n1 = normal_of(pa, pe, h);
    const V d1{n1.y, -n1.x};
    const V e = to_v(pe);
    out.quad(e + n1, e - n1, e - n1 + d1, e + n1 + d1);
  }

  *contour_count = out.nc;
  *point_count_out = out.np;
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

}  // namespace lumora::stroke
