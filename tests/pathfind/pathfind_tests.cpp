// Tests for core/pathfind. Pure CPU, integer only, deterministic (own LCG, no std distributions). Golden path digests are
// also written in docs/pathfind.md and must match on Linux and Windows.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "pathfind.h"

using namespace lumora::pathfind;

namespace {

struct Lcg {
  std::uint32_t s;
  std::uint32_t next() {
    s = s * 1664525u + 1013904223u;
    return s;
  }
};

// An ASCII map: '#' blocked, '.' cost 1, digits 1..9 their cost, 'S' start and 'G' goal (cost 1).
struct Ascii {
  std::vector<std::uint8_t> data;
  std::uint32_t w = 0, h = 0;
  Cell s{0, 0}, g{0, 0};
  Map map() const { return Map{data.data(), w, h}; }
};

Ascii parse(const std::vector<std::string>& rows) {
  Ascii a;
  a.h = static_cast<std::uint32_t>(rows.size());
  a.w = static_cast<std::uint32_t>(rows[0].size());
  for (std::size_t y = 0; y < rows.size(); ++y) {
    REQUIRE(rows[y].size() == a.w);
    for (std::size_t x = 0; x < rows[y].size(); ++x) {
      const char c = rows[y][x];
      std::uint8_t v = 1;
      if (c == '#') v = 0;
      else if (c == 'S') a.s = Cell{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
      else if (c == 'G') a.g = Cell{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
      else if (c >= '1' && c <= '9') v = static_cast<std::uint8_t>(c - '0');
      a.data.push_back(v);
    }
  }
  return a;
}

struct Run {
  PathResult r;
  std::vector<Cell> path;
};

Run run(const Ascii& a, Moves mv) {
  Run o;
  o.path.resize(static_cast<std::size_t>(a.w) * a.h + 1);
  o.r = find_path(a.map(), mv, a.s, a.g, o.path.data(), o.path.size());
  if (o.r.status == PathStatus::Found) o.path.resize(o.r.path_length);
  else o.path.clear();
  return o;
}

bool open_cell(const Map& m, int x, int y) {
  return x >= 0 && y >= 0 && x < static_cast<int>(m.width) && y < static_cast<int>(m.height) &&
         m.data[static_cast<std::size_t>(y) * m.width + static_cast<std::size_t>(x)] != 0;
}

// A path is valid: starts and ends right, every step is a legal move into an open cell, no corner cutting.
std::uint64_t check_path(const Map& m, Moves mv, Cell s, Cell g, const std::vector<Cell>& p) {
  REQUIRE(!p.empty());
  REQUIRE(p.front() == s);
  REQUIRE(p.back() == g);
  std::uint64_t total = 0;
  for (std::size_t i = 1; i < p.size(); ++i) {
    const int dx = p[i].x - p[i - 1].x, dy = p[i].y - p[i - 1].y;
    REQUIRE(((dx != 0) || (dy != 0)));
    REQUIRE(dx >= -1);
    REQUIRE(dx <= 1);
    REQUIRE(dy >= -1);
    REQUIRE(dy <= 1);
    const bool diag = dx != 0 && dy != 0;
    if (mv == Moves::Four) REQUIRE(!diag);
    REQUIRE(open_cell(m, p[i].x, p[i].y));
    if (diag) {
      REQUIRE(open_cell(m, p[i - 1].x + dx, p[i - 1].y));
      REQUIRE(open_cell(m, p[i - 1].x, p[i - 1].y + dy));
    }
    total += step_cost(m.data[static_cast<std::size_t>(p[i].y) * m.width + static_cast<std::size_t>(p[i].x)], diag);
  }
  return total;
}

// Brute force Dijkstra by repeated relaxation (slow, tiny maps only).
std::uint64_t brute_cost(const Map& m, Moves mv, Cell s, Cell g) {
  const std::uint64_t inf = ~0ull;
  const std::size_t n = static_cast<std::size_t>(m.width) * m.height;
  std::vector<std::uint64_t> d(n, inf);
  d[static_cast<std::size_t>(s.y) * m.width + static_cast<std::size_t>(s.x)] = 0;
  const int dx[8] = {0, 1, 0, -1, 1, 1, -1, -1}, dy[8] = {-1, 0, 1, 0, -1, 1, 1, -1};
  for (bool changed = true; changed;) {
    changed = false;
    for (int y = 0; y < static_cast<int>(m.height); ++y) {
      for (int x = 0; x < static_cast<int>(m.width); ++x) {
        const std::uint64_t cur = d[static_cast<std::size_t>(y) * m.width + static_cast<std::size_t>(x)];
        if (cur == inf) continue;
        for (int k = 0; k < (mv == Moves::Eight ? 8 : 4); ++k) {
          const int nx = x + dx[k], ny = y + dy[k];
          if (!open_cell(m, nx, ny)) continue;
          const bool diag = k >= 4;
          if (diag && (!open_cell(m, nx, y) || !open_cell(m, x, ny))) continue;
          const std::uint64_t nd = cur + step_cost(m.data[static_cast<std::size_t>(ny) * m.width + static_cast<std::size_t>(nx)], diag);
          std::uint64_t& slot = d[static_cast<std::size_t>(ny) * m.width + static_cast<std::size_t>(nx)];
          if (nd < slot) {
            slot = nd;
            changed = true;
          }
        }
      }
    }
  }
  return d[static_cast<std::size_t>(g.y) * m.width + static_cast<std::size_t>(g.x)];
}

const std::vector<std::string> kM1 = {"S.......#.......", ".####.#.#.#####.", ".#....#...#...#.", ".#.######.#.#.#.",
                                      ".#........#.#...", ".########.#.###.", "..........#....G"};
const std::vector<std::string> kM2 = {"S1119999....", "1##91111#.#.", "1#991##91..1", "1#1.1#1#9#.#", "1...1#1..1..", "#####.####9G"};
const std::vector<std::string> kM3 = {"S.#.....", ".##.###.", "......#.", ".####.#.", ".#....#G"};

constexpr std::uint64_t kGoldenM1Eight = 0x3eb88fde32a26c10ull;
constexpr std::uint64_t kGoldenM2Four = 0xf6ce76ca4ef438beull;
constexpr std::uint64_t kGoldenM2Eight = 0x7d3fd793f881efe0ull;
constexpr std::uint64_t kGoldenM3Eight = 0x873458ab65f4d281ull;

void same_path(const Run& o, const std::vector<Cell>& want) {
  REQUIRE(o.r.status == PathStatus::Found);
  REQUIRE(o.path.size() == want.size());
  for (std::size_t i = 0; i < want.size(); ++i) {
    REQUIRE(o.path[i].x == want[i].x);
    REQUIRE(o.path[i].y == want[i].y);
  }
}

}  // namespace

TEST_CASE("status texts are distinct", "[pathfind]") {
  const MapStatus ms[] = {MapStatus::Ok, MapStatus::BadSize, MapStatus::NullData, MapStatus::BadCost};
  for (std::size_t i = 0; i < 4; ++i) {
    REQUIRE(std::strlen(map_status_text(ms[i])) > 0);
    for (std::size_t j = i + 1; j < 4; ++j) REQUIRE(std::strcmp(map_status_text(ms[i]), map_status_text(ms[j])) != 0);
  }
  const PathStatus ps[] = {PathStatus::Found,   PathStatus::BadMap,         PathStatus::OutOfBounds, PathStatus::Blocked,
                           PathStatus::NoPath,  PathStatus::BufferTooSmall, PathStatus::BadMoves};
  for (std::size_t i = 0; i < 7; ++i) {
    REQUIRE(std::strlen(path_status_text(ps[i])) > 0);
    for (std::size_t j = i + 1; j < 7; ++j) REQUIRE(std::strcmp(path_status_text(ps[i]), path_status_text(ps[j])) != 0);
  }
}

TEST_CASE("validate_map and step_cost", "[pathfind][map]") {
  std::vector<std::uint8_t> d(16, 1);
  REQUIRE(validate_map(Map{d.data(), 4, 4}) == MapStatus::Ok);
  REQUIRE(validate_map(Map{d.data(), 0, 4}) == MapStatus::BadSize);
  REQUIRE(validate_map(Map{d.data(), 4, 0}) == MapStatus::BadSize);
  REQUIRE(validate_map(Map{d.data(), kMaxDim + 1, 1}) == MapStatus::BadSize);
  REQUIRE(validate_map(Map{d.data(), 1, kMaxDim + 1}) == MapStatus::BadSize);
  REQUIRE(validate_map(Map{nullptr, 4, 4}) == MapStatus::NullData);
  REQUIRE(validate_map(Map{nullptr, 0, 4}) == MapStatus::BadSize);  // size is checked before the pointer
  REQUIRE(validate_map(Map{d.data(), kMaxDim, 1}) == MapStatus::Ok);
  for (int c = 0; c < 256; ++c) {
    REQUIRE(step_cost(static_cast<std::uint8_t>(c), false) == static_cast<std::uint32_t>(c) * 10u);
    REQUIRE(step_cost(static_cast<std::uint8_t>(c), true) == static_cast<std::uint32_t>(c) * 14u);
  }
  REQUIRE(step_cost(255, true) == 3570u);
  REQUIRE(step_cost(255, false) == 2550u);
}

TEST_CASE("find_path: statuses, check order, nothing written on error", "[pathfind][status]") {
  std::vector<std::uint8_t> d = {1, 1, 0, 1, 1, 1};  // 3 x 2, (2,0) blocked
  const Map m{d.data(), 3, 2};
  Cell out[8];
  for (Cell& c : out) c = Cell{-7, -7};
  auto untouched = [&]() {
    for (const Cell& c : out) {
      if (c.x != -7 || c.y != -7) return false;
    }
    return true;
  };
  PathResult r = find_path(Map{nullptr, 3, 2}, Moves::Four, {0, 0}, {1, 1}, out, 8);
  REQUIRE(r.status == PathStatus::BadMap);
  r = find_path(Map{d.data(), 0, 2}, Moves::Four, {0, 0}, {1, 1}, out, 8);
  REQUIRE(r.status == PathStatus::BadMap);
  REQUIRE(r.path_length == 0);
  REQUIRE(r.total_cost == 0);
  REQUIRE(r.expanded == 0);
  const Cell bad_cells[] = {{-1, 0}, {0, -1}, {3, 0}, {0, 2}, {1000000, 0}, {0, -2147483647 - 1}};
  for (const Cell& c : bad_cells) {
    REQUIRE(find_path(m, Moves::Four, c, {1, 1}, out, 8).status == PathStatus::OutOfBounds);
    REQUIRE(find_path(m, Moves::Four, {0, 0}, c, out, 8).status == PathStatus::OutOfBounds);
  }
  REQUIRE(find_path(m, Moves::Four, {2, 0}, {0, 0}, out, 8).status == PathStatus::Blocked);
  REQUIRE(find_path(m, Moves::Four, {0, 0}, {2, 0}, out, 8).status == PathStatus::Blocked);
  const Moves bad_moves = static_cast<Moves>(2);
  REQUIRE(find_path(m, bad_moves, {0, 0}, {1, 1}, out, 8).status == PathStatus::BadMoves);
  REQUIRE(find_path(m, static_cast<Moves>(255), {0, 0}, {1, 1}, out, 8).status == PathStatus::BadMoves);
  // Order: BadMap, OutOfBounds, Blocked, BadMoves.
  REQUIRE(find_path(Map{nullptr, 3, 2}, bad_moves, {-1, 0}, {2, 0}, out, 8).status == PathStatus::BadMap);
  REQUIRE(find_path(m, bad_moves, {-1, 0}, {2, 0}, out, 8).status == PathStatus::OutOfBounds);
  REQUIRE(find_path(m, bad_moves, {0, 0}, {2, 0}, out, 8).status == PathStatus::Blocked);
  REQUIRE(untouched());
  // No path: the goal is walled in.
  const Ascii wall = parse({"S.#..", "..#.G", "..#.."});
  const Run n = run(wall, Moves::Eight);
  REQUIRE(n.r.status == PathStatus::NoPath);
  REQUIRE(n.r.path_length == 0);
  REQUIRE(n.r.total_cost == 0);
  REQUIRE(n.r.expanded == 6);  // every cell of the start side is expanded once
  REQUIRE(run(wall, Moves::Four).r.status == PathStatus::NoPath);
}

TEST_CASE("start equals goal", "[pathfind][status]") {
  const Ascii a = parse({"...", ".9.", "..."});
  Cell out[2] = {{-1, -1}, {-1, -1}};
  PathResult r = find_path(a.map(), Moves::Eight, {1, 1}, {1, 1}, out, 2);
  REQUIRE(r.status == PathStatus::Found);
  REQUIRE(r.path_length == 1);
  REQUIRE(r.total_cost == 0);  // the start cell is not charged, whatever its cost
  REQUIRE(out[0].x == 1);
  REQUIRE(out[0].y == 1);
  REQUIRE(out[1].x == -1);
  r = find_path(a.map(), Moves::Four, {1, 1}, {1, 1}, nullptr, 0);
  REQUIRE(r.status == PathStatus::Found);
  REQUIRE(r.path_length == 1);
  r = find_path(a.map(), Moves::Four, {1, 1}, {1, 1}, out, 0);  // a path of one cell does not fit in capacity 0
  REQUIRE(r.status == PathStatus::BufferTooSmall);
  REQUIRE(r.path_length == 1);
  REQUIRE(r.total_cost == 0);
}

TEST_CASE("four and eight moves, exact tie-determined paths", "[pathfind][ties]") {
  // The ties are settled by f, then lower h, then lower y, then lower x, with the neighbour order N E S W NE SE SW NW and
  // "first parent found wins". The expected paths come from an independent implementation of these rules.
  const Ascii t1 = parse({"S...", "....", "....", "...G"});
  const Run t1f = run(t1, Moves::Four), t1e = run(t1, Moves::Eight);
  same_path(t1f, {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {3, 1}, {3, 2}, {3, 3}});
  REQUIRE(t1f.r.total_cost == 60);
  REQUIRE(t1f.r.expanded == 7);
  same_path(t1e, {{0, 0}, {1, 1}, {2, 2}, {3, 3}});
  REQUIRE(t1e.r.total_cost == 42);
  REQUIRE(t1e.r.expanded == 4);
  const Ascii t2 = parse({"S..", "...", "..G"});
  same_path(run(t2, Moves::Four), {{0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}});
  same_path(run(t2, Moves::Eight), {{0, 0}, {1, 1}, {2, 2}});
  REQUIRE(run(t2, Moves::Four).r.total_cost == 40);
  REQUIRE(run(t2, Moves::Eight).r.total_cost == 28);
  const Ascii t3 = parse({"S....", ".....", "....G"});
  same_path(run(t3, Moves::Four), {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {4, 1}, {4, 2}});
  same_path(run(t3, Moves::Eight), {{0, 0}, {1, 1}, {2, 2}, {3, 2}, {4, 2}});
  REQUIRE(run(t3, Moves::Eight).r.total_cost == 48);
  // A start to the east of the goal, to check the mirrored tie order.
  const Ascii t7 = parse({"G..", "...", "..S"});
  const Run t7f = run(t7, Moves::Four);
  REQUIRE(t7f.r.status == PathStatus::Found);
  REQUIRE(t7f.r.total_cost == 40);
  check_path(t7.map(), Moves::Four, t7.s, t7.g, t7f.path);
}

TEST_CASE("no corner cutting", "[pathfind][corners]") {
  const Ascii t4 = parse({"S#", ".G"});  // the diagonal is blocked by (1,0)
  const Run a = run(t4, Moves::Eight);
  same_path(a, {{0, 0}, {0, 1}, {1, 1}});
  REQUIRE(a.r.total_cost == 20);
  const Ascii t4b = parse({"S.", "#G"});  // blocked by (0,1)
  same_path(run(t4b, Moves::Eight), {{0, 0}, {1, 0}, {1, 1}});
  const Ascii t5 = parse({"S#.", "#G."});  // both blocked: no way
  REQUIRE(run(t5, Moves::Eight).r.status == PathStatus::NoPath);
  REQUIRE(run(t5, Moves::Four).r.status == PathStatus::NoPath);
  const Ascii open = parse({"S.", ".G"});  // both open: the diagonal is taken and costs 14
  const Run d = run(open, Moves::Eight);
  same_path(d, {{0, 0}, {1, 1}});
  REQUIRE(d.r.total_cost == 14);
  // Diagonal between two blocks at the far end, and a diagonal gap two cells wide.
  const Ascii gap = parse({"S.#..", "..#..", "...#G", "..#.."});
  const Run g = run(gap, Moves::Eight);
  REQUIRE(g.r.status == PathStatus::NoPath);  // the only link is a diagonal squeeze between (2,1) and (3,2)
}

TEST_CASE("costs: uniform, varying, the cost of entering a cell", "[pathfind][costs]") {
  for (int c = 1; c <= 255; c += 37) {
    std::vector<std::uint8_t> d(5 * 5, static_cast<std::uint8_t>(c));
    const Map m{d.data(), 5, 5};
    PathResult r = find_path(m, Moves::Four, {0, 0}, {4, 0}, nullptr, 0);
    REQUIRE(r.total_cost == 4u * 10u * static_cast<std::uint32_t>(c));
    r = find_path(m, Moves::Eight, {0, 0}, {4, 4}, nullptr, 0);
    REQUIRE(r.total_cost == 4u * 14u * static_cast<std::uint32_t>(c));
    REQUIRE(r.path_length == 5);
  }
  // The start cell is free, the goal cell is charged: S has cost 9, the goal cell 7.
  const Ascii a = parse({"91G", "..."});
  std::vector<std::uint8_t> d = a.data;
  d[2] = 7;
  const Map m{d.data(), 3, 2};
  const PathResult r = find_path(m, Moves::Four, {0, 0}, {2, 0}, nullptr, 0);
  REQUIRE(r.total_cost == 10u * 1u + 10u * 7u);
  // A longer way round wins when the direct way is expensive.
  const Ascii t6 = parse({"S.9.G", "..9..", "....."});
  const Run f = run(t6, Moves::Four), e = run(t6, Moves::Eight);
  same_path(f, {{0, 0}, {1, 0}, {1, 1}, {1, 2}, {2, 2}, {3, 2}, {3, 1}, {3, 0}, {4, 0}});
  REQUIRE(f.r.total_cost == 80);
  same_path(e, {{0, 0}, {1, 1}, {2, 2}, {3, 1}, {4, 0}});
  REQUIRE(e.r.total_cost == 56);
  // A medium cost wall: pay 9 once (10*9 = 90) or walk around (a detour of 12 steps = 120).
  const Ascii w = parse({"S.9.G"});
  REQUIRE(run(w, Moves::Four).r.total_cost == 10 + 90 + 10 + 10);
}

TEST_CASE("BufferTooSmall and length-only calls", "[pathfind][buffer]") {
  const Ascii t1 = parse({"S...", "....", "....", "...G"});
  Cell out[10];
  for (Cell& c : out) c = Cell{-5, -5};
  PathResult r = find_path(t1.map(), Moves::Four, t1.s, t1.g, out, 3);
  REQUIRE(r.status == PathStatus::BufferTooSmall);
  REQUIRE(r.path_length == 7);
  REQUIRE(r.total_cost == 60);
  REQUIRE(out[0] == Cell{0, 0});
  REQUIRE(out[1] == Cell{1, 0});
  REQUIRE(out[2] == Cell{2, 0});
  REQUIRE(out[3] == Cell{-5, -5});  // the first `capacity` cells are written and nothing more
  r = find_path(t1.map(), Moves::Four, t1.s, t1.g, out, 7);  // an exact fit
  REQUIRE(r.status == PathStatus::Found);
  REQUIRE(out[6] == Cell{3, 3});
  r = find_path(t1.map(), Moves::Four, t1.s, t1.g, out, 6);  // one short
  REQUIRE(r.status == PathStatus::BufferTooSmall);
  REQUIRE(r.path_length == 7);
  r = find_path(t1.map(), Moves::Four, t1.s, t1.g, nullptr, 0);
  REQUIRE(r.status == PathStatus::Found);  // length only
  REQUIRE(r.path_length == 7);
  r = find_path(t1.map(), Moves::Four, t1.s, t1.g, nullptr, 100);
  REQUIRE(r.status == PathStatus::Found);
  REQUIRE(r.total_cost == 60);
}

TEST_CASE("optimal against a brute force Dijkstra on seeded maps", "[pathfind][optimal]") {
  Lcg g{12345};
  const std::uint8_t costs[] = {1, 1, 1, 2, 3, 5, 9, 255};
  std::size_t found = 0, none = 0;
  for (int it = 0; it < 600; ++it) {
    const std::uint32_t w = 1 + g.next() % 14, h = 1 + g.next() % 14;
    const std::uint32_t density = 10 + g.next() % 35;  // percent blocked
    std::vector<std::uint8_t> d(static_cast<std::size_t>(w) * h);
    for (std::uint8_t& c : d) c = (g.next() % 100 < density) ? 0 : costs[g.next() % 8];
    std::vector<std::uint32_t> open;
    for (std::size_t i = 0; i < d.size(); ++i) {
      if (d[i]) open.push_back(static_cast<std::uint32_t>(i));
    }
    if (open.empty()) continue;
    const std::uint32_t si = open[g.next() % open.size()], gi = open[g.next() % open.size()];
    const Cell s{static_cast<std::int32_t>(si % w), static_cast<std::int32_t>(si / w)};
    const Cell goal{static_cast<std::int32_t>(gi % w), static_cast<std::int32_t>(gi / w)};
    const Map m{d.data(), w, h};
    for (Moves mv : {Moves::Four, Moves::Eight}) {
      std::vector<Cell> path(d.size() + 1);
      const PathResult r = find_path(m, mv, s, goal, path.data(), path.size());
      const std::uint64_t want = brute_cost(m, mv, s, goal);
      if (want == ~0ull) {
        REQUIRE(r.status == PathStatus::NoPath);
        ++none;
        continue;
      }
      ++found;
      REQUIRE(r.status == PathStatus::Found);
      REQUIRE(r.total_cost == want);
      path.resize(r.path_length);
      REQUIRE(check_path(m, mv, s, goal, path) == want);
      REQUIRE(r.expanded >= 1);
      REQUIRE(r.expanded <= d.size());
      // The same call twice gives the same path (determinism within a run).
      std::vector<Cell> again(d.size() + 1);
      const PathResult r2 = find_path(m, mv, s, goal, again.data(), again.size());
      REQUIRE(r2.expanded == r.expanded);
      REQUIRE(digest_path(again.data(), r2.path_length) == digest_path(path.data(), path.size()));
    }
  }
  REQUIRE(found > 300);
  REQUIRE(none > 30);
}

TEST_CASE("narrow gaps", "[pathfind][gaps]") {
  // A wall with a single one-cell gap: the path must pass through it.
  std::vector<std::string> rows(9, std::string(9, '.'));
  for (std::size_t y = 0; y < 9; ++y) rows[y][4] = (y == 6) ? '.' : '#';
  rows[0][0] = 'S';
  rows[8][8] = 'G';
  const Ascii a = parse(rows);
  for (Moves mv : {Moves::Four, Moves::Eight}) {
    const Run o = run(a, mv);
    REQUIRE(o.r.status == PathStatus::Found);
    check_path(a.map(), mv, a.s, a.g, o.path);
    bool through = false;
    for (const Cell& c : o.path) through = through || (c.x == 4 && c.y == 6);
    REQUIRE(through);
  }
  // The same wall but with a diagonal-only link (the gap cell's neighbours are walls): corner cutting forbids it.
  std::vector<std::string> rows2(5, std::string(5, '.'));
  rows2[2] = "##.##";
  rows2[1] = "..#..";
  rows2[3] = "..#..";
  rows2[0][0] = 'S';
  rows2[4][4] = 'G';
  const Ascii b = parse(rows2);
  const Run o2 = run(b, Moves::Four);
  REQUIRE(o2.r.status == PathStatus::NoPath);
}

TEST_CASE("golden path digests", "[pathfind][digest]") {
  const Ascii m1 = parse(kM1), m2 = parse(kM2), m3 = parse(kM3);
  const Run a = run(m1, Moves::Eight), b = run(m2, Moves::Four), c = run(m2, Moves::Eight), d = run(m3, Moves::Eight);
  REQUIRE(a.r.status == PathStatus::Found);
  REQUIRE(b.r.status == PathStatus::Found);
  REQUIRE(c.r.status == PathStatus::Found);
  REQUIRE(d.r.status == PathStatus::Found);
  INFO("digests " << std::hex << digest_path(a.path.data(), a.path.size()) << " " << digest_path(b.path.data(), b.path.size()) << " "
                  << digest_path(c.path.data(), c.path.size()) << " " << digest_path(d.path.data(), d.path.size()));
  REQUIRE(digest_path(a.path.data(), a.path.size()) == kGoldenM1Eight);
  REQUIRE(digest_path(b.path.data(), b.path.size()) == kGoldenM2Four);
  REQUIRE(digest_path(c.path.data(), c.path.size()) == kGoldenM2Eight);
  REQUIRE(digest_path(d.path.data(), d.path.size()) == kGoldenM3Eight);
  REQUIRE(a.r.total_cost == 250);
  REQUIRE(a.path.size() == 26);
  REQUIRE(a.r.expanded == 51);
  REQUIRE(b.r.total_cost == 300);
  REQUIRE(c.r.total_cost == 228);
  REQUIRE(c.path.size() == 15);
  REQUIRE(d.r.total_cost == 150);
  // digest_path of an empty path and of a known path.
  REQUIRE(digest_path(nullptr, 0) == 0xcbf29ce484222325ull);
  const Cell two[2] = {{1, -2}, {0, 3}};
  REQUIRE(digest_path(two, 2) == 0x2558bd7cd4b61e42ull);
}

TEST_CASE("total_cost saturates at 0xFFFFFFFF, the path stays exact", "[pathfind][overflow]") {
  // A 4096 x 1024 serpentine corridor of cost 255 cells: about 2.1 million steps of 2550 is over 2^32.
  const std::uint32_t w = 4096, h = 1024;
  std::vector<std::uint8_t> d(static_cast<std::size_t>(w) * h, 0);
  for (std::uint32_t y = 0; y < h; y += 2) {
    for (std::uint32_t x = 0; x < w; ++x) d[static_cast<std::size_t>(y) * w + x] = 255;
    if (y + 1 < h) d[static_cast<std::size_t>(y + 1) * w + ((y % 4 == 0) ? w - 1 : 0)] = 255;
  }
  const Map m{d.data(), w, h};
  const PathResult r = find_path(m, Moves::Eight, {0, 0}, {static_cast<std::int32_t>(w) - 1, static_cast<std::int32_t>(h) - 2}, nullptr, 0);
  REQUIRE(r.status == PathStatus::Found);
  REQUIRE(r.total_cost == 0xFFFFFFFFu);
  REQUIRE(r.path_length > 1700000);
  // Just below the limit it is exact: a short corridor of known cost.
  std::vector<std::uint8_t> e(1000, 255);
  const PathResult s = find_path(Map{e.data(), 1000, 1}, Moves::Four, {0, 0}, {999, 0}, nullptr, 0);
  REQUIRE(s.total_cost == 999u * 2550u);
}

TEST_CASE("perf: 4096 x 4096, open map and a long winding path (sanitized)", "[pathfind][perf]") {
  const std::uint32_t n = 4096;
  const std::int32_t last = static_cast<std::int32_t>(n) - 1;
  std::vector<std::uint8_t> d(static_cast<std::size_t>(n) * n, 1);
  Lcg g{777};
  for (std::uint8_t& c : d) {
    if (g.next() % 100 < 3) c = 0;  // light random obstacles
  }
  d[0] = 1;
  d[static_cast<std::size_t>(n) * n - 1] = 1;
  const Map m{d.data(), n, n};
  std::vector<Cell> path(2000000);  // both paths are shorter than this
  auto t0 = std::chrono::steady_clock::now();
  PathResult r = find_path(m, Moves::Eight, {0, 0}, {last, last}, path.data(), path.size());
  double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  INFO("open map: seconds " << secs << " length " << r.path_length << " expanded " << r.expanded);
  REQUIRE(r.status == PathStatus::Found);
  path.resize(r.path_length);
  REQUIRE(check_path(m, Moves::Eight, {0, 0}, {last, last}, path) == r.total_cost);
  REQUIRE(secs < 10.0);

  // Winding path: a serpentine corridor (one cell wide) through the top 512 rows of an otherwise open map.
  std::fill(d.begin(), d.end(), static_cast<std::uint8_t>(1));
  for (std::uint32_t y = 0; y < 512; ++y) {
    for (std::uint32_t x = 0; x < n; ++x) {
      std::uint8_t c = 0;
      if (y % 2 == 0) c = 1;
      else if (x == ((y % 4 == 1) ? n - 1 : 0)) c = 1;
      d[static_cast<std::size_t>(y) * n + x] = c;
    }
  }
  path.assign(2000000, Cell{0, 0});
  t0 = std::chrono::steady_clock::now();
  r = find_path(m, Moves::Eight, {0, 0}, {0, 510}, path.data(), path.size());
  secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  INFO("serpentine: seconds " << secs << " length " << r.path_length << " expanded " << r.expanded);
  REQUIRE(r.status == PathStatus::Found);
  REQUIRE(r.path_length > 500000);
  path.resize(r.path_length);
  REQUIRE(check_path(m, Moves::Eight, {0, 0}, {0, 510}, path) == r.total_cost);
  REQUIRE(secs < 10.0);
}
