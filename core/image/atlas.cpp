// Texture atlas packer: skyline bottom-left with a documented candidate-width search. Rules: docs/atlas.md.
// Everything is integer arithmetic and total orders, so the output is byte-identical on every host and compiler.
#include "atlas.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>

namespace lumora::image {
namespace {

constexpr std::uint32_t kMaxSizeLimit = 16384;
constexpr std::uint32_t kMaxPadding = 16;
constexpr std::size_t kMaxNameBytes = 128;

AtlasResult fail(AtlasStatus status, const char* message) {
  AtlasResult r;
  r.status = status;
  r.message = message;
  return r;
}

// Valid UTF-8 (no overlong forms, no surrogates, nothing above U+10FFFF), and no control characters (< 0x20, 0x7F).
bool valid_name(const std::string& s) {
  if (s.empty() || s.size() > kMaxNameBytes) return false;
  std::size_t i = 0;
  while (i < s.size()) {
    const unsigned c = static_cast<unsigned char>(s[i]);
    if (c < 0x20 || c == 0x7F) return false;
    if (c < 0x80) {
      ++i;
      continue;
    }
    std::size_t extra;
    unsigned cp;
    if (c >= 0xC2 && c <= 0xDF) {
      extra = 1;
      cp = c & 0x1F;
    } else if (c >= 0xE0 && c <= 0xEF) {
      extra = 2;
      cp = c & 0x0F;
    } else if (c >= 0xF0 && c <= 0xF4) {
      extra = 3;
      cp = c & 0x07;
    } else {
      return false;
    }
    if (i + extra >= s.size()) return false;
    for (std::size_t k = 1; k <= extra; ++k) {
      const unsigned cc = static_cast<unsigned char>(s[i + k]);
      if ((cc & 0xC0) != 0x80) return false;
      cp = (cp << 6) | (cc & 0x3F);
    }
    if (extra == 2 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))) return false;
    if (extra == 3 && (cp < 0x10000 || cp > 0x10FFFF)) return false;
    i += extra + 1;
  }
  return true;
}

std::uint64_t ceil_sqrt(std::uint64_t v) {
  std::uint64_t r = 0;
  while (r * r < v) ++r;  // v is at most max_size^2 here (2^28), so r stays below 2^14 + 1
  return r;
}

std::uint32_t ceil_pow2(std::uint32_t v) {
  std::uint32_t p = 1;
  while (p < v) p <<= 1;
  return p;
}

std::uint32_t floor_pow2(std::uint32_t v) {
  std::uint32_t p = 1;
  while ((p << 1) <= v && (p << 1) != 0) p <<= 1;
  return p;
}

struct Cell {
  std::uint32_t w, h;  // sprite size plus the padding ring on both sides
  std::uint32_t x = 0, y = 0;
};

struct Segment {
  std::uint32_t x, y, w;
};

struct Trial {
  bool fits = false;
  std::uint32_t used_w = 0, used_h = 0;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> pos;  // per sorted cell
};

// Skyline bottom-left into a strip `width` wide and at most `height_cap` high.
// A cell goes where its top edge is lowest; ties take the smallest x. Segments are scanned left to right.
bool pack_trial(const std::vector<Cell>& cells, const std::vector<std::size_t>& order, std::uint32_t width,
                std::uint32_t height_cap, Trial& out) {
  std::vector<Segment> sky;
  sky.push_back({0, 0, width});
  out.pos.assign(cells.size(), {0, 0});
  out.used_w = out.used_h = 0;
  for (const std::size_t idx : order) {
    const Cell& c = cells[idx];
    if (c.w > width || c.h > height_cap) return false;
    bool found = false;
    std::uint32_t best_x = 0, best_y = 0;
    for (std::size_t i = 0; i < sky.size(); ++i) {
      const std::uint32_t x = sky[i].x;
      if (static_cast<std::uint64_t>(x) + c.w > width) break;  // later segments start further right
      std::uint32_t y = 0;
      std::uint64_t covered = 0;
      for (std::size_t j = i; j < sky.size() && covered < c.w; ++j) {
        y = std::max(y, sky[j].y);
        covered += sky[j].w;
      }
      if (static_cast<std::uint64_t>(y) + c.h > height_cap) continue;
      if (!found || y < best_y) {  // equal y keeps the earlier (smaller) x
        found = true;
        best_x = x;
        best_y = y;
      }
    }
    if (!found) return false;
    out.pos[idx] = {best_x, best_y};
    out.used_w = std::max(out.used_w, best_x + c.w);
    out.used_h = std::max(out.used_h, best_y + c.h);
    // Raise the skyline over [best_x, best_x + c.w).
    std::vector<Segment> next;
    next.reserve(sky.size() + 1);
    const std::uint32_t right = best_x + c.w;
    for (const Segment& s : sky) {
      const std::uint32_t s_right = s.x + s.w;
      if (s_right <= best_x) {
        next.push_back(s);
      } else if (s.x >= right) {
        next.push_back(s);
      } else {
        if (s.x < best_x) next.push_back({s.x, s.y, best_x - s.x});  // cannot happen: best_x is a segment start
        if (s_right > right) next.push_back({right, s.y, s_right - right});
      }
    }
    // Insert the new segment in x order.
    std::size_t at = 0;
    while (at < next.size() && next[at].x < best_x) ++at;
    next.insert(next.begin() + static_cast<std::ptrdiff_t>(at), Segment{best_x, best_y + c.h, c.w});
    // Merge neighbours with the same height.
    sky.clear();
    for (const Segment& s : next) {
      if (!sky.empty() && sky.back().y == s.y && sky.back().x + sky.back().w == s.x) {
        sky.back().w += s.w;
      } else {
        sky.push_back(s);
      }
    }
  }
  return true;
}

