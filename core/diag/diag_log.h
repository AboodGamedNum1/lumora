// Diagnostics log ring (docs/android-lifecycle.md section 10.1 item 3: every render-scale step is logged to the in-app diagnostics
// screen). A fixed-size ring of the most recent 64 events, oldest overwritten first, with a counter of overwritten events. Integer
// only: no clock reads (the caller supplies the timestamp), no floating point, no libm, no 128-bit types (MSVC), no allocation, no
// text formatting. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::diag {

constexpr std::uint32_t kLogCapacity = 64;

struct LogEntry {
  std::uint64_t timestamp_ns = 0;  // caller supplied, any value
  std::uint16_t code = 0;          // caller defined event code
  std::int32_t a = 0, b = 0;       // two caller defined values
};

struct DiagLog {
  LogEntry entries[kLogCapacity] = {};
  std::uint32_t count = 0;     // valid entries, 0 to kLogCapacity
  std::uint32_t next = 0;      // slot the next entry is written to, 0 to kLogCapacity - 1
  std::uint64_t dropped = 0;   // entries overwritten so far (saturates at UINT64_MAX)
};

// Valid state: count <= kLogCapacity and next < kLogCapacity. An invalid object is treated as empty by every function below: log_push
// first resets count and next to 0 (dropped and the slots are left as they are), the read functions see no entries.

// Clears everything: all slots zero, count 0, next 0, dropped 0. Returns false when log is null.
bool log_clear(DiagLog* log);

// Appends one entry. Returns false and changes nothing when log is null. Otherwise (after the repair above) writes the entry at next,
// advances next with wrap-around, and: if count < kLogCapacity, count += 1; else the oldest entry was overwritten, so dropped += 1
// (saturating at UINT64_MAX). Returns true.
bool log_push(DiagLog* log, std::uint64_t timestamp_ns, std::uint16_t code, std::int32_t a, std::int32_t b);

// Reads the entry at position index, 0 being the oldest and count - 1 the newest. The window is the count most recent slots ending
// at next - 1 with wrap-around, so a hand-built object with count below kLogCapacity and next different from count reads exactly
// those slots. Returns false and leaves *out untouched when log or out is null, the state is invalid, or index >= count.
bool log_read(const DiagLog* log, std::uint32_t index, LogEntry* out);

// Number of valid entries: 0 for a null or invalid log.
std::uint32_t log_size(const DiagLog* log);

// FNV-1a 64 over 32-bit little-endian words: per entry from the oldest to the newest: timestamp low 32 bits, timestamp high 32 bits,
// code (zero extended), a, b (as uint32 two's complement); then dropped low 32 bits and dropped high 32 bits. A null log hashes zero
// bytes; an invalid log hashes as an empty log (only the two dropped words).
std::uint64_t log_digest(const DiagLog* log);

}  // namespace lumora::diag
