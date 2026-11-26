// Lumora CPU outline builder (scope Y). Integer only, no heap, no recursion, no mutable statics. See docs/outline.md.
#include "outline.h"

namespace lumora::outline {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;

bool coord_bad(std::int32_t v) { return v < -lumora::flatten::kMaxCoord || v > lumora::flatten::kMaxCoord; }
bool point_bad(const Point& p) { return coord_bad(p.x) || coord_bad(p.y); }
bool same(const Point& a, const Point& b) { return a.x == b.x && a.y == b.y; }
bool count_ok(u32 n) { return n >= 1 && n <= kMaxCommands; }

// Overlap of two touched byte ranges. Same modular address rule as core/composite, core/resample, core/sdf, core/tess, core/clip,
// core/stroke and core/flatten: clear the top byte, take the unsigned difference modulo 2^64, answer "no overlap" at
// |delta| >= 2^47 before any other arithmetic, then compare the exact ranges. Both lengths are positive.
bool ranges_overlap(const void* a, u64 a_len, const void* b, u64 b_len) {
  const u64 kTagMask = 0x00FFFFFFFFFFFFFFull;
  const u64 ua = static_cast<u64>(reinterpret_cast<std::uintptr_t>(a)) & kTagMask;
  const u64 ub = static_cast<u64>(reinterpret_cast<std::uintptr_t>(b)) & kTagMask;
  const u64 diff = ua - ub;
  const bool negative = diff >= (static_cast<u64>(1) << 63);
  const u64 absd = negative ? (static_cast<u64>(0) - diff) : diff;
  if (absd >= (static_cast<u64>(1) << 47)) return false;
  const i64 delta = negative ? -static_cast<i64>(absd) : static_cast<i64>(absd);  // start of A relative to the start of B
  return delta < static_cast<i64>(b_len) && delta + static_cast<i64>(a_len) > 0;
}

struct Extent {
  const void* p;
  u64 bytes;
};
template <std::size_t N>
bool any_overlap(const Extent (&e)[N]) {
  for (std::size_t i = 0; i < N; ++i)
    for (std::size_t j = i + 1; j < N; ++j)
      if (ranges_overlap(e[i].p, e[i].bytes, e[j].p, e[j].bytes)) return true;
  return false;
}

bool op_ok(Op op) { return static_cast<std::uint8_t>(op) <= static_cast<std::uint8_t>(Op::Close); }

// The point fields an op uses (0 for Close).
int used_points(Op op) {
  switch (op) {
    case Op::Move: return 1;
    case Op::Line: return 1;
    case Op::Quad: return 2;
    case Op::Cubic: return 3;
    case Op::Close: return 0;
  }
  return 0;
}

struct Fnv {
  u64 h = 0xcbf29ce484222325ull;
  void word(u32 v) {
    for (int b = 0; b < 4; ++b) {
      h ^= (v >> (8 * b)) & 0xFFu;
      h *= 0x100000001b3ull;
    }
  }
};

// Contour under construction, written straight into the output array.
struct Builder {
  Point* pts;
  u32* ends;
  std::uint8_t* closed;
  u32 np = 0, nc = 0;
  u32 start = 0;       // index of the first point of the current contour
  bool active = false;  // a subpath is in progress
  void begin(const Point& p) {
    start = np;
    pts[np++] = p;
    active = true;
  }
  void add(const Point& p) {
    if (!same(pts[np - 1], p)) pts[np++] = p;  // consecutive duplicates collapse
  }
  // Ends the current subpath. A closed contour drops a last point equal to the first; fewer than two points are dropped.
  void finish(bool is_closed) {
    if (!active) return;
    active = false;
    if (is_closed && np - start >= 2 && same(pts[np - 1], pts[start])) --np;
    if (np - start >= 2) {
      ends[nc] = np;
      closed[nc] = is_closed ? 1 : 0;
      ++nc;
    } else {
      np = start;
    }
  }
};

}  // namespace

const char* status_text(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::NullBuffer: return "null buffer";
    case Status::BadCount: return "bad count";
    case Status::BadTolerance: return "bad tolerance";
    case Status::BadCommand: return "bad command";
    case Status::BadPoint: return "bad point";
    case Status::BadPath: return "bad path";
    case Status::BufferTooSmall: return "buffer too small";
    case Status::Overlap: return "overlap";
  }
  return "unknown";
}

std::uint32_t contour_bound(std::uint32_t command_count) { return count_ok(command_count) ? command_count : 0; }

std::uint32_t point_bound(std::uint32_t command_count) {
  return count_ok(command_count) ? command_count * kPointsPerCommand : 0;
}

