// Lumora CPU vector path drawing (scope Z). Integer only, no heap, no recursion, no mutable statics. See docs/vecdraw.md.
// This file only composes core/outline, core/stroke and core/raster. It contains no flattening, stroking or rasterising logic.
#include "vecdraw.h"

namespace lumora::vecdraw {

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;

// Overlap of two touched byte ranges. Same modular address rule as core/composite, core/resample, core/sdf, core/tess, core/clip,
// core/stroke, core/flatten and core/outline: clear the top byte, take the unsigned difference modulo 2^64, answer "no overlap" at
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

// Fixed element sizes used by the work layout. The layout never depends on sizeof of a platform pointer.
constexpr std::size_t kPointBytes = 8;  // raster::P, flatten::Point and stroke::Point: two int32
constexpr std::size_t kDescBytes = 16;  // one raster::Contour descriptor (a pointer and a size_t, at most 16 bytes)
static_assert(sizeof(raster::P) == kPointBytes, "raster point layout");
static_assert(sizeof(outline::Point) == kPointBytes, "outline point layout");
static_assert(sizeof(stroke::Point) == kPointBytes, "stroke point layout");
static_assert(sizeof(raster::Contour) <= kDescBytes, "raster contour descriptor layout");
static_assert(alignof(raster::Contour) <= 8 && alignof(raster::P) <= 8 && alignof(stroke::Point) <= 8, "alignment");

constexpr std::size_t kStrokeContourPoints = 4;  // every stroke contour is a triangle or a quad
// Per-polyline stroke buffers for the largest input (stroke::kMaxPoints points, closed).
constexpr std::size_t kStrokeInPoints = stroke::kMaxPoints;
constexpr std::size_t kStrokeOutContours = 2 * stroke::kMaxPoints;                    // contour_bound(1024, closed)
constexpr std::size_t kStrokeOutPoints = kStrokeContourPoints * kStrokeOutContours;   // point_bound(1024, closed)
constexpr std::size_t kKeptContours = raster::kMaxContours;
constexpr std::size_t kKeptPoints = kStrokeContourPoints * raster::kMaxContours;

constexpr std::size_t round8(std::size_t v) { return (v + 7) & ~static_cast<std::size_t>(7); }

struct Layout {
  std::size_t pts, ends, closed;      // outline output
  std::size_t sinp, soutp, sends;       // stroke only: per-contour stroke input, output points, output ends
  std::size_t rpts, desc;             // raster point copies and descriptors
  std::size_t total;
};

// n is a valid command count (1 .. kMaxCommands). Every offset and size is a multiple of 8.
Layout layout_for(std::uint32_t n, bool stroke) {
  Layout l{};
  std::size_t off = 0;
  l.pts = off;
  off += round8(static_cast<std::size_t>(outline::point_bound(n)) * kPointBytes);
  l.ends = off;
  off += round8(static_cast<std::size_t>(outline::contour_bound(n)) * sizeof(u32));
  l.closed = off;
  off += round8(static_cast<std::size_t>(outline::contour_bound(n)));
  if (stroke) {
    l.sinp = off;
    off += round8(kStrokeInPoints * kPointBytes);
    l.soutp = off;
    off += round8(kStrokeOutPoints * kPointBytes);
    l.sends = off;
    off += round8(kStrokeOutContours * sizeof(u32));
    l.rpts = off;
    off += round8(kKeptPoints * kPointBytes);
    l.desc = off;
    off += round8(kKeptContours * kDescBytes);
  } else {
    l.rpts = off;
    off += round8(static_cast<std::size_t>(outline::point_bound(n)) * kPointBytes);
    l.desc = off;
    off += round8(static_cast<std::size_t>(outline::contour_bound(n)) * kDescBytes);
  }
  l.total = off;
  return l;
}

template <class T>
T* at(void* base, std::size_t off) {
  return reinterpret_cast<T*>(static_cast<std::uint8_t*>(base) + off);
}

bool size_bad(const raster::Target& t) {
  return t.width == 0 || t.height == 0 || t.width > raster::kMaxSize || t.height > raster::kMaxSize || t.stride < t.width;
}

bool style_bad(const stroke::Style& s) {
  return s.half_width < 1 || s.half_width > stroke::kMaxHalfWidth || s.miter_limit < 1 || s.miter_limit > stroke::kMaxMiterLimit ||
         static_cast<std::uint8_t>(s.join) > static_cast<std::uint8_t>(stroke::Join::Miter) ||
         static_cast<std::uint8_t>(s.cap) > static_cast<std::uint8_t>(stroke::Cap::Square);
}

bool coord_bad(std::int32_t v) { return v < -raster::kMaxCoord || v > raster::kMaxCoord; }

// Runs the outline checks (BadCommand, BadPoint, BadPath) without touching any caller memory: with a zero capacity the outline
// builder reports every sequence error first and then BufferTooSmall, and writes nothing on any error.
Status validate_path(const outline::Command* commands, u32 n, std::int32_t tolerance) {
  outline::Point dummy_pts[1];
  u32 dummy_ends[1];
  std::uint8_t dummy_closed[1];
  u32 dummy_cc = 0, dummy_pc = 0;
  const outline::Status s = outline::build_outline(commands, n, tolerance, dummy_pts, 0, dummy_ends, 0, dummy_closed, 0, &dummy_cc,
                                                   &dummy_pc);
  switch (s) {
    case outline::Status::BufferTooSmall: return Status::Ok;  // every check before the capacity checks passed
    case outline::Status::BadCommand: return Status::BadCommand;
    case outline::Status::BadPoint: return Status::BadPoint;
    case outline::Status::BadPath: return Status::BadPath;
    case outline::Status::BadTolerance: return Status::BadTolerance;
    case outline::Status::BadCount: return Status::BadCount;
    case outline::Status::NullBuffer: return Status::NullBuffer;
    default: return Status::Overlap;  // unreachable: Ok and Overlap need capacities that the zero capacity cannot satisfy
  }
}

// The checks shared by both entry points up to and including WorkTooSmall and Overlap. Common order, with `stroke_style` null for
// fill: NullBuffer, BadSize, BadCount, BadTolerance, BadFill or BadStyle, BadCommand, BadPoint, BadPath, WorkTooSmall, Overlap.
Status common_checks(const outline::Command* commands, u32 n, std::int32_t tolerance, const raster::Fill* rule,
                     const stroke::Style* style, const raster::Target& target, void* work, std::size_t work_bytes) {
  const bool is_stroke = style != nullptr;
  if (commands == nullptr || target.data == nullptr || work == nullptr) return Status::NullBuffer;
  if (size_bad(target)) return Status::BadSize;
  if (n < 1 || n > kMaxCommands) return Status::BadCount;
  if (tolerance < 1 || tolerance > kMaxTolerance) return Status::BadTolerance;
  if (is_stroke) {
    if (style_bad(*style)) return Status::BadStyle;
  } else if (*rule != raster::Fill::NonZero && *rule != raster::Fill::EvenOdd) {
    return Status::BadFill;
  }
  const Status vs = validate_path(commands, n, tolerance);
  if (vs != Status::Ok) return vs;
  const Layout l = layout_for(n, is_stroke);
  if (work_bytes < l.total || (reinterpret_cast<std::uintptr_t>(work) % 8) != 0) return Status::WorkTooSmall;
  const Extent e[3] = {{commands, static_cast<u64>(n) * sizeof(outline::Command)},
                       {target.data, static_cast<u64>(target.stride) * (target.height - 1) + target.width},
                       {work, static_cast<u64>(work_bytes)}};
  for (int i = 0; i < 3; ++i)
    for (int j = i + 1; j < 3; ++j)
      if (ranges_overlap(e[i].p, e[i].bytes, e[j].p, e[j].bytes)) return Status::Overlap;
  return Status::Ok;
}

Status map_raster(raster::RasterStatus s) {
  switch (s) {
    case raster::RasterStatus::Ok: return Status::Ok;
    case raster::RasterStatus::NullBuffer: return Status::NullBuffer;
    case raster::RasterStatus::BadSize: return Status::BadSize;
    case raster::RasterStatus::BadFill: return Status::BadFill;
    case raster::RasterStatus::BadContour: return Status::TooManyContours;
    case raster::RasterStatus::BadPoint: return Status::BadPoint;
  }
  return Status::BadPoint;
}

}  // namespace

