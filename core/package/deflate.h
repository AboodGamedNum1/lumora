// Dependency-free DEFLATE (RFC 1951 raw streams, as stored in ZIP method 8) for .lumora packages.
// Security-sensitive: inflate runs on untrusted input. It must never read or write out of bounds, never allocate
// more than max_output (checked before growing, not after), never loop without consuming input, and reject every
// malformed stream with a status, never by crashing. The ZIP layer (core/scene/zip_store.cpp) is wired to this
// interface by the project owner after the module lands; the contributor does not edit it.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lumora::package {

enum class InflateStatus {
  Ok,
  Truncated,      // input ended before the final block ended
  BadBlockType,   // reserved block type 3
  BadStoredLen,   // stored block LEN does not match NLEN
  BadHuffman,     // over-subscribed or (where RFC 1951 forbids it) incomplete code, bad code-length sequence
  BadSymbol,      // invalid literal/length or distance symbol (286, 287, 30, 31)
  BadDistance,    // distance reaches before the start of the output
  OutputLimit     // output would exceed max_output
};

struct InflateResult {
  InflateStatus status = InflateStatus::Truncated;
  std::vector<std::uint8_t> data;  // empty unless status is Ok
  std::size_t consumed = 0;        // input bytes consumed, whole bytes, only meaningful when Ok
};

// Decodes one complete raw DEFLATE stream (all blocks up to and including the final one).
// Trailing input after the final block is not an error here; `consumed` tells the caller how much was used.
InflateResult inflate_raw(const std::uint8_t* data, std::size_t size, std::uint64_t max_output);

// Compresses to a raw DEFLATE stream. level 0: stored blocks only. level 1: fixed Huffman with a greedy
// LZ77 matcher. level 6 (default): dynamic Huffman blocks with a lazy matcher and a hash-chain search of bounded
// length. Any other level is clamped to the nearest of these three. Output is deterministic: the same input and
// level give the same bytes on every platform. Every level must round-trip through inflate_raw and through zlib.
std::vector<std::uint8_t> deflate_raw(const std::uint8_t* data, std::size_t size, int level = 6);

}  // namespace lumora::package
