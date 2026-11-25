// Clip player: binds animation tracks to scene nodes and evaluates a clip at a time (milestone M4 groundwork).
// Pure CPU, no GPU, no file system. Built on animation.h (sampling and loop modes) and the scene model in scene.h.
// Total: no exception, no out-of-bounds access, no NaN or infinity in any output transform.
// Exact rules (target syntax, error texts, order of checks) are written by the contributor in docs/animation-player.md.
#pragma once
#include <cstddef>
#include <string>

#include "animation.h"
#include "scene.h"

namespace lumora::anim {

// A track target is "<node_id>/<property>" and splits at the LAST '/', so node ids may contain '/'.
// Properties: "position" (3 components), "scale" (3 components), "rotation_z_degrees" (1 component).
// Rotation quaternion tracks (is_rotation) are not bound in this draft: they are rejected as UnsupportedTrack.
enum class BindStatus {
  Ok,
  BadClip,           // a track fails validate_track
  BadTarget,         // no '/', empty node id or empty property
  UnknownNode,       // no node with that id in the scene
  UnknownProperty,   // the property name is not one of the three above
  ComponentMismatch, // the property needs a different component count
  DuplicateTarget,   // two tracks of the clip drive the same node and property
  UnsupportedTrack   // is_rotation tracks
};

struct BindResult {
  BindStatus status = BindStatus::Ok;
  std::size_t track_index = 0;  // the first track that failed, when status != Ok
  std::string message;          // short fixed text, never input bytes
  bool ok() const { return status == BindStatus::Ok; }
};

// Checks that every track of the clip can drive a node of the scene. Does not change anything.
BindResult bind_clip(const scene::Scene& scene, const Clip& clip);

// Evaluates the clip at an absolute time and returns a copy of `base` whose node transforms are overridden by the
// tracks. `time` goes through apply_loop(time, clip_duration(clip), mode). Properties without a track keep their
// base value. Returns false and leaves `out` empty when bind_clip is not Ok (the BindResult is returned in `why`).
bool evaluate_clip(const scene::Scene& base, const Clip& clip, float time, Loop mode, scene::Scene& out, BindResult& why);

}  // namespace lumora::anim
