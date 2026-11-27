// Run-length coding of byte buffers (coverage masks, atlas padding, simple scene records). Pure integer: no floating point, no libm, no
// 128-bit types (MSVC), no allocation, no global state. Evidence label: unit tests only.
#pragma once
#include <cstddef>
#include <cstdint>

namespace lumora::rle {

constexpr std::uint32_t kMaxInput = 1u << 24;    // largest raw size, in bytes
constexpr std::uint32_t kMaxEncoded = 1u << 25;  // largest encoded size, in bytes (every raw byte can cost 2)

enum class RleResult : std::uint8_t { Ok = 0, NullArgument = 1, TooLarge = 2, BufferTooSmall = 3, Malformed = 4, NonCanonical = 5 };

// Format: a sequence of pairs (b0, b1). The pair stands for b1 repeated (b0 + 1) times, so a run is 1 to 256 bytes. The empty buffer
// encodes to zero bytes.
//
// encode: greedy maximal runs, cut at 256. The encoder's output is the only canonical form: runs are as long as possible, so two
// neighbouring pairs hold the same value only when the first of them is a full run (b0 == 255).
// Check order: out_len null, src null with n > 0, dst null with dst_cap > 0 give NullArgument; n above kMaxInput gives TooLarge;
// the encoded size above dst_cap gives BufferTooSmall (the size is computed first, in a pass that writes nothing). On any failure *out_len
// and dst are untouched. On Ok exactly *out_len bytes of dst are written (nothing beyond) and *out_len is at most 2 * n.
RleResult encode(const std::uint8_t* src, std::uint32_t n, std::uint8_t* dst, std::uint32_t dst_cap, std::uint32_t* out_len);

// decode: strict. Check order: the same three NullArgument checks (src null with n > 0, dst null with dst_cap > 0, out_len null); n odd gives
// Malformed; n above kMaxEncoded gives TooLarge; then the pairs are scanned in order and the first problem found wins: a pair whose value
// equals the value of the previous pair while the previous pair's b0 is not 255 gives NonCanonical; a running raw total above kMaxInput
// gives TooLarge; after the scan, a raw total above dst_cap gives BufferTooSmall. The scan writes nothing; on any failure *out_len and
// dst are untouched. On Ok exactly *out_len bytes of dst are written and *out_len is the raw total (0 for n == 0).
RleResult decode(const std::uint8_t* src, std::uint32_t n, std::uint8_t* dst, std::uint32_t dst_cap, std::uint32_t* out_len);

// Decisions: src and dst must not overlap; this is not checked and not tested (no aliasing promise). Sizes are kept in 64 bits while
// summing so a long input cannot wrap. A header ambiguity found later is a question to the maintainer, not an interpretation.

}  // namespace lumora::rle
