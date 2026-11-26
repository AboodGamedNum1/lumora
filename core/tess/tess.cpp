// Lumora CPU polygon tessellation (scope U). Integer only, no heap, no recursion, no mutable statics. See docs/tess.md.
#include "tess.h"

namespace lumora::tess {

namespace {

using i64 = std::int64_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u32 kRemoved = 0xFFFFFFFFu;  // next[] marker of a removed vertex
constexpr u32 kUnknown = 0xFFFFFFFFu;  // status: not evaluated since the neighbours last changed
constexpr u32 kEar = 0xFFFFFFFEu;      // status: strictly convex with an empty closed triangle

bool coord_bad(const Point& p) { return p.x < -kMaxCoord || p.x > kMaxCoord || p.y < -kMaxCoord || p.y > kMaxCoord; }

// Twice the signed area of the triangle (a, b, c): (b - a) x (c - a). Differences are at most 2^29, so every product is at most
// 2^58 and the difference of the two products at most 2^59: exact in int64.
i64 orient(const Point& a, const Point& b, const Point& c) {
  const i64 bx = static_cast<i64>(b.x) - a.x, by = static_cast<i64>(b.y) - a.y;
  const i64 cx = static_cast<i64>(c.x) - a.x, cy = static_cast<i64>(c.y) - a.y;
  return bx * cy - by * cx;
}

int sign64(i64 v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

// Exact shoelace sum in a two-word (128 bit, two's complement) accumulator. Each term is at most 2^57 in magnitude and there are at
// most 4096 terms, so the sum is below 2^69 in magnitude and never wraps the 128 bits.
struct Wide {
  u64 lo;
  u64 hi;
};

Wide shoelace(const Point* pts, u32 count) {
  Wide w{0, 0};
  for (u32 i = 0; i < count; ++i) {
    const Point& p = pts[i];
    const Point& q = pts[i + 1 == count ? 0 : i + 1];
    const i64 term = static_cast<i64>(p.x) * q.y - static_cast<i64>(q.x) * p.y;  // |x|,|y| <= 2^28: at most 2^57
    const u64 t = static_cast<u64>(term);
    const u64 old = w.lo;
    w.lo += t;
    const u64 carry = w.lo < old ? 1 : 0;
    w.hi += (term < 0 ? ~static_cast<u64>(0) : 0) + carry;  // sign extension plus carry, modulo 2^64
  }
  return w;
}

int wide_sign(const Wide& w) {
  if ((w.hi >> 63) != 0) return -1;
  return (w.hi == 0 && w.lo == 0) ? 0 : 1;
}

i64 wide_saturate(const Wide& w) {
  const bool neg = (w.hi >> 63) != 0;
  const u64 ext = neg ? ~static_cast<u64>(0) : 0;
  const bool fits = w.hi == ext && ((w.lo >> 63) != 0) == neg;
  if (fits) return static_cast<i64>(w.lo);  // two's complement reinterpretation, exact
  return neg ? INT64_MIN : INT64_MAX;
}

bool on_segment_box(const Point& a, const Point& b, const Point& p) {
  const std::int32_t lox = a.x < b.x ? a.x : b.x, hix = a.x < b.x ? b.x : a.x;
  const std::int32_t loy = a.y < b.y ? a.y : b.y, hiy = a.y < b.y ? b.y : a.y;
  return p.x >= lox && p.x <= hix && p.y >= loy && p.y <= hiy;
}

// Closed segments p1p2 and p3p4 share at least one point (touching, crossing and overlap all count).
bool segments_meet(const Point& p1, const Point& p2, const Point& p3, const Point& p4) {
  // Exact quick reject on the bounding boxes.
  const std::int32_t ax0 = p1.x < p2.x ? p1.x : p2.x, ax1 = p1.x < p2.x ? p2.x : p1.x;
  const std::int32_t bx0 = p3.x < p4.x ? p3.x : p4.x, bx1 = p3.x < p4.x ? p4.x : p3.x;
  if (ax1 < bx0 || bx1 < ax0) return false;
  const std::int32_t ay0 = p1.y < p2.y ? p1.y : p2.y, ay1 = p1.y < p2.y ? p2.y : p1.y;
  const std::int32_t by0 = p3.y < p4.y ? p3.y : p4.y, by1 = p3.y < p4.y ? p4.y : p3.y;
  if (ay1 < by0 || by1 < ay0) return false;
  const int d1 = sign64(orient(p3, p4, p1)), d2 = sign64(orient(p3, p4, p2));
  const int d3 = sign64(orient(p1, p2, p3)), d4 = sign64(orient(p1, p2, p4));
  if (d1 * d2 < 0 && d3 * d4 < 0) return true;
  if (d1 == 0 && on_segment_box(p3, p4, p1)) return true;
  if (d2 == 0 && on_segment_box(p3, p4, p2)) return true;
  if (d3 == 0 && on_segment_box(p1, p2, p3)) return true;
  if (d4 == 0 && on_segment_box(p1, p2, p4)) return true;
  return false;
}

// Overlap of two touched byte ranges. Same modular address rule as core/composite, core/resample and core/sdf: clear the top byte,
// take the unsigned difference modulo 2^64, answer "no overlap" at |delta| >= 2^47 before any other arithmetic, then compare
// the exact ranges. Both lengths are positive.
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

// Point q inside or on the boundary of the triangle (a, v, b) whose orientation sign is s (+1 or -1): no edge has q strictly on
// the outer side.
bool in_closed_triangle(const Point& a, const Point& v, const Point& b, int s, const Point& q) {
  return sign64(orient(a, v, q)) * s >= 0 && sign64(orient(v, b, q)) * s >= 0 && sign64(orient(b, a, q)) * s >= 0;
}

bool in_open_triangle(const Point& a, const Point& v, const Point& b, int s, const Point& q) {
  return sign64(orient(a, v, q)) * s > 0 && sign64(orient(v, b, q)) * s > 0 && sign64(orient(b, a, q)) * s > 0;
}

}  // namespace

const char* status_text(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::NullBuffer: return "null buffer";
    case Status::BadCount: return "bad count";
    case Status::BadPoint: return "bad point";
    case Status::BadPolygon: return "bad polygon";
    case Status::BufferTooSmall: return "buffer too small";
    case Status::Overlap: return "overlap";
  }
  return "unknown";
}

Status area2(const Point* pts, std::uint32_t count, std::int64_t* out_area2) {
  if (pts == nullptr || out_area2 == nullptr) return Status::NullBuffer;
  if (count < 3 || count > kMaxVertices) return Status::BadCount;
  for (u32 i = 0; i < count; ++i)
    if (coord_bad(pts[i])) return Status::BadPoint;
  *out_area2 = wide_saturate(shoelace(pts, count));
  return Status::Ok;
}

namespace {

// is_simple after the argument checks.
bool simple_checked(const Point* pts, u32 n) {
  if (wide_sign(shoelace(pts, n)) == 0) return false;
  for (u32 i = 0; i < n; ++i)
    for (u32 j = i + 1; j < n; ++j)
      if (pts[i].x == pts[j].x && pts[i].y == pts[j].y) return false;
  // Adjacent edges share only their common vertex: a fold back onto the previous edge (collinear, same side) is not simple.
  for (u32 i = 0; i < n; ++i) {
    const Point& p = pts[i];
    const Point& q = pts[i + 1 == n ? 0 : i + 1];
    const Point& r = pts[i + 2 >= n ? i + 2 - n : i + 2];
    if (orient(p, q, r) == 0) {
      const i64 dot = (static_cast<i64>(p.x) - q.x) * (static_cast<i64>(r.x) - q.x) +
                      (static_cast<i64>(p.y) - q.y) * (static_cast<i64>(r.y) - q.y);  // each product <= 2^58
      if (dot > 0) return false;
    }
  }
  // Non-adjacent edges share no point.
  for (u32 i = 0; i < n; ++i) {
    const Point& p1 = pts[i];
    const Point& p2 = pts[i + 1 == n ? 0 : i + 1];
    for (u32 j = i + 2; j < n; ++j) {
      if (i == 0 && j == n - 1) continue;  // adjacent through the closing edge
      const Point& p3 = pts[j];
      const Point& p4 = pts[j + 1 == n ? 0 : j + 1];
      if (segments_meet(p1, p2, p3, p4)) return false;
    }
  }
  return true;
}

}  // namespace

Status is_simple(const Point* pts, std::uint32_t count, bool* simple) {
  if (pts == nullptr || simple == nullptr) return Status::NullBuffer;
  if (count < 3 || count > kMaxVertices) return Status::BadCount;
  for (u32 i = 0; i < count; ++i)
    if (coord_bad(pts[i])) return Status::BadPoint;
  *simple = simple_checked(pts, count);
  return Status::Ok;
}

Status locate(const Point* pts, std::uint32_t count, std::int32_t px, std::int32_t py, Location* where) {
  if (pts == nullptr || where == nullptr) return Status::NullBuffer;
  if (count < 3 || count > kMaxVertices) return Status::BadCount;
  for (u32 i = 0; i < count; ++i)
    if (coord_bad(pts[i])) return Status::BadPoint;
  const Point q{px, py};
  bool inside = false;
  for (u32 i = 0; i < count; ++i) {
    const Point& a = pts[i];
    const Point& b = pts[i + 1 == count ? 0 : i + 1];
    // |b - a| <= 2^29 and |q - a| <= 2^31 + 2^28, so each product is below 2^61 and the cross value below 2^62: exact.
    const i64 c = orient(a, b, q);
    if (c == 0 && on_segment_box(a, b, q)) {
      *where = Location::Boundary;
      return Status::Ok;
    }
    // Crossing number with a ray to +x and the half-open rule on y: an edge counts when exactly one end is above py.
    if ((a.y > py) != (b.y > py)) {
      const bool upward = b.y > a.y;
      if (upward ? c > 0 : c < 0) inside = !inside;  // the crossing lies to the right of q
    }
  }
  *where = inside ? Location::Inside : Location::Outside;
  return Status::Ok;
}

std::uint32_t scratch_size(std::uint32_t count) {
  if (count < 3 || count > kMaxVertices) return 0;
  return 3 * count;
}

Status triangulate(const Point* pts, std::uint32_t count, std::uint32_t* out, std::uint32_t out_capacity, std::uint32_t* scratch,
                   std::uint32_t scratch_count, std::uint32_t* tri_count) {
  if (pts == nullptr || out == nullptr || scratch == nullptr || tri_count == nullptr) return Status::NullBuffer;
  if (count < 3 || count > kMaxVertices) return Status::BadCount;
  for (u32 i = 0; i < count; ++i)
    if (coord_bad(pts[i])) return Status::BadPoint;
  const u32 n = count;
  if (out_capacity < n - 2 || scratch_count < scratch_size(n)) return Status::BufferTooSmall;

  // Touched ranges: the points, the scratch words, the output extent 3 * min(out_capacity, n - 2) words, and *tri_count.
  const u32 tris_max = out_capacity < n - 2 ? out_capacity : n - 2;
  const u64 pts_bytes = static_cast<u64>(n) * sizeof(Point);
  const u64 scratch_bytes = static_cast<u64>(scratch_size(n)) * 4u;
  const u64 out_bytes = static_cast<u64>(tris_max) * 12u;
  if (ranges_overlap(pts, pts_bytes, scratch, scratch_bytes) || ranges_overlap(pts, pts_bytes, out, out_bytes) ||
      ranges_overlap(scratch, scratch_bytes, out, out_bytes) || ranges_overlap(tri_count, 4, pts, pts_bytes) ||
      ranges_overlap(tri_count, 4, scratch, scratch_bytes) || ranges_overlap(tri_count, 4, out, out_bytes))
    return Status::Overlap;

  if (!simple_checked(pts, n)) return Status::BadPolygon;  // also covers zero area, so the sign below is +1 or -1
  // From here on nothing can fail for a simple polygon (see docs/tess.md); the defensive returns below are never taken.
  const int s = wide_sign(shoelace(pts, n));  // +1 counter-clockwise, -1 clockwise

  u32* prev = scratch;
  u32* next = scratch + n;
  u32* st = scratch + 2 * n;
  for (u32 i = 0; i < n; ++i) {
    prev[i] = i == 0 ? n - 1 : i - 1;
    next[i] = i + 1 == n ? 0 : i + 1;
    st[i] = kUnknown;
  }
  u32 remaining = n;
  u32 lo = 0;
  u32 tris = 0;

  while (remaining >= 3) {
    while (next[lo] == kRemoved) ++lo;  // the lowest remaining index only moves up
    bool dropped = false, clipped = false;
    u32 cur = lo;
    const u32 steps = remaining;
    for (u32 k = 0; k < steps && !clipped; ++k) {
      const u32 v = cur, a = prev[v], b = next[v];
      const int turn = sign64(orient(pts[a], pts[v], pts[b]));
      if (turn == 0) {  // collinear: drop without a triangle and continue at the successor
        if (remaining <= 3) return Status::BadPolygon;  // unreachable: the last three vertices span a positive area
        next[a] = b;
        prev[b] = a;
        next[v] = kRemoved;
        st[a] = kUnknown;
        st[b] = kUnknown;
        --remaining;
        dropped = true;
        cur = b;
        continue;
      }
      bool ear = false;
      if (turn == s) {
        if (st[v] == kEar) {
          ear = true;
        } else if (st[v] != kUnknown && next[st[v]] != kRemoved) {
          ear = false;  // the remembered blocking vertex is still there and the triangle is unchanged
        } else {
          ear = true;
          for (u32 u = next[b]; u != a; u = next[u]) {
            if (in_closed_triangle(pts[a], pts[v], pts[b], s, pts[u])) {
              st[v] = u;
              ear = false;
              break;
            }
          }
          if (ear) st[v] = kEar;
        }
      }
      if (ear) {
        out[3 * tris + 0] = s > 0 ? a : b;
        out[3 * tris + 1] = v;
        out[3 * tris + 2] = s > 0 ? b : a;
        ++tris;
        next[a] = b;
        prev[b] = a;
        next[v] = kRemoved;
        st[a] = kUnknown;
        st[b] = kUnknown;
        --remaining;
        clipped = true;
      } else {
        cur = b;
      }
    }
    if (clipped || dropped) continue;

    // Fallback: a full pass found neither a collinear vertex nor an ear. Clip the first strictly convex vertex whose triangle has
    // no remaining vertex strictly inside (boundary allowed).
    bool done = false;
    u32 v = lo;
    for (u32 k = 0; k < remaining && !done; ++k, v = next[v]) {
      const u32 a = prev[v], b = next[v];
      if (sign64(orient(pts[a], pts[v], pts[b])) != s) continue;
      bool empty = true;
      for (u32 u = next[b]; u != a; u = next[u]) {
        if (in_open_triangle(pts[a], pts[v], pts[b], s, pts[u])) {
          empty = false;
          break;
        }
      }
      if (!empty) continue;
      out[3 * tris + 0] = s > 0 ? a : b;
      out[3 * tris + 1] = v;
      out[3 * tris + 2] = s > 0 ? b : a;
      ++tris;
      next[a] = b;
      prev[b] = a;
      next[v] = kRemoved;
      st[a] = kUnknown;
      st[b] = kUnknown;
      --remaining;
      done = true;
    }
    if (!done) return Status::BadPolygon;  // unreachable, see docs/tess.md
  }
  *tri_count = tris;
  return Status::Ok;
}

std::uint64_t digest_indices(const std::uint32_t* values, std::size_t count) {
  u64 h = 0xcbf29ce484222325ull;
  if (values == nullptr) return h;
  for (std::size_t i = 0; i < count; ++i) {
    u32 v = values[i];
    for (int b = 0; b < 4; ++b) {
      h ^= static_cast<u64>(v & 0xFFu);
      h *= 0x100000001b3ull;
      v >>= 8;
    }
  }
  return h;
}

}  // namespace lumora::tess
