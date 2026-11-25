// Placeholder for the clip player. The contributor replaces this file; the interface is core/anim/player.h.
#include "player.h"

namespace lumora::anim {

BindResult bind_clip(const scene::Scene&, const Clip&) {
  BindResult r;
  r.status = BindStatus::BadClip;
  r.message = "not implemented";
  return r;
}

bool evaluate_clip(const scene::Scene&, const Clip&, float, Loop, scene::Scene& out, BindResult& why) {
  out = scene::Scene{};
  why = bind_clip({}, {});
  return false;
}

}  // namespace lumora::anim
