// Frame statistics and fps overlay text (docs/android-lifecycle.md section 10.1, the diagnostics fps overlay).
// A fixed-size window of the most recent frame times, three statistics over it, and a formatter that writes the
// overlay line into a caller buffer. Integer only: no clock reads, no floating point, no libm, no 128-bit types
// (MSVC), no allocation, no standard I/O or formatting functions. Evidence label: unit tests only.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::lifecycle {

constexpr std::size_t kFrameWindow = 120;  // the window holds the most recent 120 frame times

struct FrameStats {
  std::uint64_t samples[kFrameWindow] = {};  // ring buffer of frame times in nanoseconds
  std::uint32_t count = 0;                   // valid samples, 0 to kFrameWindow
  std::uint32_t next = 0;                    // index the next sample is written to, 0 to kFrameWindow - 1
};

struct FrameSummary {
  std::uint32_t count = 0;            // samples in the window
  std::uint64_t average_ns = 0;       // floor(sum / count); the sum saturates at UINT64_MAX before dividing
  std::uint64_t worst_ns = 0;         // largest sample
  std::uint64_t p99_ns = 0;           // see summarize
  std::uint32_t fps_tenths = 0;       // floor(10 * 1e9 / average_ns), saturated at UINT32_MAX; 0 when average_ns is 0
};

// The window is the count most recent slots, ending at next - 1 with wrap-around, so a hand-built object whose count is
// below kFrameWindow and whose next differs from count reads exactly those slots. Zero samples inside a hand-built
// window are included as they are (statistics may then be zero). Valid state: count <= kFrameWindow and next <
// kFrameWindow.
// Records one frame time. Returns false and changes nothing when stats is null or frame_ns is 0. Otherwise writes
// the sample at next, advances next with wrap-around, and increases count up to kFrameWindow (the oldest sample is
// overwritten once the window is full). A stats object with count above kFrameWindow or next at least kFrameWindow is invalid and is treated as empty
// first (count = 0, next = 0), then the sample is recorded.
bool record_frame(FrameStats* stats, std::uint64_t frame_ns);

// Summarizes the window. Returns false and leaves *out untouched when stats or out is null. An empty window (or one
// with out-of-range count or next, as above) gives a zeroed summary and true. p99_ns is the sample at rank
// count - floor(count / 100) in ascending order (nearest rank, 1-based; this equals ceil(0.99 * count)). The formula is
// binding: count 1 to 99 gives the largest sample, count 100 gives the 99th smallest, count 120 the 119th.
// The sum of samples saturates at UINT64_MAX before the division by count. fps_tenths is exact: floor(10000000000 / average_ns) computed without a wide type, saturating at UINT32_MAX.
bool summarize(const FrameStats* stats, FrameSummary* out);

// Writes the overlay line "<fps>.<tenth> fps  avg <ms>.<hundredth> ms  p99 <ms>.<hundredth> ms  worst <ms>.<hundredth> ms"
// from the supplied summary fields as they are, with no recomputation or validation, as an unsigned decimal with no padding, for example "59.9 fps  avg 16.69 ms  p99 18.20 ms  worst 25.04 ms". Milliseconds
// are the nanosecond value divided by 10000, rounded down to hundredths (truncation, not rounding). A summary with
// count 0 writes "no frames" whatever its other fields hold. The text is always NUL-terminated when capacity is at least 1. Returns the number of
// characters the full text needs, excluding the NUL (snprintf style), whatever the capacity; when capacity is smaller
// than needed + 1 the output is the first capacity - 1 characters plus the NUL (plain prefix, no ellipsis); capacity 1
// writes only the NUL and still returns the full length. Returns 0 and writes nothing when out is null or
// capacity is 0 (a deliberate departure from snprintf, which returns the length at capacity 0).
std::size_t format_overlay(const FrameSummary& summary, char* out, std::size_t capacity);

}  // namespace lumora::lifecycle
