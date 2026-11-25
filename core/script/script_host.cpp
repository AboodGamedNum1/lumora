#include "script_host.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// Lua is built as C++ (see cmake/Dependencies.cmake), so errors are C++ exceptions and unwind our frames safely.
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

namespace lumora::script {

namespace {

// Hook granularity: the budget is checked every kHookStep VM instructions.
constexpr int kHookStep = 100;

struct Allocator {
  std::size_t used = 0;
  std::size_t limit = 0;
};

// Lua allocator with a hard cap. Returning null makes Lua raise a memory error. Shrinking always succeeds.
void* limited_alloc(void* ud, void* ptr, std::size_t osize, std::size_t nsize) {
  auto* a = static_cast<Allocator*>(ud);
  if (nsize == 0) {
    if (ptr) a->used -= osize;
    std::free(ptr);
    return nullptr;
  }
  const std::size_t old = ptr ? osize : 0;  // osize is a type tag when ptr is null
  if (nsize > old && a->used - old + nsize > a->limit) return nullptr;
  void* p = std::realloc(ptr, nsize);
  if (!p) return nullptr;
  a->used = a->used - old + nsize;
  return p;
}

}  // namespace

struct ScriptHost::Impl {
  Allocator alloc;
  Limits limits;
  lua_State* L = nullptr;
  std::int64_t remaining = 0;  // instructions left in the current call
  bool exceeded = false;       // set when the budget ran out in the current call
  int env_ref = LUA_NOREF;
  int fn_ref = LUA_NOREF;
  unsigned failures = 0;
  bool suspended = false;
  const scene::SettingsValues* settings = nullptr;  // only valid during setup

