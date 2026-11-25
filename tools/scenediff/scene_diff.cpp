#include "scene_diff.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>

#include "checksums.h"
#include "pack_dir.h"
#include "zip_store.h"

namespace lumora::scenediff {
namespace {

using scene::Node;

// Shortest %g form (6 to 9 significant digits) that reads back as the same float. Uses the "C" locale, which
// the CLI never changes.
std::string fmt_float(float v) {
  char buf[40];
  for (int digits = 6; digits <= 9; ++digits) {
    std::snprintf(buf, sizeof buf, "%.*g", digits, static_cast<double>(v));
    if (std::strtof(buf, nullptr) == v) break;
  }
  return buf;
}

std::string fmt_vec3(const float v[3]) {
  return "[" + fmt_float(v[0]) + ", " + fmt_float(v[1]) + ", " + fmt_float(v[2]) + "]";
}

std::string fmt_semver(const scene::SemVer& s) {
  return std::to_string(s.major) + "." + std::to_string(s.minor) + "." + std::to_string(s.patch);
}

std::string fmt_list(std::vector<std::string> v, bool sort) {
  if (sort) std::sort(v.begin(), v.end());
  std::string out = "[";
  for (std::size_t i = 0; i < v.size(); ++i) out += (i ? ", " : "") + v[i];
  return out + "]";
}

void changed(std::vector<Finding>& out, const char* cat, std::string key, std::string field, std::string o,
             std::string n, bool quoted) {
  out.push_back(Finding{'~', cat, std::move(key), std::move(field), std::move(o), std::move(n), quoted});
}

void diff_files(const LoadedPackage& a, const LoadedPackage& b, std::vector<Finding>& out) {
  auto digests = [](const scene::FileMap& files) {
    std::map<std::string, std::string> m;
    for (const auto& [path, data] : files)
      if (path != "checksums.txt") m[path] = scene::to_hex(scene::sha256(data.data(), data.size()));
    return m;
  };
  const auto da = digests(a.files), db = digests(b.files);
  for (const auto& [path, h] : da) {
    const auto it = db.find(path);
    if (it == db.end()) out.push_back(Finding{'-', "file", path, "", "", "", false});
    else if (it->second != h) changed(out, "file", path, "sha256", h.substr(0, 12), it->second.substr(0, 12), false);
  }
  for (const auto& [path, h] : db)
    if (!da.count(path)) out.push_back(Finding{'+', "file", path, "", "", "", false});
}

void diff_manifest(const scene::Manifest& a, const scene::Manifest& b, std::vector<Finding>& out) {
  auto num = [&](const char* f, std::uint32_t x, std::uint32_t y) {
    if (x != y) changed(out, "manifest", f, "", std::to_string(x), std::to_string(y), false);
  };
  auto str = [&](const char* f, const std::string& x, const std::string& y) {
    if (x != y) changed(out, "manifest", f, "", x, y, true);
  };
  num("format_version", a.format_version, b.format_version);
  num("scene_schema_version", a.scene_schema_version, b.scene_schema_version);
  num("lua_api_version", a.lua_api_version, b.lua_api_version);
  if (a.min_engine_version != b.min_engine_version)
    changed(out, "manifest", "min_engine_version", "", fmt_semver(a.min_engine_version),
            fmt_semver(b.min_engine_version), false);
  str("name", a.name, b.name);
  str("author", a.author, b.author);
  str("scene_path", a.scene_path, b.scene_path);
  str("settings_path", a.settings_path, b.settings_path);
  std::set<std::string> sa(a.backends.begin(), a.backends.end()), sb(b.backends.begin(), b.backends.end());
  if (sa != sb)
    changed(out, "manifest", "backends", "", fmt_list({sa.begin(), sa.end()}, false),
            fmt_list({sb.begin(), sb.end()}, false), false);
}

struct NodeInfo {
  const Node* node = nullptr;
  std::string parent;  // parent id, empty for the root
  std::string path;
};

void flatten(const Node& n, const std::string& parent, const std::string& parent_path,
             std::map<std::string, NodeInfo>& out) {
  // Explicit stack: the tree depth is capped by the loader, but the walk does not depend on it.
  struct Item {
    const Node* node;
    std::string parent, path;
  };
  std::vector<Item> stack;
  stack.push_back({&n, parent, parent_path.empty() ? n.id : parent_path + "/" + n.id});
  while (!stack.empty()) {
    Item it = std::move(stack.back());
    stack.pop_back();
    for (const auto& c : it.node->children) stack.push_back({&c, it.node->id, it.path + "/" + c.id});
    out[it.node->id] = NodeInfo{it.node, std::move(it.parent), std::move(it.path)};
  }
}

void diff_nodes(const scene::Scene& a, const scene::Scene& b, std::vector<Finding>& out) {
  std::map<std::string, NodeInfo> na, nb;
  flatten(a.root, "", "", na);
  flatten(b.root, "", "", nb);
  auto same_node = [&](const std::string& id) {
    const auto x = na.find(id), y = nb.find(id);
    return x != na.end() && y != nb.end() && x->second.parent == y->second.parent;
  };
  for (const auto& [id, info] : na)
    if (!same_node(id)) out.push_back(Finding{'-', "node", info.path, "", "", "", false});
  for (const auto& [id, info] : nb)
    if (!same_node(id)) out.push_back(Finding{'+', "node", info.path, "", "", "", false});
  for (const auto& [id, x] : na) {
    if (!same_node(id)) continue;
    const NodeInfo& y = nb.at(id);
    const Node& p = *x.node;
    const Node& q = *y.node;
    const std::string& path = y.path;
    if (p.name != q.name) changed(out, "node", path, "name", p.name, q.name, true);
    if (!std::equal(p.transform.position, p.transform.position + 3, q.transform.position))
      changed(out, "node", path, "position", fmt_vec3(p.transform.position), fmt_vec3(q.transform.position), false);
    if (!std::equal(p.transform.scale, p.transform.scale + 3, q.transform.scale))
      changed(out, "node", path, "scale", fmt_vec3(p.transform.scale), fmt_vec3(q.transform.scale), false);
    if (p.transform.rotation_z_degrees != q.transform.rotation_z_degrees)
      changed(out, "node", path, "rotation_z_degrees", fmt_float(p.transform.rotation_z_degrees),
              fmt_float(q.transform.rotation_z_degrees), false);
    if (p.layer_asset != q.layer_asset) changed(out, "node", path, "layer_asset", p.layer_asset, q.layer_asset, true);
    if (p.parallax_depth != q.parallax_depth)
      changed(out, "node", path, "parallax_depth", fmt_float(p.parallax_depth), fmt_float(q.parallax_depth), false);
    // Sibling order is draw order. Compare only the children that exist on both sides under this parent.
    std::vector<std::string> oa, ob;
    for (const auto& c : p.children)
      if (same_node(c.id)) oa.push_back(c.id);
    for (const auto& c : q.children)
      if (same_node(c.id)) ob.push_back(c.id);
    if (oa != ob) changed(out, "node", path, "children_order", fmt_list(oa, false), fmt_list(ob, false), false);
  }
}

void diff_assets(const scene::Scene& a, const scene::Scene& b, std::vector<Finding>& out) {
  std::map<std::string, const scene::Asset*> ma, mb;
  for (const auto& x : a.assets) ma[x.id] = &x;
  for (const auto& x : b.assets) mb[x.id] = &x;
  auto desc = [](const scene::Asset& x) { return "kind=" + x.kind + " path=" + x.path; };
  for (const auto& [id, x] : ma) {
    const auto it = mb.find(id);
    if (it == mb.end()) {
      out.push_back(Finding{'-', "asset", id, "", desc(*x), "", false});
      continue;
    }
    if (x->kind != it->second->kind) changed(out, "asset", id, "kind", x->kind, it->second->kind, true);
    if (x->path != it->second->path) changed(out, "asset", id, "path", x->path, it->second->path, true);
  }
  for (const auto& [id, x] : mb)
    if (!ma.count(id)) out.push_back(Finding{'+', "asset", id, "", "", desc(*x), false});
}

int category_rank(const std::string& c) {
  if (c == "file") return 0;
  if (c == "manifest") return 1;
  if (c == "node") return 2;
  return 3;
}

int change_rank(char c) { return c == '-' ? 0 : c == '+' ? 1 : 2; }

// Text escaping: control characters and backslash become escapes so one finding is always one line.
std::string esc_text(std::string_view s, bool quote_marks) {
  std::string out;
  for (unsigned char c : s) {
    if (c == '\\') out += "\\\\";
    else if (c == '"' && quote_marks) out += "\\\"";
    else if (c < 0x20 || c == 0x7f) {
      char buf[8];
      std::snprintf(buf, sizeof buf, "\\x%02x", c);
      out += buf;
    } else out += static_cast<char>(c);
  }
  return out;
}

std::string show(const std::string& v, bool quoted) {
  return quoted ? "\"" + esc_text(v, true) + "\"" : esc_text(v, false);
}

const char* change_word(char c) { return c == '+' ? "added" : c == '-' ? "removed" : "changed"; }

}  // namespace

LoadOutcome load_files(scene::FileMap files, std::string_view side) {
  LoadOutcome o;
  const auto lr = scene::load_package(files);
  if (!lr.ok()) {
    o.error = std::string(side) + ": package does not load: " + lr.message;
    return o;
  }
  o.package.manifest = lr.manifest;
  o.package.scene = lr.scene;
  o.package.files = std::move(files);
  o.ok = true;
  return o;
}

LoadOutcome load_input(const std::string& input, std::string_view side) {
  namespace fs = std::filesystem;
  LoadOutcome o;
  const std::string where = std::string(side) + " (" + input + "): ";
  std::error_code ec;
  const fs::path p(input);
  if (fs::is_directory(p, ec)) {
    auto r = scene::pack_directory(p);
    if (!r.ok) {
      o.error = where + r.error;
      return o;
    }
    LoadOutcome l = load_files(std::move(r.files), side);
    if (!l.ok) l.error = where + l.error.substr(std::string(side).size() + 2);
    return l;
  }
  if (!fs::is_regular_file(p, ec)) {
    o.error = where + "not a readable file or directory";
    return o;
  }
  const auto size = fs::file_size(p, ec);
  if (ec || size == 0) {
    o.error = where + "cannot read archive";
    return o;
  }
  if (size > kMaxArchiveBytes) {
    o.error = where + "archive is too large";
    return o;
  }
  std::ifstream in(p, std::ios::binary);
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (bytes.empty()) {
    o.error = where + "cannot read archive";
    return o;
  }
  auto z = scene::read_zip_store(bytes.data(), bytes.size());
  if (!z.ok) {
    o.error = where + "invalid archive: " + z.error;
    return o;
  }
  LoadOutcome l = load_files(std::move(z.files), side);
  if (!l.ok) l.error = where + l.error.substr(std::string(side).size() + 2);
  return l;
}

DiffResult diff_packages(const LoadedPackage& a, const LoadedPackage& b) {
  DiffResult r;
  diff_files(a, b, r.findings);
  diff_manifest(a.manifest, b.manifest, r.findings);
  diff_nodes(a.scene, b.scene, r.findings);
  diff_assets(a.scene, b.scene, r.findings);
  std::sort(r.findings.begin(), r.findings.end(), [](const Finding& x, const Finding& y) {
    const int cx = category_rank(x.category), cy = category_rank(y.category);
    if (cx != cy) return cx < cy;
    if (x.key != y.key) return x.key < y.key;
    if (x.field != y.field) return x.field < y.field;
    return change_rank(x.change) < change_rank(y.change);
  });
  return r;
}

std::string format_text(const DiffResult& r) {
  if (r.identical()) return "identical\n";
  std::string out;
  for (const auto& f : r.findings) {
    out += f.change;
    out += ' ' + f.category + ' ' + esc_text(f.key, false);
    if (f.change == '~') {
      if (!f.field.empty()) out += ' ' + f.field;
      out += ": " + show(f.old_value, f.quoted) + " -> " + show(f.new_value, f.quoted);
    } else {
      const std::string& v = f.change == '+' ? f.new_value : f.old_value;
      if (!v.empty()) out += ' ' + esc_text(v, false);
    }
    out += '\n';
  }
  return out;
}

std::string json_escape(std::string_view s) {
  std::string out;
  char buf[8];
  for (std::size_t i = 0; i < s.size();) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c == '"') { out += "\\\""; ++i; }
    else if (c == '\\') { out += "\\\\"; ++i; }
    else if (c == '\n') { out += "\\n"; ++i; }
    else if (c == '\r') { out += "\\r"; ++i; }
    else if (c == '\t') { out += "\\t"; ++i; }
    else if (c == '\b') { out += "\\b"; ++i; }
    else if (c == '\f') { out += "\\f"; ++i; }
    else if (c < 0x20 || c == 0x7f) {
      std::snprintf(buf, sizeof buf, "\\u%04x", c);
      out += buf;
      ++i;
    } else if (c < 0x80) { out += static_cast<char>(c); ++i; }
    else {
      // Accept only well-formed UTF-8 sequences; anything else becomes U+FFFD.
      std::size_t len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC2 ? 2 : 0;
      bool good = len != 0 && i + len <= s.size() && c <= 0xF4;
      for (std::size_t k = 1; good && k < len; ++k) good = (static_cast<unsigned char>(s[i + k]) & 0xC0) == 0x80;
      if (good && len == 3) {
        const unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        if ((c == 0xE0 && c1 < 0xA0) || (c == 0xED && c1 >= 0xA0)) good = false;
      }
      if (good && len == 4) {
        const unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        if ((c == 0xF0 && c1 < 0x90) || (c == 0xF4 && c1 >= 0x90)) good = false;
      }
      if (good) { out.append(s.substr(i, len)); i += len; }
      else { out += "\\ufffd"; ++i; }
    }
  }
  return out;
}

