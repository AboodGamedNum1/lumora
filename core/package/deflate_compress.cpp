// Deterministic raw-DEFLATE compressor (RFC 1951, ZIP method 8).
// level 0: stored blocks. level 1: fixed Huffman with a greedy LZ77 matcher.
// level 6: dynamic Huffman with a lazy matcher and bounded hash-chain search.
// The same input and level always produce the same bytes on every platform.
#include <algorithm>
#include <array>
#include <vector>

#include "deflate.h"
#include "deflate_common.h"

namespace lumora::package {
namespace {

using namespace deflate_detail;

// LSB-first bit writer.
struct BitWriter {
  std::vector<std::uint8_t> out;
  std::uint64_t buf = 0;
  int count = 0;

  void put(std::uint32_t value, int n) {
    buf |= static_cast<std::uint64_t>(value) << count;
    count += n;
    while (count >= 8) {
      out.push_back(static_cast<std::uint8_t>(buf & 0xFF));
      buf >>= 8;
      count -= 8;
    }
  }

  // Canonical codes are MSB-first; the stream wants them reversed.
  void put_code(std::uint32_t code, int n) { put(reverse_bits(code, n), n); }

  void align_to_byte() {
    if (count % 8 != 0) {
      int pad = 8 - count % 8;
      put(0, pad);
    }
  }

  void put_bytes(const std::uint8_t* data, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
      put(data[i], 8);
    }
  }

  std::size_t bit_size() const { return out.size() * 8 + static_cast<std::size_t>(count); }

