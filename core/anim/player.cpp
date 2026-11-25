// Clip player: binds animation tracks to scene nodes and evaluates a clip into a transformed scene copy.
// Rules, check order and cost: docs/animation-player.md.
// Every walk over the node tree uses an explicit stack, so a very deep scene cannot overflow the call stack
// (the default copy of a Scene is recursive, so evaluate_clip copies with its own iterative copy).
#include "player.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace lumora::anim {
namespace {

enum class Prop : std::uint8_t { Position = 0, Scale = 1, RotationZ = 2 };

struct PropInfo {
  std::string_view name;
  Prop prop;
  int components;
};
constexpr PropInfo kProps[] = {
    {"position", Prop::Position, 3},
    {"scale", Prop::Scale, 3},
    {"rotation_z_degrees", Prop::RotationZ, 1},
};

struct Resolved {
  std::size_t node_index = 0;  // pre-order index of the node
  Prop prop = Prop::Position;
  float value[4] = {0, 0, 0, 0};  // sampled value, only used by evaluate_clip
};

BindResult make(BindStatus status, std::size_t index, const char* message) {
  BindResult r;
  r.status = status;
  r.track_index = index;
  r.message = message;
  return r;
}

const char* status_message(BindStatus s) {
  switch (s) {
    case BindStatus::Ok: return "ok";
    case BindStatus::BadClip: return "a track failed validation";
    case BindStatus::BadTarget: return "target needs a node id, a slash and a property";
    case BindStatus::UnknownNode: return "no node has that id";
    case BindStatus::UnknownProperty: return "unknown property";
    case BindStatus::ComponentMismatch: return "wrong component count for the property";
    case BindStatus::DuplicateTarget: return "two tracks drive the same node and property";
    case BindStatus::UnsupportedTrack: return "rotation tracks are not supported";
  }
  return "unknown status";
}

BindResult fail(BindStatus s, std::size_t index) { return make(s, index, status_message(s)); }

// Checks every track in order. When `loop_time` is given, each track is also sampled (sample_track validates the
// track, so it doubles as the BadClip check and the track is validated only once).
BindResult resolve(const scene::Scene& scene, const Clip& clip, const float* loop_time, std::vector<Resolved>* out) {
  // Node lookup, built once in one pre-order walk: only the node ids that tracks name are kept, the first
  // occurrence of each id wins. Cost: O(total nodes) hash lookups, with no allocation per node.
  constexpr std::size_t kUnresolved = static_cast<std::size_t>(-1);
  std::unordered_map<std::string_view, std::size_t> lookup;
  for (const Track& t : clip.tracks) {
    const std::size_t slash = t.target.rfind('/');
    if (slash != std::string::npos && slash != 0 && slash + 1 != t.target.size()) {
      lookup.emplace(std::string_view(t.target.data(), slash), kUnresolved);
    }
  }
  std::size_t unresolved = lookup.size();
  if (unresolved > 0) {
    std::vector<const scene::Node*> stack;
    stack.push_back(&scene.root);
    std::size_t index = 0;
    while (!stack.empty() && unresolved > 0) {
      const scene::Node* n = stack.back();
      stack.pop_back();
      const auto it = lookup.find(std::string_view(n->id));
      if (it != lookup.end() && it->second == kUnresolved) {
        it->second = index;
        --unresolved;
      }
      ++index;
      for (std::size_t i = n->children.size(); i > 0; --i) stack.push_back(&n->children[i - 1]);
    }
  }

  std::unordered_set<std::uint64_t> seen;
  if (out != nullptr) out->reserve(clip.tracks.size());
  for (std::size_t i = 0; i < clip.tracks.size(); ++i) {
    const Track& t = clip.tracks[i];
    Resolved res;
    // 1. BadClip
    if (loop_time != nullptr) {
      if (!sample_track(t, *loop_time, res.value)) return fail(BindStatus::BadClip, i);
    } else if (validate_track(t) != TrackStatus::Ok) {
      return fail(BindStatus::BadClip, i);
    }
    // 2. BadTarget: split at the last slash
    const std::size_t slash = t.target.rfind('/');
    if (slash == std::string::npos || slash == 0 || slash + 1 == t.target.size()) return fail(BindStatus::BadTarget, i);
    const std::string_view node_id(t.target.data(), slash);
    const std::string_view prop_name(t.target.data() + slash + 1, t.target.size() - slash - 1);
    // 3. UnknownProperty
    const PropInfo* info = nullptr;
    for (const PropInfo& p : kProps) {
      if (p.name == prop_name) info = &p;
    }
    if (info == nullptr) return fail(BindStatus::UnknownProperty, i);
    // 4. UnsupportedTrack (before ComponentMismatch: a rotation track always has 4 components)
    if (t.is_rotation) return fail(BindStatus::UnsupportedTrack, i);
    // 5. ComponentMismatch
    if (t.components != info->components) return fail(BindStatus::ComponentMismatch, i);
    // 6. UnknownNode
    const auto it = lookup.find(node_id);
    if (it == lookup.end() || it->second == kUnresolved) return fail(BindStatus::UnknownNode, i);
    // 7. DuplicateTarget
    const std::uint64_t key = static_cast<std::uint64_t>(it->second) * 4u + static_cast<std::uint64_t>(info->prop);
    if (!seen.insert(key).second) return fail(BindStatus::DuplicateTarget, i);

    res.node_index = it->second;
    res.prop = info->prop;
    if (out != nullptr) out->push_back(res);
  }
  return make(BindStatus::Ok, 0, status_message(BindStatus::Ok));
}

// Copies a scene without recursion. Only the fields of scene::Node are copied one by one, because a plain Node copy
// would copy the children recursively.
void copy_node_fields(const scene::Node& src, scene::Node& dst) {
  dst.id = src.id;
  dst.name = src.name;
  dst.transform = src.transform;
  dst.layer_asset = src.layer_asset;
  dst.parallax_depth = src.parallax_depth;
}

// `order` receives the copied nodes in pre-order, so index i is the node with pre-order index i in `src`.
void copy_scene(const scene::Scene& src, scene::Scene& dst, std::vector<scene::Node*>& order) {
  dst.assets = src.assets;
  copy_node_fields(src.root, dst.root);
  std::vector<std::pair<const scene::Node*, scene::Node*>> stack;
  stack.emplace_back(&src.root, &dst.root);
  while (!stack.empty()) {
    const std::pair<const scene::Node*, scene::Node*> p = stack.back();
    stack.pop_back();
    order.push_back(p.second);
    p.second->children.resize(p.first->children.size());  // sized once, never reallocated afterwards
    for (std::size_t i = p.first->children.size(); i > 0; --i) {
      copy_node_fields(p.first->children[i - 1], p.second->children[i - 1]);
      stack.emplace_back(&p.first->children[i - 1], &p.second->children[i - 1]);
    }
  }
}

float finite_or_zero(float v) { return std::isfinite(v) ? v : 0.0f; }

}  // namespace

