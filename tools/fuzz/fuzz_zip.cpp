// Fuzz target: zip. Raw bytes go to read_zip_store, and load_package runs on the entries when parsing succeeds.
// Two invariants are checked from the input as well:
//  * a FileMap built from the input survives write_zip/read_zip_store unchanged;
//  * load_package never accepts a package whose files differ from checksums.txt.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>

#include "checksums.h"
#include "zip_store.h"

namespace {
using namespace lumora::scene;

void require(bool ok) {
  if (!ok) std::abort();
}

// Independent check of the checksum rule: when load_package accepts, every file other than checksums.txt must
// be listed and match its digest.
void check_accepted(const FileMap& files) {
  const auto cs = files.find("checksums.txt");
  require(cs != files.end());
  std::map<std::string, Sha256Digest> sums;
  std::string err;
  require(parse_checksums(std::string_view(reinterpret_cast<const char*>(cs->second.data()), cs->second.size()),
                          sums, err));
  for (const auto& [path, bytes] : files) {
    if (path == "checksums.txt") continue;
    require(verify_file(sums, path, bytes.data(), bytes.size()));
  }
}

void try_load(const FileMap& files) {
  if (load_package(files).ok()) check_accepted(files);
}

FileMap files_from_input(const std::uint8_t* data, std::size_t size) {
  FileMap files;
  if (size == 0) return files;
  const std::size_t count = 1 + data[0] % 5;
  std::size_t pos = 1;
  for (std::size_t i = 0; i < count && pos <= size; ++i) {
    const std::size_t want = pos < size ? data[pos] : 0;
    ++pos;
    const std::size_t len = want < size - pos ? want : size - pos;
    files["f" + std::to_string(i) + ".bin"].assign(data + pos, data + pos + len);
    pos += len;
  }
  return files;
}

// Builds a package from the input with a correct checksums.txt, then optionally corrupts it.
void package_check(const std::uint8_t* data, std::size_t size) {
  FileMap files = files_from_input(data, size);
  files["manifest.json"].assign(data, data + (size < 64 ? size : 64));
  std::string sums;
  for (const auto& [path, bytes] : files) {
    sums += to_hex(sha256(bytes.data(), bytes.size())) + "  " + path + "\n";
  }
  files["checksums.txt"].assign(sums.begin(), sums.end());
  try_load(files);
  const unsigned mode = size ? data[size - 1] % 3 : 0;
  if (mode == 0) {  // flip one byte of a listed file
    for (auto& [path, bytes] : files) {
      if (path == "checksums.txt" || bytes.empty()) continue;
      bytes[bytes.size() / 2] ^= 0x01;
      require(!load_package(files).ok());
      break;
    }
  } else if (mode == 1) {  // add a file that is not listed
    files["extra.bin"] = {1, 2, 3};
    require(!load_package(files).ok());
  } else {  // drop a listed file
    for (auto it = files.begin(); it != files.end(); ++it) {
      if (it->first == "checksums.txt") continue;
      files.erase(it);
      break;
    }
    require(!load_package(files).ok());
  }
}

void roundtrip_check(const std::uint8_t* data, std::size_t size) {
  const FileMap files = files_from_input(data, size);
  const int level = (size && data[size - 1] & 1) ? 1 : 6;
  const auto zip = (size && data[size - 1] & 2) ? write_zip_store(files) : write_zip(files, level);
  const ZipReadResult back = read_zip_store(zip.data(), zip.size(), 10000, 1ull << 30, 1ull << 20);
  require(back.ok);
  require(back.files == files);
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const ZipReadResult r = read_zip_store(data, size, 256, 1ull << 24, 200);
  if (r.ok) try_load(r.files);
  if (size <= (1u << 16)) {
    roundtrip_check(data, size);
    package_check(data, size);
  }
  return 0;
}
