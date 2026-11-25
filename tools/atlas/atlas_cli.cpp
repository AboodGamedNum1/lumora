#include "atlas_cli.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <system_error>

#include "atlas.h"
#include "png.h"

namespace lumora::atlas_tool {
namespace {

namespace fs = std::filesystem;
using lumora::image::AtlasInput;
using lumora::image::AtlasLimits;

constexpr std::uint64_t kMaxFileBytes = 32ull << 20;          // one input file
constexpr std::uint64_t kMaxTotalFileBytes = 256ull << 20;    // all input files together
constexpr std::uint64_t kMaxTotalPixels = 64ull << 20;        // all decoded inputs together (256 MB of RGBA)

int usage(std::ostream& err, const std::string& why) {
  err << "lumora-atlas: " << why << "\n"
      << "usage: lumora-atlas --out atlas.png --index atlas.json [--max-size N] [--padding N] [--pot] input1.png input2.png ...\n";
  return 2;
}

bool parse_number(const std::string& s, std::uint32_t& out) {
  if (s.empty() || s.size() > 6) return false;
  std::uint32_t v = 0;
  for (const char c : s) {
    if (c < '0' || c > '9') return false;
    v = v * 10 + static_cast<std::uint32_t>(c - '0');
  }
  out = v;
  return true;
}

// Base file name without its last extension. A name that starts with a dot and has no other dot is kept whole.
std::string base_name(const std::string& path) {
  std::size_t slash = path.find_last_of("/\\");
  std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  const std::size_t dot = name.find_last_of('.');
  if (dot != std::string::npos && dot > 0) name.resize(dot);
  return name;
}

bool read_file(const std::string& path, std::vector<std::uint8_t>& bytes, std::uint64_t& total, std::string& why) {
  std::error_code ec;
  const fs::path p(path);
  if (!fs::is_regular_file(p, ec)) {
    why = "not a regular file: " + path;
    return false;
  }
  const std::uint64_t size = fs::file_size(p, ec);
  if (ec) {
    why = "cannot read: " + path;
    return false;
  }
  if (size > kMaxFileBytes) {
    why = "input file above 32 MiB: " + path;
    return false;
  }
  if (total + size > kMaxTotalFileBytes) {
    why = "input files above 256 MiB in total";
    return false;
  }
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    why = "cannot open: " + path;
    return false;
  }
  bytes.resize(static_cast<std::size_t>(size));
  if (size > 0) f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
  if (!f && size > 0) {
    why = "cannot read: " + path;
    return false;
  }
  total += size;
  return true;
}

bool write_file(const fs::path& path, const void* data, std::size_t size) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  if (size > 0) f.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
  f.flush();
  return static_cast<bool>(f);
}

}  // namespace

int run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  std::string out_path, index_path;
  AtlasLimits limits;
  std::vector<std::string> files;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    auto value = [&](std::string& dst) -> bool {
      if (i + 1 >= args.size()) return false;
      dst = args[++i];
      return true;
    };
    if (a == "--out") {
      if (!value(out_path)) return usage(err, "--out needs a value");
    } else if (a == "--index") {
      if (!value(index_path)) return usage(err, "--index needs a value");
    } else if (a == "--max-size" || a == "--padding") {
      std::string v;
      if (!value(v)) return usage(err, a + " needs a value");
      std::uint32_t n = 0;
      if (!parse_number(v, n)) return usage(err, a + " needs a number");
      (a == "--max-size" ? limits.max_size : limits.padding) = n;
    } else if (a == "--pot") {
      limits.power_of_two = true;
    } else if (a.size() > 1 && a[0] == '-' && a[1] == '-') {
      return usage(err, "unknown option " + a);
    } else {
      files.push_back(a);
    }
  }
  if (out_path.empty()) return usage(err, "--out is required");
  if (index_path.empty()) return usage(err, "--index is required");
  if (out_path == index_path) return usage(err, "--out and --index must be different files");
  if (files.empty()) return usage(err, "no input files");

  if (files.size() > limits.max_inputs) {
    err << "lumora-atlas: more than " << limits.max_inputs << " input files\n";
    return 1;
  }

  std::vector<AtlasInput> inputs;
  inputs.reserve(files.size());
  std::uint64_t total_bytes = 0, total_pixels = 0;
  for (const std::string& file : files) {
    std::vector<std::uint8_t> bytes;
    std::string why;
    if (!read_file(file, bytes, total_bytes, why)) {
      err << "lumora-atlas: " << why << "\n";
      return 1;
    }
    auto decoded = lumora::image::decode_png(bytes.data(), bytes.size());
    if (!decoded.ok()) {
      err << "lumora-atlas: cannot decode " << file << ": " << decoded.message << "\n";
      return 1;
    }
    total_pixels += std::uint64_t{decoded.image.width} * decoded.image.height;
    if (total_pixels > kMaxTotalPixels) {
      err << "lumora-atlas: input images above 64 Mi pixels in total\n";
      return 1;
    }
    AtlasInput in;
    in.name = base_name(file);
    in.image = std::move(decoded.image);
    inputs.push_back(std::move(in));
  }

  const auto packed = lumora::image::pack_atlas(inputs, limits);
  if (!packed.ok()) {
    err << "lumora-atlas: " << packed.message << "\n";
    return 1;
  }
  const auto png = lumora::image::encode_png(packed.image);
  const std::string json = lumora::image::atlas_index_json(packed);
  if (png.empty() || json.empty()) {
    err << "lumora-atlas: cannot encode the result\n";
    return 1;
  }

  // Write to temporary files next to the targets, then rename. Nothing is replaced unless both files were written.
  const fs::path out_final(out_path), index_final(index_path);
  const fs::path out_tmp = fs::path(out_path + ".tmp"), index_tmp = fs::path(index_path + ".tmp");
  std::error_code ec;
  if (!write_file(out_tmp, png.data(), png.size()) || !write_file(index_tmp, json.data(), json.size())) {
    fs::remove(out_tmp, ec);
    fs::remove(index_tmp, ec);
    err << "lumora-atlas: cannot write the output files\n";
    return 1;
  }
  fs::rename(out_tmp, out_final, ec);
  if (ec) {
    fs::remove(out_tmp, ec);
    fs::remove(index_tmp, ec);
    err << "lumora-atlas: cannot write the output files\n";
    return 1;
  }
  fs::rename(index_tmp, index_final, ec);
  if (ec) {
    fs::remove(index_tmp, ec);
    err << "lumora-atlas: cannot write the index file\n";
    return 1;
  }
  out << "lumora-atlas: wrote " << packed.image.width << "x" << packed.image.height << " atlas with " << packed.entries.size()
      << " sprites\n";
  return 0;
}

}  // namespace lumora::atlas_tool
