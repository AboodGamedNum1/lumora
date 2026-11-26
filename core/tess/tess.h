// Lumora CPU polygon tessellation (scope U). Pure CPU, deterministic, no I/O, no threads, no floating point at all.
// The interface in this header is fixed; the contributor implements core/tess/tess.cpp only (plus tests and docs).
// Results must be bit-identical on Linux, Windows and Android: integer arithmetic only (64-bit where needed), no libm, no
// floats, no unordered containers, no static mutable state, no heap allocation, no recursion, no pointer values in any decision.
// Purpose: turning simple polygons (shape outlines, flattened glyphs and paths) into triangles for the 2D renderer. Evidence
// label: software (CPU) only.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::tess {

constexpr std::uint32_t kMaxVertices = 4096;
constexpr std::int32_t kMaxCoord = 1 << 28;  // |x| and |y| of every vertex; query points may use any int32 value

struct Point {
  std::int32_t x, y;
};

enum class Status : std::uint8_t {
  Ok = 0,
  NullBuffer,       // a required pointer is null (count > 0 for arrays; result pointers always)
  BadCount,         // vertex_count < 3 or > kMaxVertices
  BadPoint,         // a vertex coordinate is beyond kMaxCoord in magnitude
  BadPolygon,       // not a simple polygon (see is_simple), or zero area
  BufferTooSmall,   // out_capacity (triangles) < vertex_count - 2, or scratch_count < scratch_size(vertex_count)
  Overlap           // touched byte ranges intersect (points, scratch, out; integer address ranges as in core/composite)
};
// Lower-case spaced enum names ("ok", "null buffer", "bad count", "bad point", "bad polygon", "buffer too small", "overlap"),
// "unknown" otherwise.
const char* status_text(Status s);

// Twice the signed area (shoelace formula; counter-clockwise is positive). The sign and zero test must be exact for every input
// that passes the checks, also self-intersecting ones whose int64 sum could overflow (use a two-word accumulator). *out_area2 is
// the exact value when it fits in int64, otherwise it saturates to INT64_MAX or INT64_MIN (status stays Ok). Checks: NullBuffer, BadCount,
// BadPoint. The vertex ring is closed implicitly (the last vertex connects to the first). Writes *area2 only on Ok. For a simple polygon within 2^28 the magnitude bound is
// |area2| <= 2^59 (document the bound used).
Status area2(const Point* pts, std::uint32_t count, std::int64_t* out_area2);

// Simple polygon test, exact integers, O(n^2) is fine for 4096 vertices. Writes *simple (true or false) and returns Ok for a
// well-formed input. A polygon is simple when: no two vertices coincide, no two non-adjacent edges share any point (touching,
// crossing and overlap all count), adjacent edges share only their common vertex (a spike that folds an edge back onto the
// previous one is not simple), and the area is not zero. Collinear consecutive vertices ARE allowed. Checks: NullBuffer
// (pts and result pointer), BadCount, BadPoint.
Status is_simple(const Point* pts, std::uint32_t count, bool* simple);

// Where a point lies relative to a simple polygon: exact, any int32 query coordinates, boundary points (on an edge or equal to
// a vertex) report Boundary. The polygon is NOT revalidated (the caller passes a simple polygon; results for a non-simple
// polygon are unspecified but must not crash). Checks: NullBuffer, BadCount, BadPoint (vertices only).
enum class Location : std::uint8_t { Outside = 0, Inside = 1, Boundary = 2 };
Status locate(const Point* pts, std::uint32_t count, std::int32_t px, std::int32_t py, Location* where);

// Caller scratch for triangulate: scratch_size(count) uint32 values. Fixed: 3 * count (the contributor chooses the layout, for
// example previous, next and one spare per vertex). Returns 0 for an invalid count (< 3 or > kMaxVertices).
std::uint32_t scratch_size(std::uint32_t count);

// Ear clipping triangulation of a simple polygon of either winding. Output triangles are written as index triples into pts
// (3 uint32 per triangle, out must hold 3 * out_capacity values); every triangle has counter-clockwise winding and
// non-zero area, and together they tile the polygon exactly: sum of their doubled areas equals |area2| (tests must assert it).
// *tri_count receives the number of triangles; it is exactly count - 2 minus the number of collinear vertices removed
// (see the rule). Deterministic rule, fixed here:
//  - Work on the ring of remaining vertices in the polygon's own order; if the winding is clockwise treat the ring as
//    reversed only for orientation tests (indices written keep the triangle counter-clockwise, so swap two indices).
//  - A pass starts at the lowest remaining vertex index and visits each vertex of the ring (as it was when the pass started) once, in
//    ring order (not by index), with no restart inside the pass except after an ear clip. The first vertex v (previous a, next b) that is an ear is clipped: remove v and emit the triple (a, v, b) when the polygon is
//    counter-clockwise, or (b, v, a) when it is clockwise (so every emitted triangle is counter-clockwise). After an ear clip a new
//    pass starts at the lowest remaining vertex index.
//  - Vertex v is an ear when the corner (a, v, b) turns strictly in the interior direction (strictly convex) and no other
//    remaining vertex lies inside the closed triangle (a, v, b) except vertices that coincide with a or b (none do in a simple
//    polygon, so: no other remaining vertex at all inside or on the triangle).
//  - Collinear vertices (turn exactly zero) are dropped without emitting a triangle, when the scan meets them, and the scan
//    then continues at the dropped vertex's successor (no restart after a drop); a dropped vertex never appears in a triangle.
//    A pass that ends with drops but no ear starts a new pass at the lowest remaining index.
//  - Only if a full pass found neither a drop nor an ear (can only happen through boundary touching of remaining
//    vertices), clip the first strictly convex vertex whose triangle has no remaining vertex strictly inside (boundary allowed).
//    The contributor must prove in docs/tess.md that the loop terminates and that a strictly convex vertex always exists.
// Checks in enum order: NullBuffer (pts, out, scratch, tri_count), BadCount, BadPoint, BufferTooSmall, Overlap (every pair of: pts,
// the scratch_size(count) scratch words, the out extent 3 * min(out_capacity, count - 2) words, and the 4-byte *tri_count; touched
// ranges, modular address rule as in core/composite), BadPolygon (is_simple first). Nothing is written on error. The one
// exception: an internal "no ear found" return after output was started is unreachable for a simple polygon; docs/tess.md must
// prove that (two ears theorem with this exact rule, collinear drops and the fallback included), the code keeps a defensive
// BadPolygon return there, and the tests must show it is never taken. Costs: O(n^2) per ear search is acceptable, O(n^3) worst case for 4096 vertices is NOT: keep the amortised cost near
// O(n^2) and document it.
Status triangulate(const Point* pts, std::uint32_t count, std::uint32_t* out, std::uint32_t out_capacity, std::uint32_t* scratch,
                   std::uint32_t scratch_count, std::uint32_t* tri_count);

// FNV-1a 64 over the 32-bit little-endian bytes of each value in order, for golden tests. Null or zero count hashes zero bytes.
std::uint64_t digest_indices(const std::uint32_t* values, std::size_t count);

}  // namespace lumora::tess