void put_json_string(std::string& out, const std::string& s) {
  static const char* hex = "0123456789abcdef";
  out.push_back('"');
  for (const char ch : s) {
    const unsigned char c = static_cast<unsigned char>(ch);
    if (c == '"') {
      out += "\\\"";
    } else if (c == '\\') {
      out += "\\\\";
    } else if (c < 0x20 || c == 0x7F) {
      out += "\\u00";
      out.push_back(hex[c >> 4]);
      out.push_back(hex[c & 15]);
    } else {
      out.push_back(ch);
    }
  }
  out.push_back('"');
}

}  // namespace

AtlasResult pack_atlas(const std::vector<AtlasInput>& inputs, const AtlasLimits& limits) {
  if (limits.max_size < 1 || limits.max_size > kMaxSizeLimit) return fail(AtlasStatus::BadLimits, "max_size must be 1 to 16384");
  if (limits.padding > kMaxPadding) return fail(AtlasStatus::BadLimits, "padding must be 0 to 16");
  if (limits.max_inputs < 1) return fail(AtlasStatus::BadLimits, "max_inputs must be at least 1");
  if (inputs.empty()) return fail(AtlasStatus::NoInputs, "no inputs");
  if (inputs.size() > limits.max_inputs) return fail(AtlasStatus::TooManyInputs, "more inputs than max_inputs");

  // Names and images first, so a bad input is reported before any fit question.
  std::vector<const std::string*> names;
  names.reserve(inputs.size());
  for (const AtlasInput& in : inputs) {
    if (!valid_name(in.name)) return fail(AtlasStatus::BadInput, "bad name");
    if (!in.image.valid()) return fail(AtlasStatus::BadInput, "invalid image");
    names.push_back(&in.name);
  }
  {
    std::vector<const std::string*> sorted = names;
    std::sort(sorted.begin(), sorted.end(), [](const std::string* a, const std::string* b) { return *a < *b; });
    for (std::size_t i = 1; i < sorted.size(); ++i)
      if (*sorted[i] == *sorted[i - 1]) return fail(AtlasStatus::BadInput, "duplicate name");
  }

  const std::uint32_t pad = limits.padding;
  const std::uint32_t cap = limits.power_of_two ? floor_pow2(limits.max_size) : limits.max_size;
  const std::uint64_t cap_area = std::uint64_t{cap} * cap;

  // Cell sizes. Each side is checked against the cap before any multiplication, so nothing can overflow.
  std::vector<Cell> cells(inputs.size());
  std::uint64_t area = 0;
  std::uint32_t min_w = 1;
  for (std::size_t i = 0; i < inputs.size(); ++i) {
    const std::uint64_t cw = std::uint64_t{inputs[i].image.width} + 2ull * pad;
    const std::uint64_t ch = std::uint64_t{inputs[i].image.height} + 2ull * pad;
    if (cw > cap || ch > cap) return fail(AtlasStatus::DoesNotFit, "a sprite with its padding is larger than max_size");
    cells[i].w = static_cast<std::uint32_t>(cw);
    cells[i].h = static_cast<std::uint32_t>(ch);
    area += cw * ch;  // at most 2^28 per cell; the check below stops the sum long before 2^64
    if (area > cap_area) return fail(AtlasStatus::DoesNotFit, "the sprites do not fit inside max_size");
    min_w = std::max(min_w, cells[i].w);
  }

  // Placement order: height descending, width descending, input index ascending (a total order).
  std::vector<std::size_t> order(cells.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    if (cells[a].h != cells[b].h) return cells[a].h > cells[b].h;
    if (cells[a].w != cells[b].w) return cells[a].w > cells[b].w;
    return a < b;
  });

  // Candidate strip widths, ascending and distinct.
  std::vector<std::uint32_t> widths;
  if (limits.power_of_two) {
    for (std::uint32_t w = ceil_pow2(min_w); w <= cap; w <<= 1) {
      widths.push_back(w);
      if (w > (std::numeric_limits<std::uint32_t>::max() >> 1)) break;
    }
  } else {
    const std::uint64_t s = ceil_sqrt(area);
    static const std::uint32_t kFactor[][2] = {{1, 1}, {21, 20}, {11, 10}, {6, 5}, {27, 20}, {3, 2}, {7, 4}, {2, 1}, {3, 1}, {4, 1}};
    widths.push_back(min_w);
    widths.push_back(cap);
    for (const auto& f : kFactor) {
      const std::uint64_t w = (s * f[0] + f[1] - 1) / f[1];
      widths.push_back(static_cast<std::uint32_t>(std::min<std::uint64_t>(std::max<std::uint64_t>(w, min_w), cap)));
    }
    std::sort(widths.begin(), widths.end());
    widths.erase(std::unique(widths.begin(), widths.end()), widths.end());
  }

  bool have_best = false;
  std::uint64_t best_area = 0;
  std::uint32_t best_w = 0, best_h = 0;
  Trial best;
  Trial trial;
  for (const std::uint32_t w : widths) {
    if (!pack_trial(cells, order, w, cap, trial)) continue;
    const std::uint32_t fw = limits.power_of_two ? ceil_pow2(trial.used_w) : trial.used_w;
    const std::uint32_t fh = limits.power_of_two ? ceil_pow2(trial.used_h) : trial.used_h;
    const std::uint64_t a = std::uint64_t{fw} * fh;
    const std::uint32_t side = std::max(fw, fh);
    // Smaller area wins, then the smaller longest side, then the narrower strip (it comes first in `widths`).
    if (!have_best || a < best_area || (a == best_area && side < std::max(best_w, best_h))) {
      have_best = true;
      best_area = a;
      best_w = fw;
      best_h = fh;
      best = trial;
    }
  }
  if (!have_best) return fail(AtlasStatus::DoesNotFit, "no candidate strip width fits the sprites inside max_size");

  AtlasResult result;
  result.image.width = best_w;
  result.image.height = best_h;
  try {
    result.image.rgba.assign(static_cast<std::size_t>(best_w) * best_h * 4, 0);
    result.entries.resize(inputs.size());
  } catch (const std::bad_alloc&) {
    return fail(AtlasStatus::DoesNotFit, "not enough memory for the atlas image");
  }
  for (std::size_t i = 0; i < inputs.size(); ++i) {
    const Rgba8& src = inputs[i].image;
    const std::uint32_t cx = best.pos[i].first, cy = best.pos[i].second;
    result.entries[i].name = inputs[i].name;
    result.entries[i].x = cx + pad;
    result.entries[i].y = cy + pad;
    result.entries[i].w = src.width;
    result.entries[i].h = src.height;
    // The cell: the sprite with its border pixels extruded into a ring of `pad` pixels.
    for (std::uint32_t y = 0; y < cells[i].h; ++y) {
      const std::uint32_t sy = y < pad ? 0 : std::min(y - pad, src.height - 1);
      std::uint8_t* dst = result.image.rgba.data() + (static_cast<std::size_t>(cy + y) * best_w + cx) * 4;
      const std::uint8_t* row = src.rgba.data() + static_cast<std::size_t>(sy) * src.width * 4;
      for (std::uint32_t x = 0; x < cells[i].w; ++x) {
        const std::uint32_t sx = x < pad ? 0 : std::min(x - pad, src.width - 1);
        std::memcpy(dst + static_cast<std::size_t>(x) * 4, row + static_cast<std::size_t>(sx) * 4, 4);
      }
    }
  }
  result.status = AtlasStatus::Ok;
  result.message = "ok";
  return result;
}

std::string atlas_index_json(const AtlasResult& result) {
  if (!result.ok()) return {};
  std::string out = "{\"width\":" + std::to_string(result.image.width) + ",\"height\":" + std::to_string(result.image.height) +
                    ",\"entries\":[";
  for (std::size_t i = 0; i < result.entries.size(); ++i) {
    const AtlasEntry& e = result.entries[i];
    if (i > 0) out.push_back(',');
    out += "{\"name\":";
    put_json_string(out, e.name);
    out += ",\"x\":" + std::to_string(e.x) + ",\"y\":" + std::to_string(e.y) + ",\"w\":" + std::to_string(e.w) +
           ",\"h\":" + std::to_string(e.h) + "}";
  }
  out += "]}";
  return out;
}

}  // namespace lumora::image
