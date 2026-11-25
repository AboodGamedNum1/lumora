// Tests for core/anim/player.cpp (bind_clip, evaluate_clip). Pure CPU, no files.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "animation.h"
#include "player.h"
#include "scene.h"

using Catch::Approx;
using namespace lumora::anim;
using lumora::scene::Node;
using lumora::scene::Scene;

namespace {

constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

Node node(const std::string& id) {
  Node n;
  n.id = id;
  n.name = "name of " + id;
  return n;
}

Track linear(const std::string& target, int comps, std::vector<float> times, std::vector<float> values) {
  Track t;
  t.target = target;
  t.components = comps;
  t.interp = Interp::Linear;
  t.times = std::move(times);
  t.values = std::move(values);
  return t;
}

// root -> a -> (a1, a2), root -> b
Scene small_scene() {
  Scene s;
  s.root = node("root");
  Node a = node("a");
  a.children.push_back(node("a1"));
  a.children.push_back(node("a2"));
  s.root.children.push_back(a);
  s.root.children.push_back(node("b"));
  return s;
}

const Node* find(const Node& n, const std::string& id) {
  if (n.id == id) return &n;
  for (const Node& c : n.children) {
    if (const Node* f = find(c, id)) return f;
  }
  return nullptr;
}

void count_nodes(const Node& n, std::size_t& total) {
  ++total;
  for (const Node& c : n.children) count_nodes(c, total);
}

// Byte image of a scene in pre-order, built without recursion. Used to compare scenes bit for bit.
std::string image(const Scene& s) {
  std::string out;
  for (const auto& a : s.assets) out += "A:" + a.id + "|" + a.kind + "|" + a.path + "\n";
  std::vector<const Node*> stack{&s.root};
  while (!stack.empty()) {
    const Node* n = stack.back();
    stack.pop_back();
    out += "N:" + n->id + "|" + n->name + "|" + n->layer_asset + "|" + std::to_string(n->children.size()) + "|";
    out.append(reinterpret_cast<const char*>(&n->transform), sizeof(float) * 7);  // 3 + 3 + 1 floats, no padding
    out.append(reinterpret_cast<const char*>(&n->parallax_depth), sizeof(float));
    out += "\n";
    for (std::size_t i = n->children.size(); i > 0; --i) stack.push_back(&n->children[i - 1]);
  }
  return out;
}

}  // namespace

