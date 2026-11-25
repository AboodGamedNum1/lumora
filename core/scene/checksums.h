// SHA-256 and the checksums.txt file of a .lumora package (docs/format.md).
// Format: one "<64 lowercase hex>  <package-relative path>" line per file. Verification happens before any content is used.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace lumora::scene {

using Sha256Digest = std::array<std::uint8_t, 32>;

Sha256Digest sha256(const void* data, std::size_t size);
std::string to_hex(const Sha256Digest& d);

// Parses checksums.txt. Rejects malformed lines, uppercase hex, duplicate paths and empty paths.
// On failure returns false and sets error; out is left unspecified.
bool parse_checksums(std::string_view text, std::map<std::string, Sha256Digest>& out, std::string& error);

// True when the bytes hash to the recorded digest for path. Unlisted paths fail.
bool verify_file(const std::map<std::string, Sha256Digest>& sums, const std::string& path, const void* data,
                 std::size_t size);

}  // namespace lumora::scene
