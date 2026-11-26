// Deterministic grid A*. Rules, tie-breaking, overflow policy and golden digests: docs/pathfind.md.
// Determinism: integer math only (no floating point anywhere), a binary heap over a total order, fixed neighbour order,
// no unordered containers and no pointer values.
#include "pathfind.h"

#include <algorithm>
#include <vector>

namespace lumora::pathfind {
namespace {

constexpr std::uint64_t kUnreached = ~0ull;
constexpr std::uint32_t kSaturated = 0xffffffffu;

// Neighbour order: N, E, S, W, NE, SE, SW, NW.
constexpr int kDx[8] = {0, 1, 0, -1, 1, 1, -1, -1};
constexpr int kDy[8] = {-1, 0, 1, 0, -1, 1, 1, -1};

struct Entry {
  std::uint64_t f;
  std::uint32_t h;
  std::uint32_t idx;  // y * width + x: ascending idx is ascending y, then ascending x
};

// Min-heap order on (f, h, y, x). The key identifies the cell, so equal keys are identical entries.
struct After {
  bool operator()(const Entry& a, const Entry& b) const {
    if (a.f != b.f) return a.f > b.f;
    if (a.h != b.h) return a.h > b.h;
    return a.idx > b.idx;
  }
};

// The 32-bit total. The search itself uses 64-bit costs, so the path is exact; only the reported value is capped.
// Every real total is a sum of multiples of 10 and 14, so it is even and 0xffffffff can never be a real total.
std::uint32_t report_cost(std::uint64_t total) { return total > 0xfffffffeull ? kSaturated : static_cast<std::uint32_t>(total); }

}  // namespace

const char* map_status_text(MapStatus s) {
  switch (s) {
    case MapStatus::Ok: return "ok";
    case MapStatus::BadSize: return "bad size";
    case MapStatus::NullData: return "null data";
    case MapStatus::BadCost: return "bad cost";
  }
  return "unknown";
}

const char* path_status_text(PathStatus s) {
  switch (s) {
    case PathStatus::Found: return "found";
    case PathStatus::BadMap: return "bad map";
    case PathStatus::OutOfBounds: return "out of bounds";
    case PathStatus::Blocked: return "blocked";
    case PathStatus::NoPath: return "no path";
    case PathStatus::BufferTooSmall: return "buffer too small";
    case PathStatus::BadMoves: return "bad moves";
  }
  return "unknown";
}

MapStatus validate_map(const Map& m) {
  if (m.width == 0 || m.height == 0 || m.width > kMaxDim || m.height > kMaxDim) return MapStatus::BadSize;
  if (m.data == nullptr) return MapStatus::NullData;
  return MapStatus::Ok;  // every byte is a valid cost
}

std::uint32_t step_cost(std::uint8_t c, bool diagonal) {
  return static_cast<std::uint32_t>(c) * (diagonal ? 14u : 10u);
}

PathResult find_path(const Map& m, Moves moves, Cell start, Cell goal, Cell* out_path, std::size_t capacity) {
  PathResult res;
  if (validate_map(m) != MapStatus::Ok) {
    res.status = PathStatus::BadMap;
    return res;
  }
  const std::int64_t w = m.width, h = m.height;
  auto inside = [&](const Cell& c) { return c.x >= 0 && c.y >= 0 && c.x < w && c.y < h; };
  if (!inside(start) || !inside(goal)) {
    res.status = PathStatus::OutOfBounds;
    return res;
  }
  const std::uint32_t width = m.width;
  const std::uint32_t s_idx = static_cast<std::uint32_t>(start.y) * width + static_cast<std::uint32_t>(start.x);
  const std::uint32_t g_idx = static_cast<std::uint32_t>(goal.y) * width + static_cast<std::uint32_t>(goal.x);
  if (m.data[s_idx] == 0 || m.data[g_idx] == 0) {
    res.status = PathStatus::Blocked;
    return res;
  }
  if (moves != Moves::Four && moves != Moves::Eight) {
    res.status = PathStatus::BadMoves;
    return res;
  }
  const int dirs = moves == Moves::Eight ? 8 : 4;

  const std::size_t cells = static_cast<std::size_t>(m.width) * m.height;
  std::vector<std::uint64_t> g(cells, kUnreached);
  std::vector<std::uint8_t> dir(cells, 0);
  std::vector<std::uint8_t> closed(cells, 0);
  std::vector<Entry> open;

  auto heuristic = [&](std::uint32_t idx) {
    const std::uint32_t x = idx % width, y = idx / width;
    const std::uint32_t dx = x > static_cast<std::uint32_t>(goal.x) ? x - static_cast<std::uint32_t>(goal.x) : static_cast<std::uint32_t>(goal.x) - x;
    const std::uint32_t dy = y > static_cast<std::uint32_t>(goal.y) ? y - static_cast<std::uint32_t>(goal.y) : static_cast<std::uint32_t>(goal.y) - y;
    if (moves == Moves::Eight) return 10u * (dx > dy ? dx : dy) + 4u * (dx > dy ? dy : dx);
    return 10u * (dx + dy);
  };

  g[s_idx] = 0;
  {
    const std::uint32_t hs = heuristic(s_idx);
    open.push_back(Entry{hs, hs, s_idx});
  }
  bool found = false;
  while (!open.empty()) {
    std::pop_heap(open.begin(), open.end(), After());
    const Entry e = open.back();
    open.pop_back();
    if (closed[e.idx]) continue;  // an older, worse entry of a cell that was already expanded
    closed[e.idx] = 1;
    ++res.expanded;
    if (e.idx == g_idx) {
      found = true;
      break;
    }
    const std::int32_t cx = static_cast<std::int32_t>(e.idx % width), cy = static_cast<std::int32_t>(e.idx / width);
    for (int d = 0; d < dirs; ++d) {
      const std::int32_t nx = cx + kDx[d], ny = cy + kDy[d];
      if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
      const std::uint32_t n_idx = static_cast<std::uint32_t>(ny) * width + static_cast<std::uint32_t>(nx);
      const std::uint8_t c = m.data[n_idx];
      if (c == 0 || closed[n_idx]) continue;
      const bool diagonal = d >= 4;
      if (diagonal) {  // no corner cutting: both orthogonal neighbours must be open
        if (m.data[static_cast<std::uint32_t>(cy) * width + static_cast<std::uint32_t>(nx)] == 0) continue;
        if (m.data[static_cast<std::uint32_t>(ny) * width + static_cast<std::uint32_t>(cx)] == 0) continue;
      }
      const std::uint64_t ng = g[e.idx] + step_cost(c, diagonal);
      if (ng < g[n_idx]) {  // strictly lower only: the first parent found in expansion order wins ties
        g[n_idx] = ng;
        dir[n_idx] = static_cast<std::uint8_t>(d);
        const std::uint32_t hn = heuristic(n_idx);
        open.push_back(Entry{ng + hn, hn, n_idx});
        std::push_heap(open.begin(), open.end(), After());
      }
    }
  }
  if (!found) {
    res.status = PathStatus::NoPath;
    return res;
  }

  res.total_cost = report_cost(g[g_idx]);
  std::size_t length = 1;
  for (std::uint32_t at = g_idx; at != s_idx; ++length) {
    const int d = dir[at];
    const std::int32_t px = static_cast<std::int32_t>(at % width) - kDx[d], py = static_cast<std::int32_t>(at / width) - kDy[d];
    at = static_cast<std::uint32_t>(py) * width + static_cast<std::uint32_t>(px);
  }
  res.path_length = length;
  res.status = PathStatus::Found;
  if (out_path != nullptr) {
    if (length > capacity) res.status = PathStatus::BufferTooSmall;
    std::size_t pos = length - 1;
    std::uint32_t at = g_idx;
    for (;;) {
      if (pos < capacity) out_path[pos] = Cell{static_cast<std::int32_t>(at % width), static_cast<std::int32_t>(at / width)};
      if (at == s_idx) break;
      const int d = dir[at];
      const std::int32_t px = static_cast<std::int32_t>(at % width) - kDx[d], py = static_cast<std::int32_t>(at / width) - kDy[d];
      at = static_cast<std::uint32_t>(py) * width + static_cast<std::uint32_t>(px);
      --pos;
    }
  }
  return res;
}

std::uint64_t digest_path(const Cell* p, std::size_t n) {
  std::uint64_t h = 0xcbf29ce484222325ull;
  auto word = [&h](std::int32_t v) {
    const std::uint32_t u = static_cast<std::uint32_t>(v);
    for (int k = 0; k < 4; ++k) {
      h ^= (u >> (8 * k)) & 0xffu;
      h *= 0x100000001b3ull;
    }
  };
  for (std::size_t i = 0; i < n; ++i) {
    word(p[i].x);
    word(p[i].y);
  }
  return h;
}

}  // namespace lumora::pathfind
