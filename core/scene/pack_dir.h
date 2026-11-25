// Directory to package helpers used by the packer CLI and tests.
#pragma once
#include <filesystem>
#include <string>

#include "scene.h"

namespace lumora::scene {

struct PackResult {
  bool ok = false;
  std::string error;
  FileMap files;  // includes the generated checksums.txt
};

// Reads every regular file under dir (relative '/' paths), refuses symlinks, an existing checksums.txt, and
// anything over the caps, generates checksums.txt, then runs load_package on the result so a bad
// package is never written.
PackResult pack_directory(const std::filesystem::path& dir, std::uint64_t max_files = 10000,
                          std::uint64_t max_total_bytes = 512ull << 20);

}  // namespace lumora::scene
