// Deterministic 2D spatial grid. Rules, cell math, ordering and golden digests: docs/grid.md.
// Determinism: integer math, double arithmetic with + - * / and comparisons only, ordered containers only (no hashing, no
// pointer values, no unordered containers). Query and pair results come from the exact closed float box test, never
// from the cell lists alone, so how cells are assigned cannot change any output.
#include "grid.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <vector>

#if defined(_MSC_VER) && !defined(__clang__)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

namespace lumora::grid {
namespace {

inline bool is_finite(float v) { return v - v == 0.0f; }  // false for NaN and infinity


bool valid_box(const Aabb& b) {
  return is_finite(b.min_x) && is_finite(b.min_y) && is_finite(b.max_x) && is_finite(b.max_y) && b.min_x <= b.max_x &&
         b.min_y <= b.max_y;
}

inline bool intersects(const Aabb& a, const Aabb& b) {
  return a.min_x <= b.max_x && b.min_x <= a.max_x && a.min_y <= b.max_y && b.min_y <= a.max_y;
}

inline std::uint32_t float_bits(float f) {
  std::uint32_t u;
  std::memcpy(&u, &f, sizeof u);
  return u;
}

struct Hash {
  std::uint64_t h = 0xcbf29ce484222325ull;
  void u32(std::uint32_t v) {
    for (int k = 0; k < 4; ++k) {
      h ^= (v >> (8 * k)) & 0xffu;
      h *= 0x100000001b3ull;
    }
  }
};

constexpr double kCellEps = 1e-9;          // conservative widening of a cell range, in cell units
constexpr std::uint64_t kLargeCells = 64;  // items covering more cells than this live in the large list
constexpr std::uint64_t kScanCells = 256;  // queries covering more cells than this scan all live items

// Cell index of coordinate value t (in cell units), clamped to 0..cells-1 before the integer conversion.
inline int clamp_cell(double t, std::uint32_t cells) {
  if (!(t > 0.0)) return 0;
  if (t >= static_cast<double>(cells)) return static_cast<int>(cells - 1);
  return static_cast<int>(t);  // t is in (0, cells): truncation is the floor
}

struct Item {
  std::uint32_t id;
  Aabb box;
  bool live;
  bool large;
  int x0, y0, x1, y1;
};

}  // namespace

struct SpatialGrid::Impl {
  bool inited = false;
  double ox = 0, oy = 0, cs = 1;
  std::uint32_t cx = 0, cy = 0;
  std::vector<Item> items;                                  // ascending by id, live and dead
  std::map<std::uint32_t, std::vector<std::uint32_t>> cells;  // cell index -> item ids (any order)
  std::vector<std::uint32_t> large;                         // ids of large items, ascending
  std::uint32_t next_id = 0;
  std::size_t live = 0, dead = 0;

  void reset() {
    items.clear();
    cells.clear();
    large.clear();
    next_id = 0;
    live = dead = 0;
  }

  void range(const Aabb& b, int& x0, int& y0, int& x1, int& y1) const {
    x0 = clamp_cell((static_cast<double>(b.min_x) - ox) / cs - kCellEps, cx);
    x1 = clamp_cell((static_cast<double>(b.max_x) - ox) / cs + kCellEps, cx);
    y0 = clamp_cell((static_cast<double>(b.min_y) - oy) / cs - kCellEps, cy);
    y1 = clamp_cell((static_cast<double>(b.max_y) - oy) / cs + kCellEps, cy);
  }

  std::size_t find(std::uint32_t id) const {  // index in items, or items.size()
    std::size_t lo = 0, hi = items.size();
    while (lo < hi) {
      const std::size_t mid = lo + (hi - lo) / 2;
      if (items[mid].id < id) lo = mid + 1;
      else hi = mid;
    }
    return (lo < items.size() && items[lo].id == id) ? lo : items.size();
  }

  std::size_t find_live(std::uint32_t id) const {
    const std::size_t i = find(id);
    return (i < items.size() && items[i].live) ? i : items.size();
  }