Status build_outline(const Command* commands, std::uint32_t command_count, std::int32_t tolerance, Point* pts,
                     std::uint32_t pts_capacity, std::uint32_t* contour_ends, std::uint32_t ends_capacity, std::uint8_t* closed,
                     std::uint32_t closed_capacity, std::uint32_t* contour_count, std::uint32_t* point_count_out) {
  if (commands == nullptr || pts == nullptr || contour_ends == nullptr || closed == nullptr || contour_count == nullptr ||
      point_count_out == nullptr)
    return Status::NullBuffer;
  if (!count_ok(command_count)) return Status::BadCount;
  if (tolerance < 1 || tolerance > lumora::flatten::kMaxTolerance) return Status::BadTolerance;
  for (u32 i = 0; i < command_count; ++i)
    if (!op_ok(commands[i].op)) return Status::BadCommand;
  for (u32 i = 0; i < command_count; ++i) {
    const Command& c = commands[i];
    const int used = used_points(c.op);
    if ((used >= 1 && point_bad(c.a)) || (used >= 2 && point_bad(c.b)) || (used >= 3 && point_bad(c.c))) return Status::BadPoint;
  }
  {
    // 0: nothing yet, 1: a current point exists, 2: after Close
    int state = 0;
    for (u32 i = 0; i < command_count; ++i) {
      const Op op = commands[i].op;
      if (op == Op::Move) {
        state = 1;
      } else if (op == Op::Close) {
        if (state != 1) return Status::BadPath;
        state = 2;
      } else {
        if (state != 1) return Status::BadPath;
      }
    }
  }
  const u32 cb = contour_bound(command_count), pb = point_bound(command_count);
  if (pts_capacity < pb || ends_capacity < cb || closed_capacity < cb) return Status::BufferTooSmall;
  const Extent ext[6] = {{commands, static_cast<u64>(command_count) * sizeof(Command)},
                         {pts, static_cast<u64>(pb) * sizeof(Point)},
                         {contour_ends, static_cast<u64>(cb) * sizeof(u32)},
                         {closed, static_cast<u64>(cb)},
                         {contour_count, sizeof(u32)},
                         {point_count_out, sizeof(u32)}};
  if (any_overlap(ext)) return Status::Overlap;

  Builder b{pts, contour_ends, closed};
  Point scratch[lumora::flatten::kMaxOutputPoints];  // local flatten output, no heap and no static
  u32 scratch_count = 0;
  for (u32 i = 0; i < command_count; ++i) {
    const Command& c = commands[i];
    switch (c.op) {
      case Op::Move:
        b.finish(false);
        b.begin(c.a);
        break;
      case Op::Line:
        b.add(c.a);
        break;
      case Op::Quad:
      case Op::Cubic: {
        const Point cur = pts[b.np - 1];
        const lumora::flatten::Status st =
            c.op == Op::Quad ? lumora::flatten::flatten_quad(cur, c.a, c.b, tolerance, scratch, lumora::flatten::kMaxOutputPoints,
                                                             &scratch_count)
                             : lumora::flatten::flatten_cubic(cur, c.a, c.b, c.c, tolerance, scratch,
                                                              lumora::flatten::kMaxOutputPoints, &scratch_count);
        // Unreachable after the checks above (see docs/outline.md): valid tolerance, valid points, full capacity, no overlap.
        if (st != lumora::flatten::Status::Ok) return Status::BadPoint;
        for (u32 k = 1; k < scratch_count; ++k) b.add(scratch[k]);
        break;
      }
      case Op::Close:
        b.finish(true);
        break;
    }
  }
  b.finish(false);
  *contour_count = b.nc;
  *point_count_out = b.np;
  return Status::Ok;
}

std::uint64_t digest_outline(const Point* pts, const std::uint32_t* contour_ends, const std::uint8_t* closed,
                             std::uint32_t contour_count) {
  Fnv f;
  f.word(contour_count);
  if (contour_count == 0 || contour_ends == nullptr || closed == nullptr) return f.h;
  for (u32 i = 0; i < contour_count; ++i) {
    f.word(contour_ends[i]);
    f.word(closed[i] != 0 ? 1u : 0u);
  }
  if (pts != nullptr) {
    const u32 total = contour_ends[contour_count - 1];
    for (u32 i = 0; i < total; ++i) {
      f.word(static_cast<u32>(pts[i].x));
      f.word(static_cast<u32>(pts[i].y));
    }
  }
  return f.h;
}

}  // namespace lumora::outline
