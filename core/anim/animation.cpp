// Placeholder for the animation module. The contributor replaces this file; the interface is core/anim/animation.h.
#include "animation.h"

namespace lumora::anim {

TrackStatus validate_track(const Track&) { return TrackStatus::NoKeys; }
const char* track_status_text(TrackStatus) { return "not implemented"; }
float track_duration(const Track&) { return 0.0f; }
float clip_duration(const Clip&) { return 0.0f; }
float apply_loop(float, float, Loop) { return 0.0f; }
bool sample_track(const Track&, float, float*) { return false; }
ParseClipResult parse_clip_json(const std::string&) {
  ParseClipResult r;
  r.message = "not implemented";
  return r;
}

}  // namespace lumora::anim
