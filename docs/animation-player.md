# Clip player

`core/anim/player.h` and `core/anim/player.cpp` bind animation tracks (see `docs/animation.md`) to scene nodes and
evaluate a clip at a time into a transformed copy of a scene. Pure CPU code, no GPU and no file system. The interface
is in `player.h`; this page records the rules the implementation follows.

## Targets

A track target is `<node_id>/<property>`. It is split at the **last** slash, so a node id may contain slashes:
`layer/hills/position` means node `layer/hills`, property `position`. Matching is exact and case sensitive.

| Property | Components | Writes |
| --- | --- | --- |
| `position` | 3 | `transform.position` |
| `scale` | 3 | `transform.scale` |
| `rotation_z_degrees` | 1 | `transform.rotation_z_degrees` |

Quaternion tracks (`is_rotation`) are not bound in this draft. Nothing else in a node changes: `id`, `name`,
`layer_asset`, `parallax_depth`, children and the scene assets are copied as they are.

## Statuses and check order

Tracks are checked in index order, and `track_index` is the first track that fails. For one track the checks run in
this order, and the first failure is the status:

1. `BadClip`: `validate_track` is not `Ok` (this includes an empty target).
2. `BadTarget`: no slash, an empty node id (target starts with a slash) or an empty property (target ends with a slash).
3. `UnknownProperty`: the property is not one of the three above.
4. `UnsupportedTrack`: `is_rotation` is set. This runs before the component check because a rotation track always has
   4 components, which no property accepts, so it would otherwise never be reported.
5. `ComponentMismatch`: the track has a different component count than the property.
6. `UnknownNode`: no node has that id.
7. `DuplicateTarget`: an earlier track already drives the same node and property. Two properties of one node are fine.

`bind_clip` returns `Ok` (message `ok`) for a clip with no tracks. Messages are short fixed texts per status and never
contain input bytes. `track_index` is 0 when the status is `Ok`.

## Duplicate node ids

The scene model says ids are unique, but a hand-built scene may repeat one. The node with the **first pre-order
occurrence** is the one a track binds to: pre-order visits a node, then its children in order, then the next sibling.
Two tracks naming the same id and property are `DuplicateTarget` even if two nodes carry that id. The enum has no
separate status for repeated ids.

## evaluate_clip

`evaluate_clip(base, clip, time, mode, out, why)`:

1. The loop time is `apply_loop(time, duration, mode)`, where duration is the longest last key time of the clip
   (the same number `clip_duration` returns for a valid clip). NaN, infinite and negative times give 0, as in
   `docs/animation.md`.
2. Each track is checked as above and sampled with `sample_track` at the loop time.
3. If every track passes, `out` becomes a copy of `base` with the tracked properties replaced by the sampled values.
   Untracked properties keep the base bytes. `why` is `Ok`. The function returns true.
4. On any failure `out` is emptied (`Scene{}`), `why` holds the same result `bind_clip` would return, and the
   function returns false. `base` is never changed.

Every sampled value is finite (the sampler guarantees it, and a non-finite value would be replaced by 0 as a last
guard). Properties that are not tracked are copied verbatim, so they are finite only if the base scene is.

`out` may be the same object as `base`: the result is built aside and moved in on success. On failure `out` is emptied,
so in that case the base object is emptied as well.

The copy is done by field (`id`, `name`, `transform`, `layer_asset`, `parallax_depth`, children, assets). A field added
to `scene::Node` later must be added to `copy_node_fields` in `player.cpp`.

## Cost

- `bind_clip`: one validation pass over all tracks, O(total keys), and one pre-order walk of the nodes, O(total nodes),
  with a hash lookup per node and a lookup table that holds only the ids the tracks name. The walk stops when every
  named id has been found.
- `evaluate_clip`: one validation pass (`sample_track` validates, and that is the only validation, no separate bind
  pass), O(log keys) to find each interval, one O(total nodes) iterative copy of the scene, and O(tracks) writes. The copy
  dominates for large scenes. It is the cost of the interface, which returns a full scene copy.
- All walks and the copy use explicit stacks, so a deep scene cannot overflow the call stack. Destroying or copying a
  `Scene` with the standard operators is recursive and is not done by the player.
- A prepared object (bind once, validate once, then sample into a reusable transform list without copying the scene)
  would remove the validation and copy costs per frame. It needs new interface and is only a proposal for the owner.

## Usage

```cpp
#include "player.h"
using namespace lumora::anim;

BindResult why = bind_clip(scene, clip);          // check once, for example at load time
if (!why.ok()) { /* report why.message and why.track_index */ }

lumora::scene::Scene frame;
if (evaluate_clip(scene, clip, seconds, Loop::Repeat, frame, why)) {
  /* draw frame */
}
```

## Tests and evidence limits

Tests are in `tests/anim/player_tests.cpp` (target `lumora_anim_tests`). They cover every status and the check order,
targets with slashes, duplicate ids, untouched properties, loop tables with negative, NaN and infinite times, failure
behavior, determinism (byte images of the output scenes), a depth-5000 scene, an aurora-like scene with a parsed clip
checked by values, and a scene of 10001 nodes with 100 tracks evaluated 1000 times under a 30 second cap.

- Evidence comes from CPU unit tests run in CI and locally. Nothing measures rendering or a GPU.
- The performance test checks a generous time cap, not a benchmark. Timing on other hosts will differ.
- The deep-scene test builds and frees a 5000 level tree; the standard destructor of that tree is recursive and the
  test relies on the default stack size of the test host.
