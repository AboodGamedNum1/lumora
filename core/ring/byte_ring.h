// Fixed-capacity byte FIFO (4096 bytes) for staging audio or log bytes between two stages on one thread. Pure integer: no floating point,
// no libm, no 128-bit types (MSVC), no allocation, no global state, no locking. Evidence label: unit tests only.
#pragma once
#include <cstdint>

namespace lumora::ring {

constexpr std::uint32_t kCapacity = 4096;

struct ByteRing {
  std::uint8_t data[kCapacity];
  std::uint32_t head = 0;   // index of the oldest byte, 0 .. kCapacity - 1
  std::uint32_t count = 0;  // stored bytes, 0 .. kCapacity
};

// A ring is valid when head < kCapacity and count <= kCapacity. Every function below given a null ring or an invalid ring returns 0 (or
// false) and changes nothing. A null byte pointer with n > 0 also returns 0 and changes nothing; with n == 0 the pointer is not read and
// the call returns 0. The caller's buffers must not point into ring->data (no promise, not checked, not tested).

// Empties the ring: head = 0, count = 0 (the data bytes are not touched). A null ring is ignored. An invalid ring is reset too.
void reset(ByteRing* ring);

// Stored bytes; 0 for a null or invalid ring.
std::uint32_t size(const ByteRing* ring);

// kCapacity - size; 0 for a null or invalid ring.
std::uint32_t free_space(const ByteRing* ring);

// Appends min(n, free_space) bytes from src (partial write allowed, the first bytes of src are kept) and returns how many were written.
// The newest byte goes to index (head + count) % kCapacity before count grows; writes wrap around the end of data.
std::uint32_t write(ByteRing* ring, const std::uint8_t* src, std::uint32_t n);

// Removes the oldest min(n, size) bytes, copying them to dst in order, and returns how many were read. head advances by that amount modulo
// kCapacity and count shrinks by it. When the ring becomes empty head is NOT reset (it keeps its value).
std::uint32_t read(ByteRing* ring, std::uint8_t* dst, std::uint32_t n);

// Copies up to n bytes to dst without removing anything, starting `offset` bytes after the oldest. Returns min(n, size - offset) when
// offset < size, otherwise 0. The ring is never changed.
std::uint32_t peek(const ByteRing* ring, std::uint8_t* dst, std::uint32_t n, std::uint32_t offset);

// Removes the oldest min(n, size) bytes without copying; returns how many. Same head and count rule as read.
std::uint32_t skip(ByteRing* ring, std::uint32_t n);

// Decisions: indices are computed in 64 bits or kept below kCapacity before every addition, so n near UINT32_MAX never overflows. n
// larger than the space or the stored bytes is clamped, never an error. A header ambiguity found later is a question to the maintainer, not
// an interpretation.

}  // namespace lumora::ring
