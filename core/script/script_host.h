// Sandboxed Lua 5.4 host for the one scene callback of the M3S vertical slice (docs/scripting-slice.md).
// This is a minimal pull-forward of M5: a single callback, no engine API beyond read-only settings.
//
// Contract of a scene script:
//   function on_parallax(x, y)   -- x, y: parallax input in [-1, 1] (device tilt or touch)
//     return nx, ny              -- two finite numbers, clamped by the host to [-1, 1]
//   end
// The script runs in a fresh environment built from an allowlist (see script_host.cpp). The global `settings` is a
// read-only view of the resolved setting values (bool, number, enum id string, color as a table with r, g, b, a).
// Every call has an instruction budget, the whole state has a memory cap, and binary chunks are refused.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "settings.h"

namespace lumora::script {

struct Limits {
  std::size_t max_source_bytes = 64 * 1024;   // larger scripts are refused
  std::size_t memory_bytes = 4u << 20;        // allocation failure becomes a script error
  std::uint64_t instructions_per_call = 200000;  // budget for the chunk and for each on_parallax call
  unsigned suspend_after_failures = 3;        // consecutive failed calls before the script is suspended
};

struct ParallaxOffset {
  double x = 0;
  double y = 0;
};

class ScriptHost {
 public:
  // Compiles and runs the script chunk (which defines on_parallax). Returns null and fills `error` on any
  // failure: oversized source, binary chunk, syntax error, runtime error or budget overrun in the chunk, or a
  // missing on_parallax function.
  static std::unique_ptr<ScriptHost> create(std::string_view source, const scene::SettingsValues& settings,
                                            const Limits& limits, std::string& error);
  ~ScriptHost();
  ScriptHost(const ScriptHost&) = delete;
  ScriptHost& operator=(const ScriptHost&) = delete;

  // Calls on_parallax. False and `error` set when the call fails (error, budget, memory, bad return values) or
  // the script is suspended. Output is clamped to [-1, 1].
  bool on_parallax(double x, double y, ParallaxOffset& out, std::string& error);

  bool suspended() const;
  std::size_t memory_used() const;

  struct Impl;  // opaque

 private:
  explicit ScriptHost(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

}  // namespace lumora::script