  std::vector<std::uint8_t> finish() {
    if (count > 0) {
      out.push_back(static_cast<std::uint8_t>(buf & 0xFF));
      buf = 0;
      count = 0;
    }
    return std::move(out);
  }
};

// Canonical Huffman codes from code lengths (same construction as the decoder).
struct Codes {
  std::array<std::uint16_t, 288> code{};
  std::array<std::uint8_t, 288> len{};
};

Codes build_codes(const std::uint8_t* lengths, int n) {
  Codes c;
  std::uint16_t count[kMaxCodeBits + 1] = {};
  int max_len = 0;
  for (int i = 0; i < n; ++i) {
    c.len[i] = lengths[i];
    if (lengths[i] != 0) {
      ++count[lengths[i]];
      max_len = std::max(max_len, static_cast<int>(lengths[i]));
    }
  }
  std::uint32_t next_code[kMaxCodeBits + 1] = {};
  std::uint32_t code = 0;
  for (int len = 1; len <= max_len; ++len) {
    code = (code + count[len - 1]) << 1;
    next_code[len] = code;
  }
  for (int i = 0; i < n; ++i) {
    if (lengths[i] != 0) {
      c.code[i] = static_cast<std::uint16_t>(next_code[lengths[i]]++);
    }
  }
  return c;
}

// Length-limited Huffman code lengths from symbol frequencies, with zlib-style
// overflow redistribution. Deterministic: ties break on the symbol index.
// Symbols with zero frequency get length 0 unless force_complete requests a
// non-empty code (RFC 1951 streams must not carry an all-zero literal tree,
// and we always emit at least one distance code).
void huffman_lengths(const std::uint32_t* freq, int n, int max_bits, std::uint8_t* out_len) {
  std::fill(out_len, out_len + n, 0);
  std::vector<int> active;
  for (int i = 0; i < n; ++i) {
    if (freq[i] != 0) {
      active.push_back(i);
    }
  }
  if (active.empty()) {
    return;
  }
  if (active.size() == 1) {
    out_len[active[0]] = 1;
    return;
  }
  // In-place Huffman via a binary heap of (freq, tie) nodes; deterministic.
  struct Node {
    std::uint64_t freq;
    int tie;  // smaller wins; leaves use symbol order, internal nodes follow creation
    int left;
    int right;
  };
  std::vector<Node> nodes;
  nodes.reserve(static_cast<std::size_t>(n) * 2);
  std::vector<int> heap;
  heap.reserve(active.size());
  auto less = [&](int a, int b) {
    if (nodes[a].freq != nodes[b].freq) {
      return nodes[a].freq > nodes[b].freq;  // min-heap via std::push_heap max order
    }
    return nodes[a].tie > nodes[b].tie;
  };
  for (int sym : active) {
    nodes.push_back({freq[sym], sym, -1, -1});
    heap.push_back(static_cast<int>(nodes.size()) - 1);
    std::push_heap(heap.begin(), heap.end(), less);
  }
  while (heap.size() > 1) {
    std::pop_heap(heap.begin(), heap.end(), less);
    int a = heap.back();
    heap.pop_back();
    std::pop_heap(heap.begin(), heap.end(), less);
    int b = heap.back();
    heap.pop_back();
    nodes.push_back({nodes[a].freq + nodes[b].freq,
                     std::min(nodes[a].tie, nodes[b].tie) - n, a, b});
    heap.push_back(static_cast<int>(nodes.size()) - 1);
    std::push_heap(heap.begin(), heap.end(), less);
  }
  // Depths by walking down from the root (tree has <= 2n-1 nodes).
  std::vector<int> depth(nodes.size(), 0);
  std::vector<int> stack;
  stack.push_back(heap[0]);
  while (!stack.empty()) {
    int node = stack.back();
    stack.pop_back();
    if (nodes[node].left >= 0) {
      depth[nodes[node].left] = depth[node] + 1;
      depth[nodes[node].right] = depth[node] + 1;
      stack.push_back(nodes[node].left);
      stack.push_back(nodes[node].right);
    }
  }
  std::uint32_t bl_count[kMaxCodeBits + 1] = {};
  std::int32_t overflow = 0;
  for (std::size_t i = 0; i < active.size(); ++i) {
    int d = depth[i];  // leaves are the first active.size() nodes
    if (d > max_bits) {
      d = max_bits;
      ++overflow;
    }
    out_len[active[i]] = static_cast<std::uint8_t>(d);
    ++bl_count[d];
  }
  // Redistribute overflowed leaves: move one leaf up from the deepest
  // non-overflowed level, push two down, until the code is within the limit.
  while (overflow > 0) {
    int bits = max_bits - 1;
    while (bl_count[bits] == 0) {
      --bits;
    }
    --bl_count[bits];
    bl_count[bits + 1] += 2;
    --bl_count[max_bits];
    overflow -= 2;
  }
  // Reassign lengths: least-frequent symbols get the longest codes.
  std::vector<int> sorted = active;
  std::sort(sorted.begin(), sorted.end(), [&](int a, int b) {
    if (freq[a] != freq[b]) {
      return freq[a] < freq[b];
    }
    return a < b;
  });
  int idx = 0;
  for (int bits = max_bits; bits >= 1; --bits) {
    for (std::uint32_t k = 0; k < bl_count[bits]; ++k) {
      out_len[sorted[idx++]] = static_cast<std::uint8_t>(bits);
    }
  }
}

// One LZ77 token: a literal (sym < 256), end-of-block (256), or a match
// (length symbol 257..285 with its distance symbol and extra-bit values).
struct Token {
  std::uint16_t sym = 0;
  std::uint16_t dist_sym = 0;
  std::uint16_t len_extra = 0;
  std::uint32_t dist_extra = 0;
};

struct Match {
  std::size_t len = 0;
  std::size_t dist = 0;
};

// Hash-chain matcher over the whole input. Chains are walked most-recent
// first and bounded per search, so cost per position is capped.
class Matcher {
 public:
  static constexpr std::size_t kHashBits = 15;

  Matcher(const std::uint8_t* data, std::size_t size)
      : data_(data), size_(size), head_(std::size_t{1} << kHashBits, -1), prev_(size, -1) {}

  void insert(std::size_t pos) {
    if (pos + kMinMatch > size_) {
      return;
    }
    std::uint32_t h = hash(pos);
    prev_[pos] = head_[h];
    head_[h] = static_cast<std::int32_t>(pos);
  }

  Match find(std::size_t pos, int max_chain) const {
    Match best{0, 0};
    if (pos + kMinMatch > size_) {
      return best;
    }
    std::size_t max_len = std::min(kMaxMatch, size_ - pos);
    std::int64_t lowest = static_cast<std::int64_t>(pos) - static_cast<std::int64_t>(kWindowSize);
    std::int32_t cand = head_[hash(pos)];
    while (cand >= 0 && cand > lowest && max_chain-- > 0) {
      std::size_t c = static_cast<std::size_t>(cand);
      if (c >= pos) {  // the position we just inserted; never a real match
        cand = prev_[c];
        continue;
      }
      std::size_t len = 0;
      while (len < max_len && data_[c + len] == data_[pos + len]) {
        ++len;
      }
      if (len > best.len) {
        best = {len, pos - c};
        if (len >= max_len) {
          break;
        }
      }
      cand = prev_[c];
    }
    if (best.len < kMinMatch) {
      best = {0, 0};
    }
    return best;
  }

