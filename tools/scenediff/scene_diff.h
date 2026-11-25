// lumora-scenediff library: compares two loaded Lumora packages. CPU only, no network, no Lua.
//
// Public API
//  * load_files(files, side)  : checks a FileMap with scene::load_package and keeps the result.
//  * load_input(path, side)   : reads a .lumora archive (scene::read_zip_store) or a scene directory
//                               (scene::pack_directory), then load_package. Never returns a partial package.
//  * diff_packages(a, b)      : pure comparison. Findings come back sorted and deterministic.
//  * format_text / format_json: stable renderings of a DiffResult (see docs/scenediff.md).
//
// Matching rules (the full text is in docs/scenediff.md):
//  * Files are matched by path and compared by SHA-256. checksums.txt itself is never reported.
//  * Every Manifest field is compared. backends is compared as a set.
//  * Nodes are matched by id. A node whose id exists on both sides but under a different parent is reported as
//    removed (old path) plus added (new path). A node path is its ids from the root joined with '/'.
//  * Assets are matched by id.
//  * settings.json is just a file. It is not parsed.
#pragma once
#include <string>
#include <string_view>
#include <vector>

#include "scene.h"

namespace lumora::scenediff {

// One difference. `change` is '+' (added), '-' (removed) or '~' (changed).
// category is "file", "manifest", "node" or "asset". key is the file path, the manifest field name, the node path
// or the asset id. field names the changed part (node field, asset field, "sha256" for files) and is empty for
// added/removed entries. old_value/new_value are display strings; `quoted` tells the text renderer to put quotes
// around them (string values).
struct Finding {
  char change = '~';
  std::string category;
  std::string key;
  std::string field;
  std::string old_value;
  std::string new_value;
  bool quoted = false;
};

struct LoadedPackage {
  scene::FileMap files;  // includes checksums.txt
  scene::Manifest manifest;
  scene::Scene scene;
};

struct LoadOutcome {
  bool ok = false;
  std::string error;  // starts with the side label; empty when ok
  LoadedPackage package;
};

struct DiffResult {
  std::vector<Finding> findings;  // sorted by category, key, field, change
  bool identical() const { return findings.empty(); }
};

// Largest archive file load_input reads into memory.
inline constexpr unsigned long long kMaxArchiveBytes = 512ull << 20;

// `side` is a short label such as "A" or "B" used in error messages.
LoadOutcome load_files(scene::FileMap files, std::string_view side);

// `input` is used exactly as given; it is the only path that can appear in an error message.
LoadOutcome load_input(const std::string& input, std::string_view side);

DiffResult diff_packages(const LoadedPackage& a, const LoadedPackage& b);

// One finding per line, '\n' terminated. An identical result is the single line "identical".
std::string format_text(const DiffResult& r);

// Sorted keys, two-space indent, one finding per line. `a` and `b` are the input names as supplied.
std::string format_json(const DiffResult& r, std::string_view a, std::string_view b);

// The whole command line behaviour, so tests can run it in-process. `args` excludes the program name. Output
// that the CLI would print to stdout / stderr is appended to `out` / `err`. Returns the exit code: 0 identical,
// 1 differences, 2 error or bad usage.
int run_cli(const std::vector<std::string>& args, std::string& out, std::string& err);

// Escapes for a JSON string body: quotes, backslashes, control characters; invalid UTF-8 becomes U+FFFD.
std::string json_escape(std::string_view s);

}  // namespace lumora::scenediff