  ~Impl() {
    if (L) {
      // A __gc finalizer must not get a budget: the hook errors on its first tick.
      remaining = 0;
      exceeded = true;
      lua_close(L);
    }
  }
};

namespace {

using Impl = ScriptHost::Impl;

Impl* impl_of(lua_State* L) { return *static_cast<Impl**>(lua_getextraspace(L)); }

void budget_hook(lua_State* L, lua_Debug*) {
  Impl* h = impl_of(L);
  h->remaining -= kHookStep;
  if (h->remaining <= 0) {
    h->exceeded = true;
    luaL_error(L, "instruction budget exceeded");
  }
}

void arm(Impl* h) {
  h->remaining = static_cast<std::int64_t>(std::min<std::uint64_t>(h->limits.instructions_per_call, INT64_MAX / 2));
  h->exceeded = false;
}

// pcall that cannot swallow a budget overrun: once the budget is gone the error always propagates.
int sandbox_pcall(lua_State* L) {
  luaL_checkany(L, 1);
  lua_pushboolean(L, 1);
  lua_insert(L, 1);
  const int status = lua_pcall(L, lua_gettop(L) - 2, LUA_MULTRET, 0);
  if (impl_of(L)->exceeded) {
    lua_pushliteral(L, "instruction budget exceeded");
    return lua_error(L);
  }
  if (status != LUA_OK) {
    lua_pushboolean(L, 0);
    lua_insert(L, -2);
    return 2;
  }
  return lua_gettop(L);
}

// setmetatable without finalizers: Lua disables debug hooks while a __gc or __close handler runs, so the
// instruction budget could not stop one. Rejecting both keeps every script instruction under the hook.
int sandbox_setmetatable(lua_State* L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  const int t = lua_type(L, 2);
  luaL_argexpected(L, t == LUA_TNIL || t == LUA_TTABLE, 2, "nil or table");
  if (luaL_getmetafield(L, 1, "__metatable") != LUA_TNIL) return luaL_error(L, "cannot change a protected metatable");
  if (t == LUA_TTABLE) {
    for (const char* name : {"__gc", "__close"}) {
      lua_pushstring(L, name);
      lua_rawget(L, 2);
      if (!lua_isnil(L, -1)) return luaL_error(L, "finalizers (%s) are not allowed", name);
      lua_pop(L, 1);
    }
  }
  lua_settop(L, 2);
  lua_setmetatable(L, 1);
  return 1;
}

int readonly_newindex(lua_State* L) { return luaL_error(L, "attempt to modify a read-only table"); }

// Replaces the table on top of the stack with an empty proxy whose reads go to it and whose writes fail.
void make_readonly(lua_State* L) {
  lua_newtable(L);           // proxy
  lua_newtable(L);           // metatable
  lua_pushvalue(L, -3);      // data
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, readonly_newindex);
  lua_setfield(L, -2, "__newindex");
  lua_pushboolean(L, 0);
  lua_setfield(L, -2, "__metatable");
  lua_setmetatable(L, -2);
  lua_remove(L, -2);         // drop data, keep proxy
}

// Copies the named fields of the table at `from` into the table at the stack top.
void copy_fields(lua_State* L, int from, const char* const* names) {
  from = lua_absindex(L, from);
  for (; *names; ++names) {
    lua_getfield(L, from, *names);
    lua_setfield(L, -2, *names);
  }
}

void push_setting(lua_State* L, const scene::SettingValue& v) {
  switch (v.type) {
    case scene::SettingType::Bool: lua_pushboolean(L, v.b ? 1 : 0); break;
    case scene::SettingType::Int: lua_pushinteger(L, static_cast<lua_Integer>(v.i)); break;
    case scene::SettingType::Float: lua_pushnumber(L, v.f); break;
    case scene::SettingType::Enum: lua_pushlstring(L, v.e.data(), v.e.size()); break;
    case scene::SettingType::Color: {
      lua_newtable(L);
      const char* const keys[4] = {"r", "g", "b", "a"};
      for (int i = 0; i < 4; ++i) {
        lua_pushnumber(L, static_cast<double>(v.color[i]));
        lua_setfield(L, -2, keys[i]);
      }
      make_readonly(L);
      break;
    }
  }
}

// Runs inside lua_pcall so allocation failures and Lua errors cannot escape.
int setup(lua_State* L) {
  Impl* h = static_cast<Impl*>(lua_touserdata(L, 1));
  lua_settop(L, 0);

  // Open only the pure libraries. io, os, package, debug, coroutine and utf8 are never opened.
  luaL_requiref(L, "_G", luaopen_base, 1);
  luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
  luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
  luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
  lua_settop(L, 0);

  // Allowlisted environment, built by copying listed functions into a fresh table.
  lua_newtable(L);  // 1: env
  lua_pushglobaltable(L);  // 2: real globals
  static const char* const base_names[] = {"assert", "error", "ipairs", "next", "pairs", "select", "tonumber",
                                           "tostring", "type", "rawequal", "rawget", "rawlen",
                                           "getmetatable", nullptr};
  lua_pushvalue(L, 1);  // copy_fields writes into the table on top of the stack
  copy_fields(L, 2, base_names);
  lua_pop(L, 1);
  lua_pushcfunction(L, sandbox_pcall);
  lua_setfield(L, 1, "pcall");
  lua_pushcfunction(L, sandbox_setmetatable);
  lua_setfield(L, 1, "setmetatable");

  lua_getfield(L, 2, "math");  // 3
  lua_newtable(L);             // 4: safe math. random and randomseed are left out for determinism.
  static const char* const math_names[] = {"abs", "ceil", "floor", "fmod", "max", "min", "modf", "sqrt", "sin", "cos",
                                           "tan", "asin", "acos", "atan", "exp", "log", "tointeger", "type", "ult",
                                           "pi", "huge", "maxinteger", "mininteger", nullptr};
  copy_fields(L, 3, math_names);
  lua_setfield(L, 1, "math");
  lua_pop(L, 1);

  lua_getfield(L, 2, "string");  // 3
  lua_newtable(L);               // 4: safe string. No rep, dump, find, match, gmatch, gsub or format.
  static const char* const string_names[] = {"byte", "char", "len", "lower", "upper", "sub", "reverse", nullptr};
  copy_fields(L, 3, string_names);
  lua_pushvalue(L, 4);
  lua_setfield(L, 1, "string");
  // Strings reach methods through their shared metatable: point it at the safe table and hide it.
  lua_pushliteral(L, "");
  if (lua_getmetatable(L, -1)) {
    lua_pushvalue(L, 4);
    lua_setfield(L, -2, "__index");
    lua_pushboolean(L, 0);
    lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);
  }
  lua_pop(L, 3);  // "", safe string, real string

  lua_getfield(L, 2, "table");  // 3
  lua_newtable(L);              // 4
  static const char* const table_names[] = {"insert", "remove", "concat", "unpack", nullptr};
  copy_fields(L, 3, table_names);
  lua_setfield(L, 1, "table");
  lua_pop(L, 1);

  lua_newtable(L);
  lua_pushinteger(L, 1);
  lua_setfield(L, -2, "api_version");
  make_readonly(L);
  lua_setfield(L, 1, "lumora");

  lua_newtable(L);
  for (const auto& [id, value] : *h->settings) {
    push_setting(L, value);
    lua_setfield(L, -2, id.c_str());
  }
  make_readonly(L);
  lua_setfield(L, 1, "settings");

  // Wipe the real globals so nothing outside the environment is reachable by name from the state.
  lua_pushnil(L);
  while (lua_next(L, 2)) {
    lua_pop(L, 1);
    lua_pushvalue(L, -1);
    lua_pushnil(L);
    lua_rawset(L, 2);
  }
  lua_pop(L, 1);  // real globals
  h->env_ref = luaL_ref(L, LUA_REGISTRYINDEX);  // pops env
  return 0;
}