std::string format_json(const DiffResult& r, std::string_view a, std::string_view b) {
  auto q = [](std::string_view v) { return "\"" + json_escape(v) + "\""; };
  std::size_t added = 0, removed = 0, changed_n = 0;
  std::string out = "{\n  \"a\": " + q(a) + ",\n  \"b\": " + q(b) + ",\n  \"findings\": [";
  for (std::size_t i = 0; i < r.findings.size(); ++i) {
    const Finding& f = r.findings[i];
    (f.change == '+' ? added : f.change == '-' ? removed : changed_n)++;
    out += i ? ",\n    {" : "\n    {";
    out += "\"category\": " + q(f.category) + ", \"change\": " + q(change_word(f.change));
    if (!f.field.empty()) out += ", \"field\": " + q(f.field);
    out += ", \"key\": " + q(f.key);
    if (f.change != '-' && !f.new_value.empty()) out += ", \"new\": " + q(f.new_value);
    if (f.change != '+' && !f.old_value.empty()) out += ", \"old\": " + q(f.old_value);
    out += "}";
  }
  out += r.findings.empty() ? "],\n" : "\n  ],\n";
  out += std::string("  \"identical\": ") + (r.identical() ? "true" : "false") + ",\n";
  out += "  \"summary\": {\"added\": " + std::to_string(added) + ", \"changed\": " + std::to_string(changed_n) +
         ", \"removed\": " + std::to_string(removed) + "}\n}\n";
  return out;
}