BindResult bind_clip(const scene::Scene& scene, const Clip& clip) { return resolve(scene, clip, nullptr, nullptr); }

bool evaluate_clip(const scene::Scene& base, const Clip& clip, float time, Loop mode, scene::Scene& out, BindResult& why) {
  // clip_duration would validate every track again. The longest last key time is the same number whenever every
  // track is valid, and when one is not the evaluation fails below, so this value is never used for a result.
  float duration = 0.0f;
  for (const Track& t : clip.tracks) {
    if (!t.times.empty() && std::isfinite(t.times.back()) && t.times.back() > duration) duration = t.times.back();
  }
  const float loop_time = apply_loop(time, duration, mode);

  std::vector<Resolved> resolved;
  const BindResult r = resolve(base, clip, &loop_time, &resolved);
  if (!r.ok()) {
    out = scene::Scene{};
    why = r;
    return false;
  }

  scene::Scene result;  // built aside so that `out` may be the same object as `base`
  std::vector<scene::Node*> nodes;
  copy_scene(base, result, nodes);
  for (const Resolved& res : resolved) {
    if (res.node_index >= nodes.size()) continue;  // cannot happen: same shape as base
    scene::Transform& tf = nodes[res.node_index]->transform;
    switch (res.prop) {
      case Prop::Position:
        for (int c = 0; c < 3; ++c) tf.position[c] = finite_or_zero(res.value[c]);
        break;
      case Prop::Scale:
        for (int c = 0; c < 3; ++c) tf.scale[c] = finite_or_zero(res.value[c]);
        break;
      case Prop::RotationZ:
        tf.rotation_z_degrees = finite_or_zero(res.value[0]);
        break;
    }
  }
  out = std::move(result);
  why = r;
  return true;
}

}  // namespace lumora::anim
