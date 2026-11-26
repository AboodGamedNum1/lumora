// Lumora 2D polygon geometry module (scope I). Pure CPU, deterministic, no I/O, no threads.
// The interface in this header is fixed; the contributor implements core/geom/geom.cpp only.
// Results must be bit-identical on Linux, Windows and Android: only + - * / sqrt, double internally,
// FMA contraction off, no libm transcendentals. Orientation tests must be exact (see orient below).
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

// Twice the signed area (shoelace, vertices in order), computed exactly and rounded once to double.
// > 0 means counter-clockwise. The rounding limit is documented in docs/geom.md.
double signed_area2(const Pt* v, std::size_t n);

// Exact orientation of three float points: +1 counter-clockwise, -1 clockwise, 0 collinear.
// The sign is exact for every finite input (a plain double determinant is NOT always sign-correct, because
// coordinate differences can be inexact). The method and its proof sketch are documented in docs/geom.md.
// Non-finite input returns 0.
int orient(const Pt& a, const Pt& b, const Pt& c);

// Ear-clipping triangulation. Output: (n - 2) triangles as vertex indices, counter-clockwise triangles,
// out_indices has room for 3 * (n - 2) entries. Ear choice rule: the scan starts at vertex 0 and walks the remaining
// vertices cyclically; the first valid ear wins; after each clip the scan restarts at the vertex before the clipped one.
// Details (containment rules, fallback) are documented in docs/geom.md. Collinear vertices are removed as zero-area ears.
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
