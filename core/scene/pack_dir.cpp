#include "pack_dir.h"

#include <fstream>
#include <iterator>

#include "checksums.h"

namespace lumora::scene {

PackResult pack_directory(const std::filesystem::path& dir, std::uint64_t max_files, std::uint64_t max_total_bytes) {
  namespace fs = std::filesystem;
  PackResult r;
  auto fail = [&](std::string m) { r.ok = false; r.error = std::move(m); r.files.clear(); return r; };
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) return fail("not a directory: " + dir.string());
  std::uint64_t total = 0;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    const auto& e = *it;
    if (e.is_symlink(ec)) return fail("symlinks are not allowed: " + e.path().string());
    if (e.is_directory(ec)) continue;
    if (!e.is_regular_file(ec)) return fail("unsupported file type: " + e.path().string());
    const std::string rel = fs::relative(e.path(), dir, ec).generic_string();
    if (rel == "checksums.txt") return fail("the source directory must not contain checksums.txt");
    if (r.files.size() >= max_files) return fail("too many files");
    std::ifstream in(e.path(), std::ios::binary);
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (!in && !in.eof()) return fail("cannot read " + rel);
    total += data.size();
    if (total > max_total_bytes) return fail("package too large");
    r.files[rel] = std::move(data);
  }
  if (ec) return fail("directory walk failed: " + ec.message());
  std::string sums;
  for (const auto& [p, d] : r.files) sums += to_hex(sha256(d.data(), d.size())) + "  " + p + "\n";
  r.files["checksums.txt"] = std::vector<std::uint8_t>(sums.begin(), sums.end());
  const auto lr = load_package(r.files);
  if (!lr.ok()) return fail("package does not load: " + lr.message);
  r.ok = true;
  return r;
}

}  // namespace lumora::scene