 private:
  std::uint32_t hash(std::size_t pos) const {
    std::uint32_t v = static_cast<std::uint32_t>(data_[pos]) |
                      (static_cast<std::uint32_t>(data_[pos + 1]) << 8) |
                      (static_cast<std::uint32_t>(data_[pos + 2]) << 16);
    return (v * 0x9E3779B1u) >> (32 - kHashBits);
  }

  const std::uint8_t* data_;
  std::size_t size_;
  std::vector<std::int32_t> head_;
  std::vector<std::int32_t> prev_;
};

void emit_token(BitWriter* w, const Token& t, const Codes& lit, const Codes& dist) {
  w->put_code(lit.code[t.sym], lit.len[t.sym]);
  if (t.sym < 257) {
    return;  // literal or end-of-block, no extra bits
  }
  int li = t.sym - 257;
  if (kLenExtra[li] != 0) {
    w->put(t.len_extra, kLenExtra[li]);
  }
  w->put_code(dist.code[t.dist_sym], dist.len[t.dist_sym]);
  if (kDistExtra[t.dist_sym] != 0) {
    w->put(t.dist_extra, kDistExtra[t.dist_sym]);
  }
}

Token make_match_token(std::size_t len, std::size_t dist) {
  Token t;
  int li = length_to_symbol(len);
  int di = dist_to_symbol(dist);
  t.sym = static_cast<std::uint16_t>(257 + li);
  t.dist_sym = static_cast<std::uint16_t>(di);
  t.len_extra = static_cast<std::uint16_t>(len - kLenBase[li]);
  t.dist_extra = static_cast<std::uint32_t>(dist - kDistBase[di]);
  return t;
}

void emit_stored(BitWriter* w, const std::uint8_t* data, std::size_t size, bool final) {
  std::size_t off = 0;
  do {
    std::size_t chunk = std::min(kMaxStoredBlock, size - off);
    bool last_chunk = off + chunk >= size;
    w->put(final && last_chunk ? 1 : 0, 1);
    w->put(0, 2);  // BTYPE stored
    w->align_to_byte();
    std::uint16_t len = static_cast<std::uint16_t>(chunk);
    w->put(len, 16);
    w->put(static_cast<std::uint16_t>(~len), 16);
    w->put_bytes(data + off, chunk);
    off += chunk;
  } while (off < size);
}

// Emits one dynamic-Huffman block for `tokens`, or stored block(s) when they
// are smaller for the `raw_bytes` input bytes the tokens cover.
void emit_dynamic_block(BitWriter* w, const std::vector<Token>& tokens,
                        const std::uint8_t* raw, std::size_t raw_bytes, bool final) {
  std::uint32_t lit_freq[286] = {};
  std::uint32_t dist_freq[kNumDistSymbols] = {};
  for (const Token& t : tokens) {
    ++lit_freq[t.sym];
    if (t.sym >= 257) {
      ++dist_freq[t.dist_sym];
    }
  }
  ++lit_freq[256];  // end-of-block
  // A Huffman code needs at least two symbols to be complete.
  int lit_used = 0;
  int last_lit = 0;
  for (int i = 0; i < 286; ++i) {
    if (lit_freq[i] != 0) {
      ++lit_used;
      last_lit = i;
    }
  }
  if (lit_used == 1) {
    ++lit_freq[last_lit == 0 ? 1 : 0];
  }
  bool any_dist = false;
  for (int i = 0; i < kNumDistSymbols; ++i) {
    any_dist = any_dist || dist_freq[i] != 0;
  }
  if (!any_dist) {
    ++dist_freq[0];  // placeholder 1-bit code, never emitted
  }
  std::uint8_t lit_len[286] = {};
  std::uint8_t dist_len[kNumDistSymbols] = {};
  huffman_lengths(lit_freq, 286, kMaxCodeBits, lit_len);
  huffman_lengths(dist_freq, kNumDistSymbols, kMaxCodeBits, dist_len);

  int hlit = 286;
  while (hlit > 257 && lit_len[hlit - 1] == 0) {
    --hlit;
  }
  int hdist = kNumDistSymbols;
  while (hdist > 1 && dist_len[hdist - 1] == 0) {
    --hdist;
  }

  // Run-length encode the combined code-length sequence (RFC 1951 3.2.7).
  std::vector<std::uint8_t> seq;
  seq.reserve(static_cast<std::size_t>(hlit + hdist));
  for (int i = 0; i < hlit; ++i) {
    seq.push_back(lit_len[i]);
  }
  for (int i = 0; i < hdist; ++i) {
    seq.push_back(dist_len[i]);
  }
  struct Rle {
    std::uint8_t sym;
    std::uint8_t extra_val;
    int extra_bits;
  };
  std::vector<Rle> rle;
  std::uint32_t cl_freq[19] = {};
  std::size_t i = 0;
  while (i < seq.size()) {
    std::uint8_t v = seq[i];
    std::size_t run = 1;
    while (i + run < seq.size() && seq[i + run] == v) {
      ++run;
    }
    i += run;
    if (v == 0) {
      while (run >= 11) {
        std::size_t take = std::min(run, std::size_t{138});
        rle.push_back({18, static_cast<std::uint8_t>(take - 11), 7});
        ++cl_freq[18];
        run -= take;
      }
      if (run >= 3) {
        rle.push_back({17, static_cast<std::uint8_t>(run - 3), 3});
        ++cl_freq[17];
        run = 0;
      }
      while (run-- > 0) {
        rle.push_back({0, 0, 0});
        ++cl_freq[0];
      }
    } else {
      rle.push_back({v, 0, 0});
      ++cl_freq[v];
      --run;
      while (run >= 3) {
        std::size_t take = std::min(run, std::size_t{6});
        rle.push_back({16, static_cast<std::uint8_t>(take - 3), 2});
        ++cl_freq[16];
        run -= take;
      }
      while (run-- > 0) {
        rle.push_back({v, 0, 0});
        ++cl_freq[v];
      }
    }
  }
  std::uint8_t cl_len[19] = {};
  huffman_lengths(cl_freq, 19, kMaxLenCodeBits, cl_len);
  int hclen = 19;
  while (hclen > 4 && cl_len[kCodeLengthOrder[hclen - 1]] == 0) {
    --hclen;
  }

  // Size estimate for the dynamic block, in bits.
  std::uint64_t dynamic_bits = 3 + 5 + 5 + 4 + 3 * static_cast<std::uint64_t>(hclen);
  Codes cl_codes = build_codes(cl_len, 19);
  for (const Rle& r : rle) {
    dynamic_bits += cl_codes.len[r.sym] + r.extra_bits;
  }
  for (const Token& t : tokens) {
    dynamic_bits += lit_len[t.sym];
    if (t.sym >= 257) {
      dynamic_bits += kLenExtra[t.sym - 257] + dist_len[t.dist_sym] + kDistExtra[t.dist_sym];
    }
  }
  dynamic_bits += lit_len[256];
  // Stored cost: 3-bit header per 65535 chunk, byte alignment, 32-bit LEN/NLEN.
  std::uint64_t chunks = raw_bytes / kMaxStoredBlock + 1;
  std::uint64_t stored_bits = chunks * (3 + 8 + 32) + raw_bytes * 8;

  if (stored_bits < dynamic_bits) {
    emit_stored(w, raw, raw_bytes, final);
    return;
  }
  w->put(final ? 1 : 0, 1);
  w->put(2, 2);  // BTYPE dynamic
  w->put(static_cast<std::uint32_t>(hlit - 257), 5);
  w->put(static_cast<std::uint32_t>(hdist - 1), 5);
  w->put(static_cast<std::uint32_t>(hclen - 4), 4);
  for (int k = 0; k < hclen; ++k) {
    w->put(cl_len[kCodeLengthOrder[k]], 3);
  }
  for (const Rle& r : rle) {
    w->put_code(cl_codes.code[r.sym], cl_codes.len[r.sym]);
    if (r.extra_bits != 0) {
      w->put(r.extra_val, r.extra_bits);
    }
  }
  Codes lit_codes = build_codes(lit_len, hlit);
  Codes dist_codes = build_codes(dist_len, hdist);
  for (const Token& t : tokens) {
    emit_token(w, t, lit_codes, dist_codes);
  }
  w->put_code(lit_codes.code[256], lit_codes.len[256]);
}

void emit_fixed_block(BitWriter* w, const std::vector<Token>& tokens, bool final) {
  Codes lit_codes = build_codes(kFixedLitLen, 288);
  Codes dist_codes = build_codes(kFixedDistLen, 32);
  w->put(final ? 1 : 0, 1);
  w->put(1, 2);  // BTYPE fixed
  for (const Token& t : tokens) {
    emit_token(w, t, lit_codes, dist_codes);
  }
  w->put_code(lit_codes.code[256], lit_codes.len[256]);
}

// Tokenizes the input with the matcher. `lazy` enables one-step lookahead.
std::vector<Token> tokenize(const std::uint8_t* data, std::size_t size, bool lazy,
                            int max_chain) {
  std::vector<Token> tokens;
  Matcher matcher(data, size);
  std::size_t pos = 0;
  while (pos < size) {
    matcher.insert(pos);
    Match cur = matcher.find(pos, max_chain);
    if (lazy && cur.len >= kMinMatch && cur.len < kMaxMatch && pos + 1 < size) {
      matcher.insert(pos + 1);
      Match next = matcher.find(pos + 1, max_chain);
      if (next.len > cur.len) {
        Token lit{};
        lit.sym = data[pos];
        tokens.push_back(lit);
        ++pos;
        cur = next;
        tokens.push_back(make_match_token(cur.len, cur.dist));
        for (std::size_t k = pos + 1; k < pos + cur.len; ++k) {
          matcher.insert(k);
        }
        pos += cur.len;
        continue;
      }
    }
    if (cur.len >= kMinMatch) {
      tokens.push_back(make_match_token(cur.len, cur.dist));
      for (std::size_t k = pos + 1; k < pos + cur.len; ++k) {
        matcher.insert(k);
      }
      pos += cur.len;
    } else {
      Token lit{};
      lit.sym = data[pos];
      tokens.push_back(lit);
      ++pos;
    }
  }
  return tokens;
}

}  // namespace