const char* status_text(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::NullBuffer: return "null buffer";
    case Status::BadSize: return "bad size";
    case Status::BadCount: return "bad count";
    case Status::BadTolerance: return "bad tolerance";
    case Status::BadFill: return "bad fill";
    case Status::BadStyle: return "bad style";
    case Status::BadCommand: return "bad command";
    case Status::BadPoint: return "bad point";
    case Status::BadPath: return "bad path";
    case Status::WorkTooSmall: return "work too small";
    case Status::Overlap: return "overlap";
    case Status::TooManyPoints: return "too many points";
    case Status::TooManyContours: return "too many contours";
  }
  return "unknown";
}

std::size_t work_bytes_needed(std::uint32_t command_count, bool stroke) {
  if (command_count < 1 || command_count > kMaxCommands) return 0;
  return layout_for(command_count, stroke).total;
}

Status fill_path(const outline::Command* commands, std::uint32_t command_count, std::int32_t tolerance, raster::Fill rule,
                 const raster::Target& target, void* work, std::size_t work_bytes) {
  const Status cs = common_checks(commands, command_count, tolerance, &rule, nullptr, target, work, work_bytes);
  if (cs != Status::Ok) return cs;
  const Layout l = layout_for(command_count, false);
  outline::Point* opts = at<outline::Point>(work, l.pts);
  u32* oends = at<u32>(work, l.ends);
  std::uint8_t* oclosed = at<std::uint8_t>(work, l.closed);
  u32 cc = 0, pc = 0;
  // Cannot fail: every check of build_outline was passed by common_checks with the same arguments, the capacities are exactly the
  // bounds, and the five regions (commands, three work sections, the two local counters) are disjoint.
  if (outline::build_outline(commands, command_count, tolerance, opts, outline::point_bound(command_count), oends,
                             outline::contour_bound(command_count), oclosed, outline::contour_bound(command_count), &cc,
                             &pc) != outline::Status::Ok)
    return Status::BadPath;  // unreachable
  raster::P* rp = at<raster::P>(work, l.rpts);
  raster::Contour* desc = at<raster::Contour>(work, l.desc);
  std::size_t kept = 0, used = 0;
  u32 begin = 0;
  for (u32 i = 0; i < cc; ++i) {
    const u32 end = oends[i];
    const u32 m = end - begin;
    if (m >= 3) {  // contours under 3 points are skipped
      raster::P* dst = rp + used;
      for (u32 k = 0; k < m; ++k) dst[k] = raster::P{opts[begin + k].x, opts[begin + k].y};
      desc[kept] = raster::Contour{dst, m};
      ++kept;
      used += m;
    }
    begin = end;
  }
  // Cannot fail: target is non-null with a valid size, the rule is valid, kept <= 64 contours of 3 .. 16384 points, and every
  // coordinate is within the raster limit (outline points lie within flatten::kMaxCoord, which equals raster::kMaxCoord).
  return map_raster(raster::fill_polygons(desc, kept, rule, target));
}

