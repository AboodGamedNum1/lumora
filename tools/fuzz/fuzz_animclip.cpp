// Fuzz target: animclip. Feeds raw bytes to parse_clip_json. A failure must leave the clip empty. A success must give
// tracks that all pass validate_track, a finite non-negative clip duration, and finite sampled values at several times.
// Anything else aborts, so the fuzzer reports it as a crash.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "animation.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using namespace lumora::anim;
  const std::string text(reinterpret_cast<const char*>(data), size);
  const ParseClipResult r = parse_clip_json(text);
  if (!r.ok) {
    if (!r.clip.tracks.empty()) std::abort();
    return 0;
  }
  if (r.clip.tracks.size() > kMaxTracksPerClip) std::abort();
  const float dur = clip_duration(r.clip);
  if (!std::isfinite(dur) || dur < 0.0f) std::abort();
  const float times[5] = {0.0f, dur * 0.5f, dur, dur + 1.0f, 1e30f};
  for (const Track& t : r.clip.tracks) {
    if (validate_track(t) != TrackStatus::Ok) std::abort();
    float out[4] = {0, 0, 0, 0};
    for (float tm : times) {
      if (!sample_track(t, tm, out)) std::abort();
      for (int i = 0; i < t.components; ++i) {
        if (!std::isfinite(out[i])) std::abort();
      }
    }
  }
  return 0;
}
