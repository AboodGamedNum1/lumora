// Texture atlas packer: packs many small RGBA images into one RGBA image and describes where each went.
// Pure CPU, no GPU and no file system. Deterministic: the same inputs and limits give the same bytes on every host.
// Inputs come from the authoring side (the editor and the atlas tool), but the packer still validates everything:
// sizes are checked in 64-bit arithmetic against the limits before any allocation, and failures return a status.
// Exact rules (ordering, tie-breaks, padding) are written by the contributor in docs/atlas.md.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "png.h"

namespace lumora::image {

struct AtlasInput {
  std::string name;  // unique, 1 to 128 bytes, no control characters
  Rgba8 image;       // must be valid()
};

struct AtlasLimits {
  std::uint32_t max_size = 4096;  // atlas width and height cap, 1 to 16384
  std::uint32_t padding = 2;      // pixels of edge extrusion around every sprite, 0 to 16
  bool power_of_two = false;      // round the atlas width and height up to powers of two
  std::size_t max_inputs = 4096;
};

struct AtlasEntry {
  std::string name;
  std::uint32_t x = 0, y = 0, w = 0, h = 0;  // the sprite rectangle without padding, in atlas pixels
};

enum class AtlasStatus {
  Ok,
  NoInputs,
  TooManyInputs,
  BadLimits,
  BadInput,      // invalid image, bad or duplicate name
  DoesNotFit     // the inputs cannot be placed inside max_size
};

struct AtlasResult {
  AtlasStatus status = AtlasStatus::BadInput;
  std::string message;
  Rgba8 image;
  std::vector<AtlasEntry> entries;  // in input order
  bool ok() const { return status == AtlasStatus::Ok; }
};

AtlasResult pack_atlas(const std::vector<AtlasInput>& inputs, const AtlasLimits& limits = {});

// Deterministic JSON index: {"width":W,"height":H,"entries":[{"name":...,"x":...,"y":...,"w":...,"h":...}]}.
// Names are escaped as JSON strings. Entries keep input order. Returns an empty string unless result.ok().
std::string atlas_index_json(const AtlasResult& result);

}  // namespace lumora::image
