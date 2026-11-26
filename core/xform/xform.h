// Lumora 2D transform and camera module (scope J). Pure CPU, deterministic, no I/O, no threads.
// The interface in this header is fixed; the contributor implements core/xform/xform.cpp only.
// Results must be bit-identical on Linux, Windows and Android: only + - * / sqrt, double internally,
// FMA contraction off, no libm transcendentals (sin, cos, atan2 and similar must be own code).
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::xform {

struct Vec2 {
  float x, y;
};

// 2D affine matrix, column-vector convention: x' = a*x + c*y + tx, y' = b*x + d*y + ty.
struct Affine {
  float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
};

Affine identity();
Affine translation(float tx, float ty);
Affine scaling(float sx, float sy);
// Rotation by `radians` counter-clockwise (y up): a = cos, b = sin, c = -sin, d = cos.
// Own sin/cos, argument reduced exactly enough for |radians| <= 1e4 (documented max error); beyond that or NaN returns identity.
Affine rotation(float radians);
// Compose: result applies `rhs` first, then `lhs` (lhs * rhs). Computed in double, rounded once per entry.
Affine multiply(const Affine& lhs, const Affine& rhs);
Vec2 apply(const Affine& m, Vec2 p);
Vec2 apply_vector(const Affine& m, Vec2 v);  // no translation

// Determinant a*d - b*c computed exactly (expansion) and rounded once to double.
double determinant(const Affine& m);
// Inverse. Returns false (out untouched) when the matrix has non-finite entries or the exact determinant is 0
// or the inverse would overflow to non-finite float entries.
bool invert(const Affine& m, Affine& out);

// Translate, rotate (radians), scale in the order T * R * S (scale first, translate last).
Affine compose_trs(Vec2 translate, float radians, Vec2 scale);

enum class CameraStatus : std::uint8_t {
  Ok = 0,
  BadViewport,   // width or height not finite or <= 0 or > 1e6
  BadZoom,       // not finite, <= 0, or > 1e6, or < 1e-6
  BadPosition,   // not finite or |coordinate| > 1e9
  BadRotation,   // not finite or |radians| > 1e4
};
const char* camera_status_text(CameraStatus s);

// Orthographic 2D camera. World space is y up. `position` is the world point shown at the viewport center.
// Screen space has the origin at the top-left corner of the viewport and y down, in pixels.
struct Camera {
  Vec2 position{0, 0};
  float zoom = 1;       // pixels per world unit
  float rotation = 0;   // radians, counter-clockwise: the view is rotated by this angle around `position`
  float width = 1280;   // viewport pixels
  float height = 720;
};

CameraStatus validate_camera(const Camera& c);  // checks in enum order
// World to screen and back. Invalid camera: world_to_screen / screen_to_world return false (out untouched).
bool camera_matrix(const Camera& c, Affine& world_to_screen);
bool world_to_screen(const Camera& c, Vec2 world, Vec2& out);
bool screen_to_world(const Camera& c, Vec2 screen, Vec2& out);

// Axis-aligned bounds of the world region visible through the camera (rotation included).
struct Rect {
  float min_x, min_y, max_x, max_y;
};
// Returns false on an invalid camera. Bounds are rounded outward to float.
bool visible_world_bounds(const Camera& c, Rect& out);

// Axis-aligned bounds of a rectangle after an affine transform, rounded outward to float.
Rect transform_rect(const Affine& m, const Rect& r);

// FNV-1a 64 over the little-endian bit patterns of the floats, in order, for golden tests.
std::uint64_t digest_floats(const float* v, std::size_t n);

}  // namespace lumora::xform
