// Lumora 2D polygon geometry module (scope I). Pure CPU, deterministic, no I/O, no threads.
// The interface in this header is fixed; the contributor implements core/geom/geom.cpp only.
// Results must be bit-identical on Linux, Windows and Android: only + - * / sqrt, double internally,
// FMA contraction off, no libm transcendentals. Orientation tests must be exact for the inputs (see below).
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::geom {

struct Pt {
  float x, y;
};

constexpr std::size_t kMaxVertices = 4096;

enum class PolyStatus : std::uint8_t {
  Ok = 0,
  TooFewVertices,   // fewer than 3
  TooManyVertices,  // more than kMaxVertices
  BadVertex,        // NaN, infinite or |coordinate| > 1e6, or null pointer
  Degenerate,       // zero area (all vertices collinear or coincident)
  SelfIntersecting, // any two non-adjacent edges cross or touch, or adjacent edges overlap
};
const char* poly_status_text(PolyStatus s);

// Simple polygon, vertices in order, implicitly closed. Either winding is accepted.
PolyStatus validate_polygon(const Pt* v, std::size_t n);  // checks in enum order; O(n^2) is fine

// Twice the signed area, exact summation in double (shoelace, vertices in order). > 0 means counter-clockwise.
double signed_area2(const Pt* v, std::size_t n);

// Exact orientation of three float points: +1 counter-clockwise, -1 clockwise, 0 collinear.
// Float inputs make the determinant exactly computable in double (document the argument in docs/geom.md).
int orient(const Pt& a, const Pt& b, const Pt& c);

// Ear-clipping triangulation. Output: (n - 2) triangles as vertex indices, counter-clockwise triangles,
// out_indices has room for 3 * (n - 2) entries. Ear choice rule: always clip the lowest-index valid ear,
// restarting the scan from the previous vertex after each clip. Collinear vertices are removed as zero-area ears.
// Returns the number of triangles written (a collinear vertex may reduce it below n - 2) or 0 on an invalid polygon.
std::size_t triangulate(const Pt* v, std::size_t n, std::uint32_t* out_indices);

// Point in polygon, even-odd rule. Points on an edge or vertex count as inside. NaN point is outside.
bool point_in_polygon(const Pt* v, std::size_t n, const Pt& p);

// Convex hull (Andrew monotone chain), counter-clockwise, starting at the lowest x (lowest y on ties),
// collinear points removed, duplicates removed. Writes at most n points to out, returns the count
// (0 for n == 0, 1 or 2 for fewer than three distinct points). Any NaN or non-finite point returns 0.
std::size_t convex_hull(const Pt* pts, std::size_t n, Pt* out);

// FNV-1a 64 over the little-endian bytes of each index (4 bytes), in order, for golden tests.
std::uint64_t digest_indices(const std::uint32_t* idx, std::size_t n);

}  // namespace lumora::geom
