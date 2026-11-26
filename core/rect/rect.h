// Integer rectangles (damage regions, clip boxes): validity, intersection, bounding union, point test and area. Pure integer: no floating
// point, no libm, no 128-bit types (MSVC), no allocation, no global state. All intermediate sums are 64-bit. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::rect {

struct Rect {
  std::int32_t x = 0, y = 0;  // top left corner
  std::int32_t w = 0, h = 0;  // width and height; the rectangle covers x <= px < x + w and y <= py < y + h
};

// A rectangle is valid when w >= 0, h >= 0, x + w <= INT32_MAX and y + h <= INT32_MAX (sums in 64 bits). Every function below treats an
// invalid rectangle exactly like an empty one. A rectangle is empty when it is invalid or w == 0 or h == 0.
bool is_valid(const Rect& r);
bool is_empty(const Rect& r);

// The overlap of a and b: all zeros (x 0, y 0, w 0, h 0) when either is empty or they share no pixel; otherwise x = max of the x values,
// y = max of the y values, and the width and height reach the smaller right and bottom edges. Touching edges do not overlap.
Rect intersect(const Rect& a, const Rect& b);

// True when intersect(a, b) is not empty.
bool overlaps(const Rect& a, const Rect& b);

// Smallest rectangle containing both. An empty input is ignored: if both are empty the result is all zeros and the return is true; if one
// is empty the result is the other one. Otherwise x and y are the smaller left and top edges and the right and bottom edges are the larger
// ones. Returns false and leaves *out unchanged when out is null or the union's width or height would exceed INT32_MAX (the result is
// never an invalid rectangle).
bool bounds_union(const Rect& a, const Rect& b, Rect* out);

// True when the point lies inside the half-open rectangle; always false for an empty rectangle.
bool contains_point(const Rect& r, std::int32_t px, std::int32_t py);

// w * h as uint64; 0 for an empty rectangle.
std::uint64_t area(const Rect& r);

// Decisions: bounds_union checks out for null first and leaves *out unchanged on every false return, even when an input is empty. For two
// non-empty inputs the bounding width and height are computed in 64 bits and rejected only when above INT32_MAX (equal is accepted), so far
// apart valid rectangles can fail although each edge fits in int32. out may alias a or b: all fields are computed first and *out is
// assigned once at the end (tests cover both aliases). Right and bottom edges exactly INT32_MAX are valid, above are invalid; zero width
// or height is valid but empty; a negative width or height is invalid; negative coordinates are allowed. contains_point compares in 64 bits
// against the half-open edges. A header ambiguity found later is a question to the maintainer, not an interpretation.

}  // namespace lumora::rect
