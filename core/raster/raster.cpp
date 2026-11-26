// Deterministic integer polygon rasterizer. Rules, rounding, operation order, bounds and golden digests: docs/raster.md.
// Determinism: integer arithmetic only (64-bit intermediates), no floats, no libm, no unordered containers, no pointer values
// in any decision.
#include "raster.h"

#include <vector>

namespace lumora::raster {
namespace {

using i64 = std::int64_t;

// floor(a / b) for b > 0, correct for negative a.
i64 floor_div(i64 a, i64 b) {
  i64 q = a / b;
  if ((a % b != 0) && (a < 0)) --q;
  return q;
}

// Round to nearest, ties toward +infinity: floor((2n + d) / (2d)), d > 0.
// This is the single snapping rule of the rasterizer (docs/raster.md).
i64 round_nearest(i64 n, i64 d) { return floor_div(2 * n + d, 2 * d); }

// One edge, normalized so that y0 < y1. `sign` is +1 when the contour edge went down (y increasing), -1 when it went up.
struct Edge {
  i64 x0, y0, x1, y1;
  i64 sign;
};

// x of the edge at the row boundary Y (y0 <= Y <= y1), snapped to the 1/256 grid.
i64 edge_x_at(const Edge& e, i64 Y) { return e.x0 + round_nearest((Y - e.y0) * (e.x1 - e.x0), e.y1 - e.y0); }

struct Band {
  i64 row0, rows;       // first row of the band and its row count
  int width;            // target width
  std::vector<i64>* cov;
  std::vector<i64>* area;  // doubled area: 2 * A, in units of 1/131072 pixel^2
};

// Adds one piece that lies inside one cell column. Offsets are in 1/256 of the cell, 0..256. cover is signed.
void add_piece(const Band& b, i64 row, i64 col, i64 lp, i64 lq, i64 cover) {
  const std::size_t idx = static_cast<std::size_t>(row - b.row0) * static_cast<std::size_t>(b.width) + static_cast<std::size_t>(col);
  (*b.cov)[idx] += cover;
  (*b.area)[idx] += cover * (512 - lp - lq);
}

// Walks the snapped sub-segment (xa,ya)-(xb,yb) of one row (ya < yb), left part clipped to column 0, right part dropped.
void add_row_segment(const Band& b, i64 row, i64 xa, i64 ya, i64 xb, i64 yb, i64 sign) {
  const i64 W = b.width;
  if (xa == xb) {  // vertical
    i64 c = floor_div(xa, 256);
    if (c >= W) return;
    i64 lp = 0;
    if (c < 0) c = 0;
    else lp = xa - c * 256;
    add_piece(b, row, c, lp, lp, sign * (yb - ya));
    return;
  }
  // Order left to right; the cover of a piece is sign * |dy| whatever the walking direction.
  i64 xl = xa, yl = ya, xr = xb, yr = yb;
  if (xl > xr) {
    xl = xb;
    yl = yb;
    xr = xa;
    yr = ya;
  }
  const i64 dx = xr - xl, dyv = yr - yl;
  auto y_at = [&](i64 X) { return yl + round_nearest((X - xl) * dyv, dx); };
  i64 xp = xl, yp = yl;
  if (xp < 0) {
    // Everything left of x = 0 acts as a vertical edge at x = 0 with the total cover of that part.
    const i64 xq = xr < 0 ? xr : 0;
    const i64 yq = xr <= 0 ? yr : y_at(0);
    const i64 d = yq > yp ? yq - yp : yp - yq;
    if (d != 0) add_piece(b, row, 0, 0, 0, sign * d);
    xp = xq;
    yp = yq;
    if (xp >= xr) return;
  }
  // From here on xp >= 0 and xp < xr.
  i64 c = xp / 256;
  while (c < W) {
    const i64 line = (c + 1) * 256;
    i64 xq, yq;
    if (line < xr) {
      xq = line;
      yq = y_at(line);
    } else {
      xq = xr;
      yq = yr;
    }
    const i64 d = yq > yp ? yq - yp : yp - yq;
    if (d != 0) add_piece(b, row, c, xp - c * 256, xq - c * 256, sign * d);
    if (xq >= xr) return;
    xp = xq;
    yp = yq;
    ++c;
  }
}

void add_edge(const Band& b, const P& pa, const P& pb) {
  if (pa.y == pb.y) return;
  Edge e;
  if (pa.y < pb.y) {
    e = Edge{pa.x, pa.y, pb.x, pb.y, 1};
  } else {
    e = Edge{pb.x, pb.y, pa.x, pa.y, -1};
  }
  i64 j0 = floor_div(e.y0, 256);
  i64 j1 = floor_div(e.y1 - 1, 256);
  if (j0 < b.row0) j0 = b.row0;
  if (j1 > b.row0 + b.rows - 1) j1 = b.row0 + b.rows - 1;
  for (i64 j = j0; j <= j1; ++j) {
    const i64 ya = e.y0 > j * 256 ? e.y0 : j * 256;
    const i64 yb = e.y1 < (j + 1) * 256 ? e.y1 : (j + 1) * 256;
    const i64 xa = ya == e.y0 ? e.x0 : edge_x_at(e, ya);
    const i64 xb = yb == e.y1 ? e.x1 : edge_x_at(e, yb);
    add_row_segment(b, j, xa, ya, xb, yb, e.sign);
  }
}

std::uint8_t fold(i64 a2, Fill rule) {
  std::uint64_t a = static_cast<std::uint64_t>(a2 < 0 ? -a2 : a2);
  std::uint64_t v2;
  if (rule == Fill::NonZero) {
    v2 = a < 131072u ? a : 131072u;
  } else {
    const std::uint64_t m = a % 262144u;
    v2 = m <= 131072u ? m : 262144u - m;
  }
  return static_cast<std::uint8_t>((v2 * 255u + 65536u) >> 17);
}

bool coord_ok(Fx v) { return v >= -kMaxCoord && v <= kMaxCoord; }

constexpr std::size_t kBandCells = static_cast<std::size_t>(1) << 20;  // cells per band: 2 arrays of 8 MiB

// ---- flattening ----
i64 sq(i64 v) { return v * v; }

// Smallest k in 0..6 with sqrt(dev2) <= 4 * tol * 4^k, where dev2 is the squared length scaled for the curve (see docs).
int pick_depth(i64 dev2, Fx tol) {
  i64 t = tol < 1 ? 1 : tol;
  if (t > (static_cast<i64>(1) << 28)) t = static_cast<i64>(1) << 28;
  for (int k = 0; k <= 6; ++k) {
    const i64 lim = 4 * t * (static_cast<i64>(1) << (2 * k));
    if (lim >= (static_cast<i64>(1) << 28)) return k;  // the test passes without computing it
    if (dev2 <= lim * lim) return k;
  }
  return 6;
}

P mid(const P& a, const P& b) {
  return P{static_cast<Fx>((static_cast<i64>(a.x) + b.x + 1) >> 1), static_cast<Fx>((static_cast<i64>(a.y) + b.y + 1) >> 1)};
}

void sub_quad(const P& p0, const P& p1, const P& p2, int k, P* out, std::size_t& n) {
  if (k == 0) {
    out[n++] = p2;
    return;
  }
  const P a = mid(p0, p1), c = mid(p1, p2), m = mid(a, c);
  sub_quad(p0, a, m, k - 1, out, n);
  sub_quad(m, c, p2, k - 1, out, n);
}

void sub_cubic(const P& p0, const P& p1, const P& p2, const P& p3, int k, P* out, std::size_t& n) {
  if (k == 0) {
    out[n++] = p3;
    return;
  }
  const P a = mid(p0, p1), b = mid(p1, p2), c = mid(p2, p3), d = mid(a, b), e = mid(b, c), m = mid(d, e);
  sub_cubic(p0, a, d, m, k - 1, out, n);
  sub_cubic(m, e, c, p3, k - 1, out, n);
}

bool pts_ok(const P* p, int n) {
  for (int i = 0; i < n; ++i) {
    if (!coord_ok(p[i].x) || !coord_ok(p[i].y)) return false;
  }
  return true;
}

}  // namespace

const char* raster_status_text(RasterStatus s) {
  switch (s) {
    case RasterStatus::Ok: return "ok";
    case RasterStatus::NullBuffer: return "null buffer";
    case RasterStatus::BadSize: return "bad size";
    case RasterStatus::BadFill: return "bad fill rule";
    case RasterStatus::BadContour: return "bad contour";
    case RasterStatus::BadPoint: return "bad point";
  }
  return "unknown raster status";
}

RasterStatus fill_polygons(const Contour* contours, std::size_t contour_count, Fill rule, const Target& target) {
  // 1. NullBuffer
  if (target.data == nullptr) return RasterStatus::NullBuffer;
  if (contours == nullptr && contour_count > 0) return RasterStatus::NullBuffer;
  if (contour_count <= kMaxContours) {  // beyond the limit the count itself is the error (BadContour below); no pointer is read
    for (std::size_t i = 0; i < contour_count; ++i) {
      if (contours[i].count > 0 && contours[i].pts == nullptr) return RasterStatus::NullBuffer;
    }
  }
  // 2. BadSize
  if (target.width == 0 || target.height == 0 || target.width > kMaxSize || target.height > kMaxSize || target.stride < target.width)
    return RasterStatus::BadSize;
  // 3. BadFill
  if (rule != Fill::NonZero && rule != Fill::EvenOdd) return RasterStatus::BadFill;
  // 4. BadContour
  if (contour_count > kMaxContours) return RasterStatus::BadContour;
  for (std::size_t i = 0; i < contour_count; ++i) {
    if (contours[i].count < 3 || contours[i].count > kMaxPointsPerContour) return RasterStatus::BadContour;
  }
  // 5. BadPoint
  for (std::size_t i = 0; i < contour_count; ++i) {
    for (std::size_t k = 0; k < contours[i].count; ++k) {
      if (!coord_ok(contours[i].pts[k].x) || !coord_ok(contours[i].pts[k].y)) return RasterStatus::BadPoint;
    }
  }

  const std::size_t W = target.width, H = target.height;
  std::size_t band_rows = kBandCells / W;
  if (band_rows < 1) band_rows = 1;
  if (band_rows > H) band_rows = H;
  std::vector<i64> cov(band_rows * W), area(band_rows * W);
  for (std::size_t r0 = 0; r0 < H; r0 += band_rows) {
    const std::size_t rows = (H - r0 < band_rows) ? H - r0 : band_rows;
    for (std::size_t i = 0; i < rows * W; ++i) {
      cov[i] = 0;
      area[i] = 0;
    }
    if (contour_count > 0) {
      const Band b{static_cast<i64>(r0), static_cast<i64>(rows), static_cast<int>(W), &cov, &area};
      for (std::size_t ci = 0; ci < contour_count; ++ci) {
        const P* p = contours[ci].pts;
        const std::size_t n = contours[ci].count;
        for (std::size_t k = 0; k < n; ++k) add_edge(b, p[k], p[k + 1 == n ? 0 : k + 1]);
      }
    }
    for (std::size_t r = 0; r < rows; ++r) {
      std::uint8_t* row = target.data + (r0 + r) * static_cast<std::size_t>(target.stride);
      i64 run = 0;
      for (std::size_t x = 0; x < W; ++x) {
        const std::size_t idx = r * W + x;
        row[x] = fold(area[idx] + 512 * run, rule);
        run += cov[idx];
      }
    }
  }
  return RasterStatus::Ok;
}

std::size_t flatten_quad(const P& p0, const P& p1, const P& p2, Fx tol, P* out, std::size_t cap) {
  const P in[3] = {p0, p1, p2};
  if (!pts_ok(in, 3) || out == nullptr) return 0;
  const i64 sx = static_cast<i64>(p0.x) - 2 * static_cast<i64>(p1.x) + p2.x;
  const i64 sy = static_cast<i64>(p0.y) - 2 * static_cast<i64>(p1.y) + p2.y;
  const int k = pick_depth(sq(sx) + sq(sy), tol);
  const std::size_t count = static_cast<std::size_t>(1) << k;
  if (cap < count) return 0;
  std::size_t n = 0;
  sub_quad(p0, p1, p2, k, out, n);
  return n;
}

std::size_t flatten_cubic(const P& p0, const P& p1, const P& p2, const P& p3, Fx tol, P* out, std::size_t cap) {
  const P in[4] = {p0, p1, p2, p3};
  if (!pts_ok(in, 4) || out == nullptr) return 0;
  const i64 ax = static_cast<i64>(p0.x) - 2 * static_cast<i64>(p1.x) + p2.x;
  const i64 ay = static_cast<i64>(p0.y) - 2 * static_cast<i64>(p1.y) + p2.y;
  const i64 bx = static_cast<i64>(p1.x) - 2 * static_cast<i64>(p2.x) + p3.x;
  const i64 by = static_cast<i64>(p1.y) - 2 * static_cast<i64>(p2.y) + p3.y;
  const i64 a2 = sq(ax) + sq(ay), b2 = sq(bx) + sq(by);
  const int k = pick_depth(9 * (a2 > b2 ? a2 : b2), tol);
  const std::size_t count = static_cast<std::size_t>(1) << k;
  if (cap < count) return 0;
  std::size_t n = 0;
  sub_cubic(p0, p1, p2, p3, k, out, n);
  return n;
}

std::uint64_t digest_mask(const Target& t) {
  std::uint64_t h = 0xcbf29ce484222325ull;
  if (t.data == nullptr || t.width == 0 || t.height == 0) return h;
  for (std::uint32_t y = 0; y < t.height; ++y) {
    const std::uint8_t* row = t.data + static_cast<std::size_t>(y) * t.stride;
    for (std::uint32_t x = 0; x < t.width; ++x) {
      h ^= row[x];
      h *= 0x100000001b3ull;
    }
  }
  return h;
}

}  // namespace lumora::raster
