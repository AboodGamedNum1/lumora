// Archive entry policy for .lumora packages (docs/security-model.md, "Archive defenses").
// Pure functions on names and sizes. No ZIP library, no file system, no allocation limits
// beyond std::string. The ZIP reader (later milestone) calls these before it touches an entry.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lumora::package {

enum class NameVerdict {
  Ok,
  Empty,
  Absolute,           // leading '/' or '\', drive letter, UNC prefix
  Traversal,          // a ".." component
  Backslash,
  ControlCharacter,   // bytes 0x00-0x1F and 0x7F
  TrailingDotOrSpace, // any component ends with '.' or ' '
  WindowsReserved,    // CON, PRN, AUX, NUL, COM1-9, LPT1-9, with or without extension
  EmptyComponent,     // "a//b"
  InvalidUtf8,
  TooLong             // over limits.max_name_bytes
};

struct Limits {
  std::uint64_t max_entries = 10000;
  std::uint64_t max_name_bytes = 255;
  std::uint64_t max_entry_bytes = 512ull << 20;
  std::uint64_t max_total_uncompressed_bytes = 2048ull << 20;
  std::uint64_t max_compression_ratio = 200;  // uncompressed / compressed, per entry
};

struct EntryInfo {
  std::string name;
  std::uint64_t compressed_bytes = 0;
  std::uint64_t uncompressed_bytes = 0;
};

enum class SetVerdict {
  Ok,
  BadName,              // validate_name failed; see name_verdict
  TooManyEntries,
  EntryTooLarge,
  TotalTooLarge,
  RatioTooHigh,
  SizeOverflow,         // a sum would overflow 64 bits
  DuplicateName,        // exact duplicate
  CaseFoldCollision,    // names equal after ASCII case folding
  NormalizationCollision  // names equal after NFC-style folding of the cases the policy supports
};

struct SetResult {
  SetVerdict verdict = SetVerdict::Ok;
  std::size_t first_index = 0;   // index of the offending entry
  std::size_t other_index = 0;   // for collisions, the earlier entry
  NameVerdict name_verdict = NameVerdict::Ok;  // set when verdict is BadName
};

NameVerdict validate_name(std::string_view name, const Limits& limits = {});
const char* to_string(NameVerdict v);
const char* to_string(SetVerdict v);

// Order: entry count, then per entry (in order) name, size, ratio, running total, then
// collisions. Stops at the first failure and reports the offending entry index.
SetResult validate_entries(const std::vector<EntryInfo>& entries, const Limits& limits = {});

}  // namespace lumora::package