std::vector<std::uint8_t> deflate_raw(const std::uint8_t* data, std::size_t size, int level) {
  static const std::uint8_t kEmpty = 0;
  if (data == nullptr) {
    data = &kEmpty;  // only dereferenced when size > 0
  }
  // Clamp to the nearest supported level: 0 (stored), 1 (fixed), 6 (dynamic).
  int effective = level <= 0 ? 0 : (level <= 3 ? 1 : 6);
  BitWriter w;
  if (effective == 0) {
    emit_stored(&w, data, size, true);
    return w.finish();
  }
  const bool lazy = effective >= 6;
  const int max_chain = lazy ? 128 : 32;
  // Tokenize in bounded blocks so token memory stays small; block boundaries
  // depend only on the token count, keeping output deterministic.
  constexpr std::size_t kBlockTokens = 1 << 19;
  std::vector<Token> all = tokenize(data, size, lazy, max_chain);
  std::size_t block_begin = 0;  // first token index of the current block
  std::size_t raw_offset = 0;   // input offset of token block_begin
  std::size_t raw_pos = 0;      // input offset of token i
  for (std::size_t i = 0; i <= all.size(); ++i) {
    bool flush = i == all.size() || i - block_begin >= kBlockTokens;
    if (flush) {
      std::vector<Token> block(all.begin() + static_cast<std::ptrdiff_t>(block_begin),
                               all.begin() + static_cast<std::ptrdiff_t>(i));
      bool final = i == all.size();
      if (lazy) {
        emit_dynamic_block(&w, block, data + raw_offset, raw_pos - raw_offset, final);
      } else {
        emit_fixed_block(&w, block, final);
      }
      raw_offset = raw_pos;
      block_begin = i;
    }
    if (i < all.size()) {
      const Token& t = all[i];
      raw_pos += t.sym < 257 ? 1
                             : static_cast<std::size_t>(kLenBase[t.sym - 257]) + t.len_extra;
    }
  }
  return w.finish();
}

}  // namespace lumora::package