  void place(Item& it) {
    range(it.box, it.x0, it.y0, it.x1, it.y1);
    const std::uint64_t n = static_cast<std::uint64_t>(it.x1 - it.x0 + 1) * static_cast<std::uint64_t>(it.y1 - it.y0 + 1);
    it.large = n > kLargeCells;
    if (it.large) {
      large.insert(std::lower_bound(large.begin(), large.end(), it.id), it.id);
      return;
    }
    for (int y = it.y0; y <= it.y1; ++y) {
      for (int x = it.x0; x <= it.x1; ++x) {
        cells[static_cast<std::uint32_t>(y) * cx + static_cast<std::uint32_t>(x)].push_back(it.id);
      }
    }
  }

  void unplace(const Item& it) {
    if (it.large) {
      large.erase(std::lower_bound(large.begin(), large.end(), it.id));
      return;
    }
    for (int y = it.y0; y <= it.y1; ++y) {
      for (int x = it.x0; x <= it.x1; ++x) {
        auto f = cells.find(static_cast<std::uint32_t>(y) * cx + static_cast<std::uint32_t>(x));
        if (f == cells.end()) continue;
        std::vector<std::uint32_t>& v = f->second;
        for (std::size_t k = 0; k < v.size(); ++k) {
          if (v[k] == it.id) {
            v[k] = v.back();
            v.pop_back();
            break;
          }
        }
        if (v.empty()) cells.erase(f);
      }
    }
  }

  void compact() {
    std::size_t out = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
      if (items[i].live) items[out++] = items[i];
    }
    items.resize(out);
    dead = 0;
  }

  // Candidate ids from the cells under `b` plus the large list: sorted, unique, only ids greater than `above` when
  // `use_above`.
  void candidates(const Aabb& b, bool use_above, std::uint32_t above, std::vector<std::uint32_t>& out) const {
    out.clear();
    int x0, y0, x1, y1;
    range(b, x0, y0, x1, y1);
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        auto f = cells.find(static_cast<std::uint32_t>(y) * cx + static_cast<std::uint32_t>(x));
        if (f == cells.end()) continue;
        for (std::uint32_t id : f->second) {
          if (!use_above || id > above) out.push_back(id);
        }
      }
    }
    for (std::uint32_t id : large) {
      if (!use_above || id > above) out.push_back(id);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
  }
};

const char* grid_status_text(GridStatus s) {
  switch (s) {
    case GridStatus::Ok: return "ok";
    case GridStatus::BadCellSize: return "bad cell size";
    case GridStatus::BadOrigin: return "bad origin";
    case GridStatus::BadCells: return "bad cells";
    case GridStatus::BadBox: return "bad box";
    case GridStatus::TooManyItems: return "too many items";
    case GridStatus::UnknownItem: return "unknown item";
  }
  return "unknown";
}

SpatialGrid::SpatialGrid() : impl_(new Impl()) {}
SpatialGrid::~SpatialGrid() { delete impl_; }

GridStatus SpatialGrid::init(float origin_x, float origin_y, float cell_size, std::uint32_t cells_x, std::uint32_t cells_y) {
  if (!is_finite(cell_size) || !(cell_size > 0.0f)) return GridStatus::BadCellSize;
  if (!is_finite(origin_x) || !is_finite(origin_y) || origin_x > 1e9f || origin_x < -1e9f || origin_y > 1e9f ||
      origin_y < -1e9f) {
    return GridStatus::BadOrigin;
  }
  if (cells_x == 0 || cells_y == 0 || cells_x > kMaxCellsPerAxis || cells_y > kMaxCellsPerAxis) return GridStatus::BadCells;
  Impl& m = *impl_;
  m.reset();
  m.inited = true;
  m.ox = origin_x;
  m.oy = origin_y;
  m.cs = cell_size;
  m.cx = cells_x;
  m.cy = cells_y;
  return GridStatus::Ok;
}

GridStatus SpatialGrid::insert(const Aabb& box, std::uint32_t& out_id) {
  Impl& m = *impl_;
  if (!m.inited) return GridStatus::BadCells;
  if (!valid_box(box)) return GridStatus::BadBox;
  if (m.live >= kMaxItems || m.next_id == 0xffffffffu) return GridStatus::TooManyItems;
  Item it{};
  it.id = m.next_id++;
  it.box = box;
  it.live = true;
  m.place(it);
  m.items.push_back(it);
  ++m.live;
  out_id = it.id;
  return GridStatus::Ok;
}

