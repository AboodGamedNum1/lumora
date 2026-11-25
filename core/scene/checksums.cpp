#include "checksums.h"

#include <cstring>

namespace lumora::scene {
namespace {
constexpr std::uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98,
    0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8,
    0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
    0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2};

constexpr std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

void compress(std::uint32_t h[8], const std::uint8_t* p) {
  std::uint32_t w[64];
  for (int i = 0; i < 16; ++i)
    w[i] = (std::uint32_t{p[4 * i]} << 24) | (std::uint32_t{p[4 * i + 1]} << 16) | (std::uint32_t{p[4 * i + 2]} << 8) |
           std::uint32_t{p[4 * i + 3]};
  for (int i = 16; i < 64; ++i) {
    const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
  for (int i = 0; i < 64; ++i) {
    const std::uint32_t t1 = hh + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
    const std::uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
    hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
  }
  h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}
}  // namespace

Sha256Digest sha256(const void* data, std::size_t size) {
  std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  const auto* p = static_cast<const std::uint8_t*>(data);
  std::size_t left = size;
  while (left >= 64) { compress(h, p); p += 64; left -= 64; }
  std::uint8_t tail[128] = {};
  if (left) std::memcpy(tail, p, left);
  tail[left] = 0x80;
  const std::size_t total = left < 56 ? 64 : 128;
  const std::uint64_t bits = static_cast<std::uint64_t>(size) * 8;
  for (int i = 0; i < 8; ++i) tail[total - 1 - i] = static_cast<std::uint8_t>(bits >> (8 * i));
  compress(h, tail);
  if (total == 128) compress(h, tail + 64);
  Sha256Digest out{};
  for (int i = 0; i < 8; ++i)
    for (int j = 0; j < 4; ++j) out[4 * i + j] = static_cast<std::uint8_t>(h[i] >> (24 - 8 * j));
  return out;
}

std::string to_hex(const Sha256Digest& d) {
  static const char* hex = "0123456789abcdef";
  std::string s;
  for (auto b : d) { s += hex[b >> 4]; s += hex[b & 15]; }
  return s;
}

bool parse_checksums(std::string_view text, std::map<std::string, Sha256Digest>& out, std::string& error) {
  out.clear();
  std::size_t pos = 0;
  int line_no = 0;
  while (pos < text.size()) {
    std::size_t nl = text.find('\n', pos);
    std::string_view line = text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
    pos = nl == std::string_view::npos ? text.size() : nl + 1;
    ++line_no;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.empty()) continue;
    auto fail = [&](const char* why) {
      error = "checksums.txt line " + std::to_string(line_no) + ": " + why;
      return false;
    };
    if (line.size() < 67 || line[64] != ' ' || line[65] != ' ') return fail("expected '<sha256>  <path>'");
    Sha256Digest d{};
    for (int i = 0; i < 32; ++i) {
      int v[2];
      for (int k = 0; k < 2; ++k) {
        const char c = line[2 * i + k];
        if (c >= '0' && c <= '9') v[k] = c - '0';
        else if (c >= 'a' && c <= 'f') v[k] = c - 'a' + 10;
        else return fail("digest must be 64 lowercase hex digits");
      }
      d[i] = static_cast<std::uint8_t>(v[0] * 16 + v[1]);
    }
    std::string path(line.substr(66));
    if (path.empty()) return fail("empty path");
    if (!out.emplace(path, d).second) return fail("duplicate path");
  }
  return true;
}

bool verify_file(const std::map<std::string, Sha256Digest>& sums, const std::string& path, const void* data,
                 std::size_t size) {
  const auto it = sums.find(path);
  return it != sums.end() && it->second == sha256(data, size);
}

}  // namespace lumora::scene