Status stroke_path(const outline::Command* commands, std::uint32_t command_count, std::int32_t tolerance,
                   const stroke::Style& style, const raster::Target& target, void* work, std::size_t work_bytes) {
  const Status cs = common_checks(commands, command_count, tolerance, nullptr, &style, target, work, work_bytes);
  if (cs != Status::Ok) return cs;
  const Layout l = layout_for(command_count, true);
  outline::Point* opts = at<outline::Point>(work, l.pts);
  u32* oends = at<u32>(work, l.ends);
  std::uint8_t* oclosed = at<std::uint8_t>(work, l.closed);
  u32 cc = 0, pc = 0;
  // Cannot fail, see fill_path.
  if (outline::build_outline(commands, command_count, tolerance, opts, outline::point_bound(command_count), oends,
                             outline::contour_bound(command_count), oclosed, outline::contour_bound(command_count), &cc,
                             &pc) != outline::Status::Ok)
    return Status::BadPath;  // unreachable
  // First every flattened contour is checked against the stroke input limit, before any stroking.
  {
    u32 begin = 0;
    for (u32 i = 0; i < cc; ++i) {
      if (oends[i] - begin > stroke::kMaxPoints) return Status::TooManyPoints;
      begin = oends[i];
    }
  }
  stroke::Point* sinp = at<stroke::Point>(work, l.sinp);
  stroke::Point* soutp = at<stroke::Point>(work, l.soutp);
  u32* sends = at<u32>(work, l.sends);
  raster::P* rp = at<raster::P>(work, l.rpts);
  raster::Contour* desc = at<raster::Contour>(work, l.desc);
  std::size_t kept = 0, used = 0;
  u32 begin = 0;
  for (u32 i = 0; i < cc; ++i) {
    const u32 end = oends[i];
    const u32 m = end - begin;  // 2 .. 1024
    for (u32 k = 0; k < m; ++k) sinp[k] = stroke::Point{opts[begin + k].x, opts[begin + k].y};
    begin = end;
    stroke::Style st = style;
    st.closed = oclosed[i] != 0;
    u32 scc = 0, spc = 0;
    // Cannot fail: m is 2 .. 1024, the style was validated, every input coordinate is within 2^24 (below stroke::kMaxCoord), the
    // output capacities are the bounds for the largest polyline, and the regions are disjoint.
    if (stroke::stroke_polyline(sinp, m, st, soutp, static_cast<u32>(kStrokeOutPoints), sends, static_cast<u32>(kStrokeOutContours),
                                &scc, &spc) != stroke::Status::Ok)
      return Status::BadPoint;  // unreachable
    for (u32 k = 0; k < spc; ++k)
      if (coord_bad(soutp[k].x) || coord_bad(soutp[k].y)) return Status::BadPoint;
    if (kept + scc > raster::kMaxContours) return Status::TooManyContours;
    u32 sb = 0;
    for (u32 c = 0; c < scc; ++c) {
      const u32 se = sends[c];
      raster::P* dst = rp + used;
      for (u32 k = sb; k < se; ++k) dst[k - sb] = raster::P{soutp[k].x, soutp[k].y};
      desc[kept] = raster::Contour{dst, se - sb};
      ++kept;
      used += se - sb;
      sb = se;
    }
  }
  // Cannot fail: kept <= 4096 contours of 3 or 4 points, all coordinates were checked above, the rule is NonZero, and the target
  // was validated.
  return map_raster(raster::fill_polygons(desc, kept, raster::Fill::NonZero, target));
}

}  // namespace lumora::vecdraw
