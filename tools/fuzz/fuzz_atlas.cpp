// Fuzz target: atlas. Builds a few tiny images from the input bytes and packs them with small limits. On success the
// atlas image must be valid and inside the limits, every entry must lie inside the image, entries must not overlap, and
// the JSON index must be non-empty. On failure the index must be empty. Anything else aborts.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "atlas.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using namespace lumora::image;
  if (size < 4) return 0;
  std::size_t pos = 0;
  auto next = [&]() -> std::uint8_t { return data[pos++ % size]; };
  AtlasLimits limits;
  limits.max_size = 1u + (next() % 96u);
  limits.padding = next() % 5u;
  limits.power_of_two = (next() & 1u) != 0;
  limits.max_inputs = 1u + (next() % 8u);
  const std::size_t count = next() % 10u;  // may exceed max_inputs on purpose
  std::vector<AtlasInput> inputs;
  for (std::size_t i = 0; i < count; ++i) {
    AtlasInput in;
    in.name = (next() % 16u == 0) ? std::string() : "s" + std::to_string(next() % 12u);  // duplicates happen
    const std::uint32_t w = next() % 20u, h = next() % 20u;  // zero sizes give invalid images on purpose
    in.image.width = w;
    in.image.height = h;
    in.image.rgba.resize(std::size_t{w} * h * 4);
    for (auto& b : in.image.rgba) b = next();
    inputs.push_back(std::move(in));
  }
  const AtlasResult r = pack_atlas(inputs, limits);
  const std::string json = atlas_index_json(r);
  if (!r.ok()) {
    if (!json.empty()) std::abort();
    return 0;
  }
  if (!r.image.valid() || json.empty()) std::abort();
  if (r.image.width > limits.max_size || r.image.height > limits.max_size) std::abort();
  if (r.entries.size() != inputs.size()) std::abort();
  for (std::size_t i = 0; i < r.entries.size(); ++i) {
    const AtlasEntry& a = r.entries[i];
    if (a.w != inputs[i].image.width || a.h != inputs[i].image.height) std::abort();
    if (std::uint64_t{a.x} + a.w > r.image.width || std::uint64_t{a.y} + a.h > r.image.height) std::abort();
    for (std::size_t j = i + 1; j < r.entries.size(); ++j) {
      const AtlasEntry& b = r.entries[j];
      const bool apart = a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y;
      if (!apart) std::abort();
    }
  }
  return 0;
}