std::string take_error(lua_State* L) {
  std::string msg = "unknown error";
  if (lua_type(L, -1) == LUA_TSTRING) msg = lua_tostring(L, -1);
  else if (lua_isnumber(L, -1)) msg = lua_tostring(L, -1);
  else msg = std::string("error object of type ") + luaL_typename(L, -1);
  lua_pop(L, 1);
  if (msg.size() > 200) msg.resize(200);
  return msg;
}

int load_and_run(lua_State* L) {
  Impl* h = static_cast<Impl*>(lua_touserdata(L, 1));
  const char* src = static_cast<const char*>(lua_touserdata(L, 2));
  const std::size_t len = static_cast<std::size_t>(lua_tointeger(L, 3));
  lua_settop(L, 0);
  if (luaL_loadbufferx(L, src, len, "=script", "t") != LUA_OK) return lua_error(L);
  lua_rawgeti(L, LUA_REGISTRYINDEX, h->env_ref);
  if (!lua_setupvalue(L, 1, 1)) return luaL_error(L, "cannot set the script environment");
  lua_call(L, 0, 0);
  lua_rawgeti(L, LUA_REGISTRYINDEX, h->env_ref);
  lua_getfield(L, -1, "on_parallax");
  if (!lua_isfunction(L, -1)) return luaL_error(L, "the script does not define on_parallax");
  h->fn_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}

}  // namespace

ScriptHost::ScriptHost(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
ScriptHost::~ScriptHost() = default;

std::unique_ptr<ScriptHost> ScriptHost::create(std::string_view source, const scene::SettingsValues& settings,
                                               const Limits& limits, std::string& error) {
  if (source.size() > limits.max_source_bytes) {
    error = "script is larger than the allowed size";
    return nullptr;
  }
  if (!source.empty() && source[0] == '\x1b') {
    error = "binary chunks are not accepted";
    return nullptr;
  }
  auto h = std::make_unique<Impl>();
  h->limits = limits;
  h->alloc.limit = limits.memory_bytes;
  h->settings = &settings;
  h->L = lua_newstate(limited_alloc, &h->alloc);
  if (!h->L) {
    error = "cannot create the script state";
    return nullptr;
  }
  *static_cast<Impl**>(lua_getextraspace(h->L)) = h.get();
  lua_sethook(h->L, budget_hook, LUA_MASKCOUNT, kHookStep);

  arm(h.get());
  lua_pushcfunction(h->L, setup);
  lua_pushlightuserdata(h->L, h.get());
  if (lua_pcall(h->L, 1, 0, 0) != LUA_OK) {
    error = "setup failed: " + take_error(h->L);
    return nullptr;
  }
  h->settings = nullptr;

  arm(h.get());
  lua_pushcfunction(h->L, load_and_run);
  lua_pushlightuserdata(h->L, h.get());
  lua_pushlightuserdata(h->L, const_cast<char*>(source.data()));
  lua_pushinteger(h->L, static_cast<lua_Integer>(source.size()));
  if (lua_pcall(h->L, 3, 0, 0) != LUA_OK) {
    error = take_error(h->L);
    return nullptr;
  }
  return std::unique_ptr<ScriptHost>(new ScriptHost(std::move(h)));
}

bool ScriptHost::on_parallax(double x, double y, ParallaxOffset& out, std::string& error) {
  Impl* h = impl_.get();
  if (h->suspended) {
    error = "script is suspended after repeated failures";
    return false;
  }
  auto fail = [&](std::string msg) {
    error = std::move(msg);
    if (++h->failures >= h->limits.suspend_after_failures) h->suspended = true;
    return false;
  };
  lua_State* L = h->L;
  lua_settop(L, 0);
  arm(h);
  lua_rawgeti(L, LUA_REGISTRYINDEX, h->fn_ref);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  if (lua_pcall(L, 2, 2, 0) != LUA_OK) return fail(take_error(L));
  if (!lua_isnumber(L, -2) || !lua_isnumber(L, -1) || lua_type(L, -2) == LUA_TSTRING || lua_type(L, -1) == LUA_TSTRING) {
    lua_settop(L, 0);
    return fail("on_parallax must return two numbers");
  }
  const double nx = lua_tonumber(L, -2), ny = lua_tonumber(L, -1);
  lua_settop(L, 0);
  if (!std::isfinite(nx) || !std::isfinite(ny)) return fail("on_parallax returned a value that is not finite");
  out.x = std::clamp(nx, -1.0, 1.0);
  out.y = std::clamp(ny, -1.0, 1.0);
  h->failures = 0;
  return true;
}

bool ScriptHost::suspended() const { return impl_->suspended; }
std::size_t ScriptHost::memory_used() const { return impl_->alloc.used; }

}  // namespace lumora::script