TEST_CASE("Transform has no padding (the byte image in these tests relies on it)", "[anim][player]") {
  CHECK(sizeof(lumora::scene::Transform) == sizeof(float) * 7);
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("bind_clip: Ok and every status", "[anim][player][bind]") {
  const Scene s = small_scene();
  Clip c;
  CHECK(bind_clip(s, c).ok());  // an empty clip binds
  CHECK(bind_clip(s, c).message == "ok");

  c.tracks.push_back(linear("a/position", 3, {0, 1}, {0, 0, 0, 1, 1, 1}));
  c.tracks.push_back(linear("a1/scale", 3, {0, 1}, {1, 1, 1, 2, 2, 2}));
  c.tracks.push_back(linear("b/rotation_z_degrees", 1, {0, 1}, {0, 90}));
  BindResult r = bind_clip(s, c);
  CHECK(r.ok());
  CHECK(r.status == BindStatus::Ok);

  auto first_failure = [&](Track bad, std::size_t at = 99) {
    Clip x;
    x.tracks.push_back(linear("a/position", 3, {0, 1}, {0, 0, 0, 1, 1, 1}));
    x.tracks.push_back(std::move(bad));
    BindResult res = bind_clip(s, x);
    if (at != 99) x.tracks.insert(x.tracks.begin(), at, linear("b/scale", 3, {0}, {1, 1, 1}));
    return res;
  };

  // BadClip
  BindResult bad = first_failure(linear("a/position", 3, {0, 1}, {0, 0, 0}));  // wrong value count
  CHECK(bad.status == BindStatus::BadClip);
  CHECK(bad.track_index == 1);
  // BadTarget
  for (const char* target : {"nope", "/position", "a/", "", "/", "a"}) {
    INFO(target);
    bad = first_failure(linear(target, 3, {0}, {0, 0, 0}));
    if (std::string(target).empty()) {
      CHECK(bad.status == BindStatus::BadClip);  // an empty target already fails validate_track
    } else {
      CHECK(bad.status == BindStatus::BadTarget);
    }
    CHECK(bad.track_index == 1);
  }
  // UnknownProperty
  for (const char* target : {"a/rotation", "a/Position", "a/position ", "a/pos", "a/position/x2", "a/x"}) {
    INFO(target);
    bad = first_failure(linear(target, 3, {0}, {0, 0, 0}));
    CHECK(bad.status == BindStatus::UnknownProperty);
  }
  // ComponentMismatch
  bad = first_failure(linear("a/position", 2, {0}, {0, 0}));
  CHECK(bad.status == BindStatus::ComponentMismatch);
  bad = first_failure(linear("a/scale", 1, {0}, {1}));
  CHECK(bad.status == BindStatus::ComponentMismatch);
  bad = first_failure(linear("a/rotation_z_degrees", 3, {0}, {0, 0, 0}));
  CHECK(bad.status == BindStatus::ComponentMismatch);
  bad = first_failure(linear("a/rotation_z_degrees", 4, {0}, {0, 0, 0, 1}));
  CHECK(bad.status == BindStatus::ComponentMismatch);
  // UnknownNode
  bad = first_failure(linear("zzz/position", 3, {0}, {0, 0, 0}));
  CHECK(bad.status == BindStatus::UnknownNode);
  bad = first_failure(linear("A/position", 3, {0}, {0, 0, 0}));  // ids are case sensitive
  CHECK(bad.status == BindStatus::UnknownNode);
  // UnsupportedTrack
  Track rot = linear("a/rotation_z_degrees", 4, {0}, {0, 0, 0, 1});
  rot.is_rotation = true;
  bad = first_failure(rot);
  CHECK(bad.status == BindStatus::UnsupportedTrack);
  rot.target = "a/position";
  bad = first_failure(rot);
  CHECK(bad.status == BindStatus::UnsupportedTrack);
  // DuplicateTarget
  bad = first_failure(linear("a/position", 3, {0}, {5, 5, 5}));
  CHECK(bad.status == BindStatus::DuplicateTarget);
  CHECK(bad.track_index == 1);
  bad = first_failure(linear("a/scale", 3, {0}, {1, 1, 1}));
  CHECK(bad.status == BindStatus::Ok);  // different property of the same node is fine

  // Messages are fixed, short and distinct per status.
  std::vector<std::string> msgs;
  for (BindStatus st : {BindStatus::BadClip, BindStatus::BadTarget, BindStatus::UnknownNode, BindStatus::UnknownProperty,
                        BindStatus::ComponentMismatch, BindStatus::DuplicateTarget, BindStatus::UnsupportedTrack}) {
    Clip x;
    Track t;
    switch (st) {
      case BindStatus::BadClip: t = linear("a/position", 3, {}, {}); break;
      case BindStatus::BadTarget: t = linear("a", 3, {0}, {0, 0, 0}); break;
      case BindStatus::UnknownNode: t = linear("q/position", 3, {0}, {0, 0, 0}); break;
      case BindStatus::UnknownProperty: t = linear("a/q", 3, {0}, {0, 0, 0}); break;
      case BindStatus::ComponentMismatch: t = linear("a/position", 1, {0}, {0}); break;
      case BindStatus::UnsupportedTrack: t = linear("a/position", 4, {0}, {0, 0, 0, 1}); t.is_rotation = true; break;
      default: x.tracks.push_back(linear("a/position", 3, {0}, {0, 0, 0})); t = x.tracks[0]; break;
    }
    x.tracks.push_back(t);
    const BindResult res = bind_clip(s, x);
    CHECK(res.status == st);
    CHECK_FALSE(res.message.empty());
    CHECK(res.message.size() < 60);
    for (const std::string& m : msgs) CHECK(m != res.message);
    msgs.push_back(res.message);
  }
}

TEST_CASE("bind_clip: per-track check order", "[anim][player][bind]") {
  const Scene s = small_scene();
  auto status_of = [&](Track t) {
    Clip c;
    c.tracks.push_back(std::move(t));
    return bind_clip(s, c).status;
  };
  // A track broken in several ways reports the first failing check:
  // BadClip, BadTarget, UnknownProperty, UnsupportedTrack, ComponentMismatch, UnknownNode, DuplicateTarget.
  CHECK(status_of(linear("nope", 3, {0, 0}, {0, 0, 0, 0, 0, 0})) == BindStatus::BadClip);                 // times, target
  CHECK(status_of(linear("nope", 3, {0}, {0, 0, 0})) == BindStatus::BadTarget);                            // target, node
  CHECK(status_of(linear("zz/q", 2, {0}, {0, 0})) == BindStatus::UnknownProperty);                        // property first
  Track r = linear("zz/q", 4, {0}, {0, 0, 0, 1});
  r.is_rotation = true;
  CHECK(status_of(r) == BindStatus::UnknownProperty);                                                      // before unsupported
  r.target = "zz/position";
  CHECK(status_of(r) == BindStatus::UnsupportedTrack);                                                     // before node, mismatch
  CHECK(status_of(linear("zz/position", 2, {0}, {0, 0})) == BindStatus::ComponentMismatch);               // before node
  CHECK(status_of(linear("zz/position", 3, {0}, {0, 0, 0})) == BindStatus::UnknownNode);
  // Tracks are checked in order: the earlier track's problem wins even if a later one has a "worse" status.
  Clip c;
  c.tracks.push_back(linear("zz/position", 3, {0}, {0, 0, 0}));            // UnknownNode
  c.tracks.push_back(linear("a/position", 3, {0}, {0, 0}));                // BadClip
  BindResult res = bind_clip(s, c);
  CHECK(res.status == BindStatus::UnknownNode);
  CHECK(res.track_index == 0);
  // Duplicate is only reported for the second track, and only after its other checks pass.
  c.tracks.clear();
  c.tracks.push_back(linear("a/position", 3, {0}, {0, 0, 0}));
  c.tracks.push_back(linear("a/position", 2, {0}, {0, 0}));
  CHECK(bind_clip(s, c).status == BindStatus::ComponentMismatch);
  c.tracks[1] = linear("a/position", 3, {0}, {1, 1, 1});
  res = bind_clip(s, c);
  CHECK(res.status == BindStatus::DuplicateTarget);
  CHECK(res.track_index == 1);
}

TEST_CASE("targets split at the last slash", "[anim][player][bind]") {
  Scene s;
  s.root = node("root");
  s.root.children.push_back(node("layer/hills"));
  s.root.children.push_back(node("layer/hills/far"));
  s.root.children.push_back(node("x/"));       // an id may end with a slash
  s.root.children.push_back(node("/lead"));    // or start with one
  s.root.children.push_back(node("a//b"));
  Clip c;
  c.tracks.push_back(linear("layer/hills/position", 3, {0}, {1, 2, 3}));
  c.tracks.push_back(linear("layer/hills/far/position", 3, {0}, {4, 5, 6}));
  c.tracks.push_back(linear("x//scale", 3, {0}, {7, 7, 7}));
  c.tracks.push_back(linear("/lead/position", 3, {0}, {8, 8, 8}));
  c.tracks.push_back(linear("a//b/rotation_z_degrees", 1, {0}, {9}));
  REQUIRE(bind_clip(s, c).ok());
  Scene out;
  BindResult why;
  REQUIRE(evaluate_clip(s, c, 0.0f, Loop::Clamp, out, why));
  CHECK(find(out.root, "layer/hills")->transform.position[0] == 1.0f);
  CHECK(find(out.root, "layer/hills/far")->transform.position[0] == 4.0f);
  CHECK(find(out.root, "x/")->transform.scale[0] == 7.0f);
  CHECK(find(out.root, "/lead")->transform.position[2] == 8.0f);
  CHECK(find(out.root, "a//b")->transform.rotation_z_degrees == 9.0f);
  // "layer/hills" alone is the node "layer" with property "hills": unknown property.
  Clip d;
  d.tracks.push_back(linear("layer/hills", 3, {0}, {0, 0, 0}));
  CHECK(bind_clip(s, d).status == BindStatus::UnknownProperty);
  // "/position" has an empty node id even though a node "/lead" exists.
  d.tracks[0] = linear("/position", 3, {0}, {0, 0, 0});
  CHECK(bind_clip(s, d).status == BindStatus::BadTarget);
}

TEST_CASE("duplicate node ids select the first pre-order occurrence", "[anim][player][bind]") {
  // Pre-order: root, d (first, under p), p's child e, then d again under q. The first "d" is the one under p.
  Scene s;
  s.root = node("root");
  Node p = node("p");
  p.children.push_back(node("d"));
  p.children.back().name = "first";
  Node q = node("q");
  q.children.push_back(node("d"));
  q.children.back().name = "second";
  s.root.children.push_back(p);
  s.root.children.push_back(q);
  Clip c;
  c.tracks.push_back(linear("d/position", 3, {0}, {1, 2, 3}));
  REQUIRE(bind_clip(s, c).ok());
  Scene out;
  BindResult why;
  REQUIRE(evaluate_clip(s, c, 0.0f, Loop::Clamp, out, why));
  CHECK(out.root.children[0].children[0].transform.position[0] == 1.0f);
  CHECK(out.root.children[1].children[0].transform.position[0] == 0.0f);
  // Two tracks for the same id and property are duplicates even though two nodes carry the id.
  c.tracks.push_back(linear("d/position", 3, {0}, {4, 5, 6}));
  CHECK(bind_clip(s, c).status == BindStatus::DuplicateTarget);
  // Pre-order, not breadth-first: root -> (x -> d(deep), d(shallow)). Pre-order meets the deep one first.
  Scene t;
  t.root = node("root");
  Node x = node("x");
  x.children.push_back(node("d"));
  t.root.children.push_back(x);
  t.root.children.push_back(node("d"));
  Clip e;
  e.tracks.push_back(linear("d/scale", 3, {0}, {2, 2, 2}));
  REQUIRE(evaluate_clip(t, e, 0.0f, Loop::Clamp, out, why));
  CHECK(out.root.children[0].children[0].transform.scale[0] == 2.0f);
  CHECK(out.root.children[1].transform.scale[0] == 1.0f);
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("evaluate_clip: values, untouched properties and an unchanged base", "[anim][player][eval]") {
  Scene s = small_scene();
  s.root.children[0].transform.position[0] = 5;
  s.root.children[0].transform.scale[1] = 3;
  s.root.children[1].layer_asset = "tex";
  s.root.children[1].parallax_depth = 0.25f;
  s.assets.push_back({"tex", "texture", "assets/tex.ppm"});
  const std::string before = image(s);

  Clip c;
  c.tracks.push_back(linear("a/position", 3, {0, 2}, {0, 0, 0, 10, 20, 30}));
  c.tracks.push_back(linear("b/rotation_z_degrees", 1, {0, 4}, {0, 90}));
  c.tracks.push_back(linear("a2/scale", 3, {1}, {2, 3, 4}));  // single key: held

  Scene out;
  BindResult why;
  REQUIRE(evaluate_clip(s, c, 1.0f, Loop::Clamp, out, why));
  CHECK(why.ok());
  CHECK(image(s) == before);  // base is unchanged

  const Node* a = find(out.root, "a");
  CHECK(a->transform.position[0] == 5.0f);
  CHECK(a->transform.position[1] == 10.0f);
  CHECK(a->transform.position[2] == 15.0f);
  CHECK(a->transform.scale[1] == 3.0f);            // untouched property keeps the base value
  CHECK(a->transform.rotation_z_degrees == 0.0f);
  CHECK(find(out.root, "b")->transform.rotation_z_degrees == 22.5f);  // duration 4: time 1 is a quarter
  CHECK(find(out.root, "b")->layer_asset == "tex");
  CHECK(find(out.root, "b")->parallax_depth == 0.25f);
  CHECK(find(out.root, "b")->name == "name of b");
  CHECK(find(out.root, "a2")->transform.scale[2] == 4.0f);
  CHECK(find(out.root, "a1")->transform.scale[2] == 1.0f);
  CHECK(out.assets.size() == 1);
  CHECK(out.assets[0].path == "assets/tex.ppm");
  // Everything outside the tracked properties is identical to the base.
  Scene expected = s;
  expected.root.children[0].transform.position[0] = 5;
  Clip empty;
  Scene same;
  REQUIRE(evaluate_clip(s, empty, 123.0f, Loop::Repeat, same, why));
  CHECK(image(same) == before);
}

TEST_CASE("evaluate_clip: Clamp, Repeat and PingPong time tables", "[anim][player][loop]") {
  Scene s = small_scene();
  Clip c;
  // duration 2, value = 10 * time
  c.tracks.push_back(linear("a/position", 3, {0, 2}, {0, 0, 0, 20, 20, 20}));
  struct Row { float time, clamp, repeat, pingpong; };
  const Row rows[] = {
      {0.0f, 0, 0, 0},      {0.5f, 5, 5, 5},       {1.0f, 10, 10, 10},   {2.0f, 20, 0, 20},  {2.5f, 20, 5, 15},
      {3.0f, 20, 10, 10},   {4.0f, 20, 0, 0},      {5.0f, 20, 10, 10},   {-1.0f, 0, 0, 0},   {-1e30f, 0, 0, 0},
      {kNaN, 0, 0, 0},      {kInf, 0, 0, 0},       {-kInf, 0, 0, 0},     {1e30f, 20, 0, 0},
  };
  for (const Row& r : rows) {
    INFO("time " << r.time);
    Scene out;
    BindResult why;
    REQUIRE(evaluate_clip(s, c, r.time, Loop::Clamp, out, why));
    CHECK(find(out.root, "a")->transform.position[0] == r.clamp);
    REQUIRE(evaluate_clip(s, c, r.time, Loop::Repeat, out, why));
    CHECK(find(out.root, "a")->transform.position[0] == r.repeat);
    REQUIRE(evaluate_clip(s, c, r.time, Loop::PingPong, out, why));
    CHECK(find(out.root, "a")->transform.position[0] == r.pingpong);
  }
  // The loop duration is the longest track of the clip.
  c.tracks.push_back(linear("b/rotation_z_degrees", 1, {0, 4}, {0, 40}));
  Scene out;
  BindResult why;
  REQUIRE(evaluate_clip(s, c, 3.0f, Loop::Repeat, out, why));
  CHECK(find(out.root, "a")->transform.position[0] == 20.0f);   // held after its last key
  CHECK(find(out.root, "b")->transform.rotation_z_degrees == 30.0f);
  REQUIRE(evaluate_clip(s, c, 5.0f, Loop::PingPong, out, why));
  CHECK(find(out.root, "b")->transform.rotation_z_degrees == 30.0f);  // 5 mirrors to 3
  // A clip with only a single-key track has duration of that key time.
  Clip one;
  one.tracks.push_back(linear("a/position", 3, {3}, {1, 2, 3}));
  REQUIRE(evaluate_clip(s, one, 99.0f, Loop::Repeat, out, why));
  CHECK(find(out.root, "a")->transform.position[1] == 2.0f);
}

TEST_CASE("evaluate_clip failure leaves out empty and fills why", "[anim][player][eval]") {
  Scene s = small_scene();
  Scene out = small_scene();
  BindResult why;
  Clip c;
  c.tracks.push_back(linear("a/position", 3, {0}, {0, 0, 0}));
  c.tracks.push_back(linear("nope/position", 3, {0}, {0, 0, 0}));
  CHECK_FALSE(evaluate_clip(s, c, 0.0f, Loop::Clamp, out, why));
  CHECK(why.status == BindStatus::UnknownNode);
  CHECK(why.track_index == 1);
  CHECK(why.message == bind_clip(s, c).message);
  CHECK(out.root.id.empty());
  CHECK(out.root.children.empty());
  CHECK(out.assets.empty());
  // Same status as bind_clip for every kind of failure, and `why` is overwritten on success.
  Clip bad;
  bad.tracks.push_back(linear("a/position", 3, {0, 0}, {0, 0, 0, 0, 0, 0}));
  CHECK_FALSE(evaluate_clip(s, bad, 0.0f, Loop::Clamp, out, why));
  CHECK(why.status == BindStatus::BadClip);
  Clip good;
  REQUIRE(evaluate_clip(s, good, 0.0f, Loop::Clamp, out, why));
  CHECK(why.ok());
  CHECK(why.message == "ok");
  // An empty base is fine too.
  Scene empty_base;
  CHECK_FALSE(evaluate_clip(empty_base, c, 0.0f, Loop::Clamp, out, why));
  CHECK(evaluate_clip(empty_base, good, 0.0f, Loop::Clamp, out, why));
}

TEST_CASE("evaluate_clip: output is deterministic and always finite", "[anim][player][eval]") {
  const Scene s = small_scene();
  Clip c;
  c.tracks.push_back(linear("a/position", 3, {0, 1, 2}, {-3e38f, 0, 3e38f, 3e38f, 1, -3e38f, 0, 0, 0}));
  c.tracks.push_back(linear("b/scale", 3, {0, 1}, {1, 2, 3, 4, 5, 6}));
  Track cs = linear("a1/rotation_z_degrees", 1, {0, 1}, {0, 0, 0, 90, 1, 0});
  cs.interp = Interp::CubicSpline;
  c.tracks.push_back(cs);
  std::string first;
  for (int i = 0; i < 50; ++i) {
    const float time = static_cast<float>(i) * 0.07f - 0.5f;
    Scene out1, out2;
    BindResult why;
    REQUIRE(evaluate_clip(s, c, time, Loop::PingPong, out1, why));
    REQUIRE(evaluate_clip(s, c, time, Loop::PingPong, out2, why));
    CHECK(image(out1) == image(out2));
    for (const char* id : {"a", "b", "a1"}) {
      const lumora::scene::Transform& t = find(out1.root, id)->transform;
      for (float v : t.position) CHECK(std::isfinite(v));
      for (float v : t.scale) CHECK(std::isfinite(v));
      CHECK(std::isfinite(t.rotation_z_degrees));
    }
  }
}

TEST_CASE("evaluate_clip may write into the base object", "[anim][player][eval]") {
  Scene s = small_scene();
  Clip c;
  c.tracks.push_back(linear("b/position", 3, {0}, {1, 2, 3}));
  BindResult why;
  REQUIRE(evaluate_clip(s, c, 0.0f, Loop::Clamp, s, why));
  CHECK(find(s.root, "b")->transform.position[2] == 3.0f);
  CHECK(find(s.root, "a1") != nullptr);
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("a depth-5000 scene with several tracks", "[anim][player][deep]") {
  Scene s;
  s.root = node("n0");
  std::vector<Node*> chain{&s.root};
  const int depth = 5000;
  for (int i = 1; i < depth; ++i) {
    chain.back()->children.push_back(node("n" + std::to_string(i)));
    chain.push_back(&chain.back()->children.back());
  }
  chain.back()->children.push_back(node("leaf/with/slashes"));
  Clip c;
  c.tracks.push_back(linear("n0/position", 3, {0, 1}, {0, 0, 0, 1, 1, 1}));
  c.tracks.push_back(linear("n2500/scale", 3, {0, 1}, {1, 1, 1, 3, 3, 3}));
  c.tracks.push_back(linear("n4999/rotation_z_degrees", 1, {0, 1}, {0, 180}));
  c.tracks.push_back(linear("leaf/with/slashes/position", 3, {0}, {7, 7, 7}));
  REQUIRE(bind_clip(s, c).ok());
  Scene out;
  BindResult why;
  REQUIRE(evaluate_clip(s, c, 0.5f, Loop::Clamp, out, why));
  // Walk the output without recursion.
  const Node* n = &out.root;
  int seen = 1;
  while (!n->children.empty()) {
    n = &n->children[0];
    ++seen;
  }
  CHECK(seen == depth + 1);
  CHECK(n->id == "leaf/with/slashes");
  CHECK(n->transform.position[0] == 7.0f);
  CHECK(out.root.transform.position[0] == 0.5f);
  const Node* mid = &out.root;
  for (int i = 0; i < 2500; ++i) mid = &mid->children[0];
  CHECK(mid->id == "n2500");
  CHECK(mid->transform.scale[0] == 2.0f);
  CHECK(image(out) != image(s));
  // Deterministic.
  Scene again;
  REQUIRE(evaluate_clip(s, c, 0.5f, Loop::Clamp, again, why));
  CHECK(image(again) == image(out));
  // A missing deep node is reported, not crashed on.
  c.tracks.push_back(linear("n5000/position", 3, {0}, {0, 0, 0}));
  CHECK(bind_clip(s, c).status == BindStatus::UnknownNode);
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("aurora-like hills movement checked by values", "[anim][player][parse]") {
  const char* scene_json = R"({
    "assets": [ { "id": "sky", "kind": "texture", "path": "assets/sky.ppm" },
                { "id": "hills", "kind": "texture", "path": "assets/hills.ppm" } ],
    "root": { "id": "root", "children": [
      { "id": "sky-layer", "name": "Sky", "layer": { "asset": "sky", "parallax_depth": 1.0 } },
      { "id": "hills-layer", "name": "Hills", "layer": { "asset": "hills", "parallax_depth": 0.4 },
        "transform": { "position": [0, -0.2, 0] } } ] } })";
  const auto parsed = lumora::scene::parse_scene(scene_json);
  REQUIRE(parsed.ok());
  const ParseClipResult clip = parse_clip_json(R"({
    "name": "drift",
    "tracks": [
      {"target": "hills-layer/position", "components": 3, "interpolation": "cubicspline",
       "times": [0, 4, 8],
       "values": [0,0,0,  0,-0.2,0,  0.5,0,0,
                  0.5,0,0,  1,-0.2,0,  0.5,0,0,
                  0.5,0,0,  0,-0.2,0,  0,0,0]},
      {"target": "sky-layer/scale", "components": 3, "interpolation": "linear",
       "times": [0, 8], "values": [1,1,1, 1.2,1.2,1.2]}
    ]})");
  REQUIRE(clip.ok);
  REQUIRE(bind_clip(parsed.scene, clip.clip).ok());
  Scene out;
  BindResult why;
  // time 0: first key value of the hills; sky scale 1.
  REQUIRE(evaluate_clip(parsed.scene, clip.clip, 0.0f, Loop::Repeat, out, why));
  CHECK(out.root.children[1].transform.position[0] == 0.0f);
  CHECK(out.root.children[1].transform.position[1] == Approx(-0.2).margin(1e-6));
  CHECK(out.root.children[0].transform.scale[0] == 1.0f);
  // time 4: the middle key exactly, sky scale 1.1.
  REQUIRE(evaluate_clip(parsed.scene, clip.clip, 4.0f, Loop::Repeat, out, why));
  CHECK(out.root.children[1].transform.position[0] == 1.0f);
  CHECK(out.root.children[0].transform.scale[1] == Approx(1.1).margin(1e-6));
  // time 2: u = 0.5 of dt = 4. x: v0 = 0, m0 = 0.5 (out of key 0), v1 = 1, m1 = 0.5 (in of key 1)
  // = h10*4*0.5 + h01*1 + h11*4*0.5 = 0.125*2 + 0.5 + (-0.125)*2 = 0.5.
  // y: v0 = v1 = -0.2, tangents 0 -> -0.2.
  REQUIRE(evaluate_clip(parsed.scene, clip.clip, 2.0f, Loop::Repeat, out, why));
  CHECK(out.root.children[1].transform.position[0] == Approx(0.5).margin(1e-6));
  CHECK(out.root.children[1].transform.position[1] == Approx(-0.2).margin(1e-6));
  CHECK(out.root.children[0].transform.scale[0] == Approx(1.05).margin(1e-6));
  // Repeat: time 8 wraps to 0, time 10 equals time 2; the base was not touched.
  REQUIRE(evaluate_clip(parsed.scene, clip.clip, 10.0f, Loop::Repeat, out, why));
  CHECK(out.root.children[1].transform.position[0] == Approx(0.5).margin(1e-6));
  CHECK(parsed.scene.root.children[1].transform.position[1] == -0.2f);
  CHECK(parsed.scene.root.children[1].transform.position[0] == 0.0f);
  // Nodes without tracks and the assets are copied unchanged.
  CHECK(out.root.children[0].layer_asset == "sky");
  CHECK(out.root.children[1].parallax_depth == 0.4f);
  CHECK(out.assets.size() == 2);
}

// ---------------------------------------------------------------------------------------------------------------
TEST_CASE("10000 nodes, 100 tracks, 1000 evaluations under a time cap", "[anim][player][perf]") {
  Scene s;
  s.root = node("root");
  const int groups = 100, per_group = 99;  // 1 + 100 + 100 * 99 = 10001 nodes
  for (int g = 0; g < groups; ++g) {
    Node group = node("g" + std::to_string(g));
    for (int i = 0; i < per_group; ++i) group.children.push_back(node("g" + std::to_string(g) + "_c" + std::to_string(i)));
    s.root.children.push_back(std::move(group));
  }
  std::size_t total = 0;
  count_nodes(s.root, total);
  REQUIRE(total >= 10000);

  Clip c;
  for (int t = 0; t < 100; ++t) {
    std::vector<float> times, values;
    for (int k = 0; k < 8; ++k) {
      times.push_back(static_cast<float>(k));
      for (int v = 0; v < 3; ++v) values.push_back(static_cast<float>(k * (v + 1) + t));
    }
    c.tracks.push_back(linear("g" + std::to_string(t) + "_c" + std::to_string(t % per_group) + "/position", 3, times, values));
  }
  REQUIRE(bind_clip(s, c).ok());

  const auto start = std::chrono::steady_clock::now();
  Scene out;
  BindResult why;
  for (int i = 0; i < 1000; ++i) {
    REQUIRE(evaluate_clip(s, c, static_cast<float>(i) * 0.013f, Loop::PingPong, out, why));
  }
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  INFO("1000 evaluations took " << secs << " s");
  CHECK(secs < 30.0);
  CHECK(find(out.root, "g0_c0")->transform.position[0] != 0.0f);
}
