// Lumora 2D spatial grid (scope L): a deterministic broadphase for axis-aligned boxes. Pure CPU, no I/O, no threads.
// The interface in this header is fixed; the contributor implements core/grid/grid.cpp only.
// Results (including the order of every output list) must be identical on Linux, Windows and Android: integer
// math and float comparisons only, no libm, no hashing of pointers, no unordered containers, no constant division
// by zero or constant overflow (MSVC rejects those at compile time).
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::grid {

struct Aabb {
  float min_x, min_y, max_x, max_y;
};

constexpr std::size_t kMaxItems = 65536;
constexpr std::uint32_t kMaxCellsPerAxis = 4096;

enum class GridStatus : std::uint8_t {
  Ok = 0,
  BadCellSize,   // not finite or <= 0
  BadOrigin,     // not finite or |coordinate| > 1e9
  BadCells,      // cells_x or cells_y is 0 or > kMaxCellsPerAxis
  BadBox,        // non-finite coordinate or min > max (checked on insert and update)
  TooManyItems,  // insert beyond kMaxItems live items
  UnknownItem,   // id not live (remove, update, query_item)
};
const char* grid_status_text(GridStatus s);

// A fixed-size grid. World cell of a point p: floor((p - origin) / cell_size) clamped to 0..cells-1 on each axis,
// so boxes outside the grid land in the border cells (the grid never rejects by position).
// An item occupies every cell its box overlaps (closed intervals: a box edge exactly on a cell border occupies both cells).
class SpatialGrid {
 public:
  SpatialGrid();
  ~SpatialGrid();
  SpatialGrid(const SpatialGrid&) = delete;
  SpatialGrid& operator=(const SpatialGrid&) = delete;

  GridStatus init(float origin_x, float origin_y, float cell_size, std::uint32_t cells_x, std::uint32_t cells_y);

  // Insert a box. Item ids are handed out in increasing order starting at 0 and are never reused after remove.
  GridStatus insert(const Aabb& box, std::uint32_t& out_id);
  GridStatus remove(std::uint32_t id);
  GridStatus update(std::uint32_t id, const Aabb& box);
  std::size_t live_count() const;

  // All live items whose box intersects `query` (closed: touching intersects), ids in ascending order, no duplicates.
  // Writes at most `capacity` ids; returns the total number of matches (so a caller can detect truncation).
  // An invalid query box (non-finite or min > max) returns 0.
  std::size_t query(const Aabb& query, std::uint32_t* out_ids, std::size_t capacity) const;

  // All unordered pairs (a < b) of live items whose boxes intersect (closed), sorted by (a, b) ascending.
  // Writes at most `capacity` pairs (a in out_a[i], b in out_b[i]); returns the total number of pairs.
  std::size_t pairs(std::uint32_t* out_a, std::uint32_t* out_b, std::size_t capacity) const;

  // Digest for golden tests: FNV-1a 64 over the little-endian 4-byte values of the live item count, then for every
  // live item in ascending id order: id and the 4 float bit patterns of its box.
  std::uint64_t digest() const;

 private:
  struct Impl;
  Impl* impl_;
};

// FNV-1a 64 over 32-bit values (4 bytes each, little-endian) in order, for golden tests of query and pair results.
std::uint64_t digest_u32(const std::uint32_t* v, std::size_t n);

}  // namespace lumora::grid
