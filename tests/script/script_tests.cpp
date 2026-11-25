// Sandbox and contract tests for the Lua host (core/script). Pure CPU. Software evidence only.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>

#include "script_host.h"

using namespace lumora;
using lumora::script::Limits;
using lumora::script::ParallaxOffset;
using lumora::script::ScriptHost;

namespace {

scene::SettingValue num(double v) {
  scene::SettingValue s;
  s.type = scene::SettingType::Float;
  s.f = v;
  return s;
}

std::unique_ptr<ScriptHost> make(const std::string& src, std::string& err, const scene::SettingsValues& st = {},
                                 Limits lim = {}) {
  return ScriptHost::create(src, st, lim, err);
}

// Runs one call and returns the first result, or a marker string with the error.
std::string run(const std::string& src, double x = 0.5, double y = -0.25, const scene::SettingsValues& st = {},
                Limits lim = {}) {
  std::string err;
  auto h = make(src, err, st, lim);
  if (!h) return "create failed: " + err;
  ParallaxOffset o;
  if (!h->on_parallax(x, y, o, err)) return "call failed: " + err;
  return std::to_string(o.x) + "," + std::to_string(o.y);
}

bool contains(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

}  // namespace

TEST_CASE("script returns what it computes from the input", "[script]") {
  CHECK(run("function on_parallax(x, y) return x * 2, y + 0.5 end") == "1.000000,0.250000");
  CHECK(run("function on_parallax(x, y) return x, y end", 0.125, -0.5) == "0.125000,-0.500000");
}

TEST_CASE("script output is clamped to [-1, 1]", "[script]") {
  CHECK(run("function on_parallax() return 5, -7 end") == "1.000000,-1.000000");
}

TEST_CASE("settings are readable and read-only", "[script]") {
  scene::SettingsValues st;
  st["strength"] = num(0.5);
  scene::SettingValue flag;
  flag.type = scene::SettingType::Bool;
  flag.b = true;
  st["on"] = flag;
  scene::SettingValue mode;
  mode.type = scene::SettingType::Enum;
  mode.e = "calm";
  st["mode"] = mode;
  scene::SettingValue col;
  col.type = scene::SettingType::Color;
  col.color[0] = 0.25f;
  st["tint"] = col;
  CHECK(run("function on_parallax(x, y) return x * settings.strength, y end", 0.5, 0, st) == "0.250000,0.000000");
  CHECK(run("function on_parallax() return settings.on and 1 or 0, settings.mode == 'calm' and 1 or 0 end", 0, 0, st) ==
        "1.000000,1.000000");
  CHECK(run("function on_parallax() return settings.tint.r, 0 end", 0, 0, st) == "0.250000,0.000000");
  CHECK(contains(run("function on_parallax() settings.strength = 9 return 0, 0 end", 0, 0, st), "read-only"));
  CHECK(contains(run("function on_parallax() settings.tint.r = 9 return 0, 0 end", 0, 0, st), "read-only"));
  CHECK(contains(run("function on_parallax() rawset(settings, 'x', 1) return 0, 0 end", 0, 0, st), "call failed"));
}

TEST_CASE("the sandbox has no io, os, package, debug, loaders or coroutines", "[script][sandbox]") {
  const char* names[] = {"io", "os", "package", "debug", "require", "load", "loadfile", "dofile", "loadstring",
                         "collectgarbage", "print", "rawset", "coroutine", "utf8", "_G", "xpcall"};
  for (const char* n : names) {
    CAPTURE(n);
    CHECK(run(std::string("function on_parallax() return type(") + n + ") == 'nil' and 1 or 0, 0 end") ==
          "1.000000,0.000000");
  }
  CHECK(run("function on_parallax() return (string.rep == nil and string.dump == nil and string.find == nil and "
            "string.gsub == nil and string.format == nil) and 1 or 0, 0 end") == "1.000000,0.000000");
  CHECK(run("function on_parallax() return (math.random == nil and math.randomseed == nil) and 1 or 0, 0 end") ==
        "1.000000,0.000000");
}

TEST_CASE("string methods cannot reach the removed functions through the string metatable", "[script][sandbox]") {
  CHECK(contains(run("function on_parallax() local s = ('x'):rep(1000000000) return 0, 0 end"), "call failed"));
  CHECK(contains(run("function on_parallax() local s = ('x'):dump() return 0, 0 end"), "call failed"));
  CHECK(run("function on_parallax() return getmetatable('') == false and 1 or 0, 0 end") == "1.000000,0.000000");
  CHECK(run("function on_parallax() return ('ab'):upper() == 'AB' and 1 or 0, ('abc'):len() end") ==
        "1.000000,1.000000");
}

TEST_CASE("binary chunks and oversized or broken scripts are refused", "[script][sandbox]") {
  CHECK(contains(run(std::string("\x1b") + "Lua binary"), "binary chunks"));
  Limits lim;
  lim.max_source_bytes = 32;
  CHECK(contains(run(std::string(100, ' ') + "function on_parallax() return 0, 0 end", 0, 0, {}, lim), "larger"));
  CHECK(contains(run("function on_parallax( return"), "create failed"));
  CHECK(contains(run("x = 1"), "does not define on_parallax"));
  CHECK(contains(run("on_parallax = 5"), "does not define on_parallax"));
  CHECK(contains(run("error('boom')"), "boom"));
}

TEST_CASE("runaway scripts are stopped by the instruction budget", "[script][sandbox]") {
  const auto t0 = std::chrono::steady_clock::now();
  CHECK(contains(run("function on_parallax() while true do end end"), "instruction budget"));
  CHECK(contains(run("while true do end"), "instruction budget"));
  CHECK(contains(run("function on_parallax() local n = 0 repeat n = n + 1 until false end"), "instruction budget"));
  CHECK(contains(run("function on_parallax() return on_parallax() + 1 end"), "call failed"));
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
  CHECK(ms < 5000);
}

TEST_CASE("pcall cannot swallow the instruction budget", "[script][sandbox]") {
  CHECK(contains(run("function on_parallax() while true do pcall(function() while true do end end) end end"),
                 "call failed"));
  CHECK(contains(run("function on_parallax() pcall(function() while true do end end) while true do end end"),
                 "call failed"));
  CHECK(contains(run("function on_parallax() while true do pcall(pcall, function() while true do end end) end end"),
                 "call failed"));
}

TEST_CASE("finalizers and to-be-closed handlers are refused", "[script][sandbox]") {
  // Lua turns debug hooks off while a __gc or __close handler runs, so these would escape the budget.
  CHECK(contains(run("setmetatable({}, {__gc = function() while true do end end}) "
                     "function on_parallax() return 0, 0 end"),
                 "finalizers"));
  CHECK(contains(run("function on_parallax() setmetatable({}, {__close = function() end}) return 0, 0 end"),
                 "finalizers"));
  CHECK(run("function on_parallax() local t = setmetatable({}, {__index = function(_, k) return 2 end}) "
            "return t.a, 0 end") == "1.000000,0.000000");
  // A metatable gaining __gc after setmetatable has no effect in Lua 5.4, and a script cannot reach the string
  // metatable to add one there.
  CHECK(run("function on_parallax() local mt = {} local t = setmetatable({}, mt) mt.__gc = function() "
            "while true do end end t = nil return 0, 0 end") == "0.000000,0.000000");
}

TEST_CASE("memory is capped and the host stays alive", "[script][sandbox]") {
  Limits lim;
  lim.memory_bytes = 1u << 20;
  std::string err;
  auto h = make("function on_parallax(x) if x > 0 then local t = {} for i = 1, 100000000 do t[i] = {i} end end "
                "return 0, 0 end",
                err, {}, lim);
  REQUIRE(h);
  ParallaxOffset o;
  CHECK_FALSE(h->on_parallax(1, 0, o, err));
  CHECK(contains(err, "memory"));
  CHECK(h->memory_used() <= lim.memory_bytes);
  CHECK(h->on_parallax(0, 0, o, err));  // garbage is collectable, so a harmless call still works
}

TEST_CASE("deep recursion is an error, not a crash", "[script][sandbox]") {
  CHECK(contains(run("local function f(n) return 1 + f(n + 1) end function on_parallax() return f(1), 0 end"),
                 "call failed"));
  CHECK(contains(run("function on_parallax() local s = '' for i = 1, 100000 do s = s .. 'x' end return 0, 0 end",
                     0, 0, {}, [] { Limits l; l.memory_bytes = 64 * 1024; return l; }()),
                 "call failed"));
}

TEST_CASE("bad return values are errors", "[script]") {
  CHECK(contains(run("function on_parallax() return 0 end"), "two numbers"));
  CHECK(contains(run("function on_parallax() return 'a', 'b' end"), "two numbers"));
  CHECK(contains(run("function on_parallax() return '1', '2' end"), "two numbers"));
  CHECK(contains(run("function on_parallax() return 0/0, 0 end"), "not finite"));
  CHECK(contains(run("function on_parallax() return math.huge, 0 end"), "not finite"));
  CHECK(contains(run("function on_parallax() error({}) end"), "error object"));
  CHECK(contains(run("function on_parallax() error(nil) end"), "call failed"));
}

TEST_CASE("repeated failures suspend the script, and a success resets the count", "[script]") {
  std::string err;
  auto h = make("function on_parallax(x) if x > 0 then error('bad') end return x, 0 end", err);
  REQUIRE(h);
  ParallaxOffset o;
  CHECK_FALSE(h->on_parallax(1, 0, o, err));
  CHECK_FALSE(h->on_parallax(1, 0, o, err));
  CHECK(h->on_parallax(-0.5, 0, o, err));  // resets the streak
  CHECK_FALSE(h->on_parallax(1, 0, o, err));
  CHECK_FALSE(h->on_parallax(1, 0, o, err));
  CHECK_FALSE(h->on_parallax(1, 0, o, err));
  CHECK(h->suspended());
  CHECK_FALSE(h->on_parallax(-0.5, 0, o, err));
  CHECK(contains(err, "suspended"));
}

TEST_CASE("script state persists between calls and results are deterministic", "[script]") {
  std::string err;
  auto h = make("local n = 0 function on_parallax(x) n = n + 1 return n / 10, x end", err);
  REQUIRE(h);
  ParallaxOffset a, b;
  REQUIRE(h->on_parallax(0.5, 0, a, err));
  REQUIRE(h->on_parallax(0.5, 0, b, err));
  CHECK(a.x == 0.1);
  CHECK(b.x == 0.2);
  CHECK(run("function on_parallax(x, y) return x * 0.3, y end") == run("function on_parallax(x, y) return x * 0.3, y end"));
}

TEST_CASE("the instruction budget applies to every call", "[script][sandbox]") {
  Limits lim;
  lim.instructions_per_call = 1000;
  std::string err;
  auto h = make("function on_parallax(x) local n = 0 for i = 1, x do n = n + 1 end return 0, 0 end", err, {}, lim);
  REQUIRE(h);
  ParallaxOffset o;
  CHECK(h->on_parallax(10, 0, o, err));
  CHECK_FALSE(h->on_parallax(100000, 0, o, err));
  CHECK(contains(err, "instruction budget"));
  CHECK(h->on_parallax(10, 0, o, err));  // the budget is re-armed per call
}
