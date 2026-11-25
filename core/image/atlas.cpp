// Placeholder for the atlas packer. The contributor replaces this file; the interface is core/image/atlas.h.
#include "atlas.h"

namespace lumora::image {

AtlasResult pack_atlas(const std::vector<AtlasInput>&, const AtlasLimits&) {
  AtlasResult r;
  r.status = AtlasStatus::BadInput;
  r.message = "not implemented";
  return r;
}

std::string atlas_index_json(const AtlasResult&) { return {}; }

}  // namespace lumora::image