GridStatus SpatialGrid::remove(std::uint32_t id) {
  Impl& m = *impl_;
  const std::size_t i = m.find_live(id);
  if (i == m.items.size()) return GridStatus::UnknownItem;
  m.unplace(m.items[i]);
  m.items[i].live = false;
  --m.live;
  ++m.dead;
  if (m.dead > 1024 && m.dead > m.live) m.compact();
  return GridStatus::Ok;
}

GridStatus SpatialGrid::update(std::uint32_t id, const Aabb& box) {
  Impl& m = *impl_;
  if (!valid_box(box)) return GridStatus::BadBox;
  const std::size_t i = m.find_live(id);
  if (i == m.items.size()) return GridStatus::UnknownItem;
  m.unplace(m.items[i]);
  m.items[i].box = box;
  m.place(m.items[i]);
  return GridStatus::Ok;
}

std::size_t SpatialGrid::live_count() const { return impl_->live; }

std::size_t SpatialGrid::query(const Aabb& q, std::uint32_t* out_ids, std::size_t capacity) const {
  const Impl& m = *impl_;
  if (!m.inited || !valid_box(q)) return 0;
  if (out_ids == nullptr) capacity = 0;
  std::size_t total = 0;
  auto emit = [&](std::uint32_t id) {
    if (total < capacity) out_ids[total] = id;
    ++total;
  };
  int x0, y0, x1, y1;
  m.range(q, x0, y0, x1, y1);
  const std::uint64_t n = static_cast<std::uint64_t>(x1 - x0 + 1) * static_cast<std::uint64_t>(y1 - y0 + 1);
  if (n > kScanCells) {
    for (const Item& it : m.items) {
      if (it.live && intersects(it.box, q)) emit(it.id);
    }
    return total;
  }
  std::vector<std::uint32_t> cand;
  m.candidates(q, false, 0, cand);
  for (std::uint32_t id : cand) {
    const std::size_t i = m.find_live(id);
    if (i != m.items.size() && intersects(m.items[i].box, q)) emit(id);
  }
  return total;
}

std::size_t SpatialGrid::pairs(std::uint32_t* out_a, std::uint32_t* out_b, std::size_t capacity) const {
  const Impl& m = *impl_;
  if (!m.inited) return 0;
  if (out_a == nullptr || out_b == nullptr) capacity = 0;
  std::size_t total = 0;
  std::vector<std::uint32_t> cand;
  for (const Item& a : m.items) {
    if (!a.live) continue;
    if (a.large) {
      for (const Item& b : m.items) {
        if (!b.live || b.id <= a.id || !intersects(a.box, b.box)) continue;
        if (total < capacity) {
          out_a[total] = a.id;
          out_b[total] = b.id;
        }
        ++total;
      }
      continue;
    }
    m.candidates(a.box, true, a.id, cand);
    for (std::uint32_t id : cand) {
      const std::size_t i = m.find_live(id);
      if (i == m.items.size() || !intersects(a.box, m.items[i].box)) continue;
      if (total < capacity) {
        out_a[total] = a.id;
        out_b[total] = id;
      }
      ++total;
    }
  }
  return total;
}

std::uint64_t SpatialGrid::digest() const {
  const Impl& m = *impl_;
  Hash h;
  h.u32(static_cast<std::uint32_t>(m.live));
  for (const Item& it : m.items) {
    if (!it.live) continue;
    h.u32(it.id);
    h.u32(float_bits(it.box.min_x));
    h.u32(float_bits(it.box.min_y));
    h.u32(float_bits(it.box.max_x));
    h.u32(float_bits(it.box.max_y));
  }
  return h.h;
}

std::uint64_t digest_u32(const std::uint32_t* v, std::size_t n) {
  Hash h;
  for (std::size_t i = 0; i < n; ++i) h.u32(v[i]);
  return h.h;
}

}  // namespace lumora::grid