int run_cli(const std::vector<std::string>& args, std::string& out, std::string& err) {
  const char* const usage = "usage: lumora-scenediff A B [--format text|json] [--quiet]\n";
  std::string format = "text";
  bool quiet = false;
  std::string inputs[2];
  int n = 0;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "--quiet") {
      quiet = true;
    } else if (arg == "--format") {
      if (i + 1 >= args.size()) { err += usage; return 2; }
      format = args[++i];
    } else if (arg.rfind("--", 0) == 0) {
      err += usage;
      return 2;
    } else if (n < 2) {
      inputs[n++] = arg;
    } else {
      err += usage;
      return 2;
    }
  }
  if (n != 2 || (format != "text" && format != "json")) { err += usage; return 2; }
  const LoadOutcome a = load_input(inputs[0], "A");
  if (!a.ok) { err += "error: " + a.error + "\n"; return 2; }
  const LoadOutcome b = load_input(inputs[1], "B");
  if (!b.ok) { err += "error: " + b.error + "\n"; return 2; }
  const DiffResult r = diff_packages(a.package, b.package);
  if (!quiet) out += format == "json" ? format_json(r, inputs[0], inputs[1]) : format_text(r);
  return r.identical() ? 0 : 1;
}

}  // namespace lumora::scenediff
