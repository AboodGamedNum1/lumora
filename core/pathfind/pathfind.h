// Lumora grid pathfinding (scope M): deterministic A* on a 2D occupancy grid. Pure CPU, no I/O, no threads.
// The interface in this header is fixed; the contributor implements core/pathfind/pathfind.cpp only.
// Results must be identical on Linux, Windows and Android: integer costs only (no floats in the search), no
// unordered containers, no pointer hashing, no constant division by zero or constant overflow (MSVC rejects those).
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::pathfind {

struct Cell {
  std::int32_t x, y;
};
inline bool operator==(const Cell& a, const Cell& b) { return a.x == b.x && a.y == b.y; }

constexpr std::uint32_t kMaxDim = 4096;  // width and height limit

enum class MapStatus : std::uint8_t {
  Ok = 0,
  BadSize,      // width or height is 0 or > kMaxDim
  NullData,     // data pointer is null
  BadCost,      // a cell cost byte is invalid (see below)
};
const char* map_status_text(MapStatus s);

// Row-major cost bytes, index = y * width + x. 0 means blocked. 1..255 is the cost to ENTER that cell (see step_cost).
struct Map {
  const std::uint8_t* data = nullptr;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};
MapStatus validate_map(const Map& m);  // checks in enum order; BadCost is never produced (every byte is valid), kept for evolution

enum class Moves : std::uint8_t { Four = 0, Eight = 1 };

// Cost to enter a cell with byte c: orthogonal c * 10, diagonal c * 14 (integers, 14 ~ 10 * sqrt(2)).
// Diagonal steps are allowed only when both orthogonally adjacent cells are not blocked (no corner cutting).
std::uint32_t step_cost(std::uint8_t c, bool diagonal);

enum class PathStatus : std::uint8_t {
  Found = 0,
  BadMap,        // validate_map failed
  OutOfBounds,   // start or goal outside the map
  Blocked,       // start or goal cell is blocked
  NoPath,
  BufferTooSmall,  // path longer than capacity; path_length still reports the true length
  BadMoves,
};
const char* path_status_text(PathStatus s);

struct PathResult {
  PathStatus status = PathStatus::NoPath;
  std::uint32_t total_cost = 0;    // sum of step costs along the path (start cell not charged)
  std::size_t path_length = 0;     // number of cells including start and goal
  std::size_t expanded = 0;        // cells popped from the open set (for tests and tuning)
};

// A*. Heuristic: octile distance for Eight (10 * max(dx,dy) + 4 * min(dx,dy)), Manhattan * 10 for Four; both are
// admissible and consistent for cost-1 cells, and are scaled so they stay admissible for all costs >= 1.
// Determinism rules: ties in f are broken by lower h, then lower y, then lower x of the cell; neighbors are expanded
// in the order N (0,-1), E (1,0), S (0,1), W (-1,0), then NE, SE, SW, NW. The returned path is the one this rule
// produces, start to goal. Writes at most `capacity` cells to out_path (null out_path = length only).
// start == goal returns Found with a one-cell path and cost 0.
PathResult find_path(const Map& m, Moves moves, Cell start, Cell goal, Cell* out_path, std::size_t capacity);

// FNV-1a 64 over (x, y) pairs as little-endian 4-byte two's complement values, for golden tests.
std::uint64_t digest_path(const Cell* p, std::size_t n);

}  // namespace lumora::pathfind
