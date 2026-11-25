// Minimal ZIP container for .lumora packages: STORED entries only, no ZIP64, no encryption, no data descriptors.
// Deflate and ZIP64 need a pinned library decision (docs/dependency-manifest.md) and are not implemented here.
// The reader never trusts sizes or offsets: every read is bounds-checked and anything it does not understand is rejected.
// Entry-name policy (core/package/entry_policy.h) is applied by the caller before names are used.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "scene.h"

namespace lumora::scene {

// Writes entries (sorted by name, as FileMap iterates) into a ZIP byte stream. Fixed timestamps for reproducible output.
std::vector<std::uint8_t> write_zip_store(const FileMap& files);

struct ZipReadResult {
  bool ok = false;
  std::string error;
  FileMap files;
};

ZipReadResult read_zip_store(const std::uint8_t* data, std::size_t size, std::uint64_t max_entries = 10000,
                             std::uint64_t max_total_bytes = 2048ull << 20);

}  // namespace lumora::scene
