// Hardened raw-DEFLATE decoder (RFC 1951, ZIP method 8). Runs on untrusted
// input: every read is bounds-checked, output growth is checked against
// max_output before it happens, and every malformed stream ends in a status.
#include "deflate.h"

#include <algorithm>

#include "deflate_common.h"

namespace lumora::package {
namespace {

using namespace deflate_detail;

// LSB-first bit reader over the input. Only real input bits are ever loaded;
// peeking near the end zero-pads above the loaded bits, and consumers check
// that the bits they consume were real.
struct BitReader {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0;
  std::size_t pos = 0;      // next byte to load into buf
  std::uint64_t buf = 0;    // low `count` bits are loaded, unconsumed input
  int count = 0;            // real bits currently in buf

  void fill() {
    while (count <= 56 && pos < size) {
      buf |= static_cast<std::uint64_t>(data[pos]) << count;
      ++pos;
      count += 8;
    }
  }

  // Reads n (<= 32) bits. Returns false (Truncated) if they are not all real.
  bool get(int n, std::uint32_t* out) {
    fill();
    if (count < n) {
      return false;
    }
    *out = static_cast<std::uint32_t>(buf & ((std::uint64_t{1} << n) - 1));
    buf >>= n;
    count -= n;
    return true;
  }

  void align_to_byte() {
    int drop = count % 8;
    buf >>= drop;
    count -= drop;
  }

  // Whole input bytes consumed so far, rounding up over a partial byte.
  std::size_t consumed_bytes() const {
    std::size_t used_bits = pos * 8 - static_cast<std::size_t>(count);
    return (used_bits + 7) / 8;
  }
};

// Single-level canonical Huffman decode table. Entries are indexed by the low
// `max_len` stream bits (LSB-first); each entry is the decoded symbol and its
// code length. A complete code fills the table; codes shorter than max_len
// fill every extension of their prefix, so zero padding past the input end
// can only match a code whose length exceeds the real bits left, which the
// caller rejects as Truncated.
struct DecodeTable {
  struct Entry {
    std::uint16_t symbol = 0;
    std::uint8_t len = 0;  // 0 marks an unfilled slot
  };
  std::vector<Entry> entries;
  std::uint32_t mask = 0;
  int max_len = 0;
  bool empty = false;  // no symbols at all (only legal for an unused distance tree)
};

// Builds a decode table from code lengths. Rejects over-subscribed codes and
// incomplete codes, with the RFC 1951 / zlib exception that a code with
// exactly one symbol is treated as a single 1-bit code.
bool build_table(const std::uint8_t* lengths, int n, DecodeTable* out) {
  int used = 0;
  int last_symbol = 0;
  int max_len = 0;
  std::uint16_t count[kMaxCodeBits + 1] = {};
  for (int i = 0; i < n; ++i) {
    if (lengths[i] > kMaxCodeBits) {
      return false;
    }
    if (lengths[i] != 0) {
      ++count[lengths[i]];
      ++used;
      last_symbol = i;
      max_len = std::max(max_len, static_cast<int>(lengths[i]));
    }
  }
  out->entries.clear();
  out->max_len = 0;
  out->mask = 0;
  out->empty = used == 0;
  if (used == 0) {
    return true;
  }
  if (used == 1) {
    // A lone symbol forms a degenerate 1-bit code: either bit decodes to it.
    out->max_len = 1;
    out->mask = 1;
    out->entries.assign(2, {static_cast<std::uint16_t>(last_symbol), 1});
    return true;
  }
  // Over-subscribed / incomplete check: one code space unit per bit length.
  std::int32_t left = 1;
  for (int len = 1; len <= max_len; ++len) {
    left <<= 1;
    left -= count[len];
    if (left < 0) {
      return false;  // over-subscribed
    }
  }
  if (left > 0) {
    return false;  // incomplete
  }
  // Canonical codes in (length, symbol) order, RFC 1951 3.2.2.
  std::uint32_t next_code[kMaxCodeBits + 1] = {};
  std::uint32_t code = 0;
  for (int len = 1; len <= max_len; ++len) {
    code = (code + count[len - 1]) << 1;
    next_code[len] = code;
  }
  out->max_len = max_len;
  out->mask = (std::uint32_t{1} << max_len) - 1;
  out->entries.assign(std::size_t{1} << max_len, {});
  for (int sym = 0; sym < n; ++sym) {
    int len = lengths[sym];
    if (len == 0) {
      continue;
    }
    std::uint32_t rev = reverse_bits(next_code[len], len);
    ++next_code[len];
    std::uint32_t step = std::uint32_t{1} << len;
    for (std::uint32_t idx = rev; idx <= out->mask; idx += step) {
      out->entries[idx] = {static_cast<std::uint16_t>(sym), static_cast<std::uint8_t>(len)};
    }
  }
  return true;
}

// Decodes one symbol. Returns false on Truncated (not enough real bits) or
// BadHuffman (no code defined, e.g. an empty distance tree being used).
bool decode_symbol(BitReader* br, const DecodeTable& table, std::uint32_t* symbol,
                   InflateStatus* err) {
  if (table.empty) {
    *err = InflateStatus::BadHuffman;
    return false;
  }
  br->fill();
  const DecodeTable::Entry& e = table.entries[br->buf & table.mask];
  if (e.len == 0 || e.len > br->count) {
    *err = e.len == 0 ? InflateStatus::BadHuffman : InflateStatus::Truncated;
    return false;
  }
  br->buf >>= e.len;
  br->count -= e.len;
  *symbol = e.symbol;
  return true;
}

struct OutputBuffer {
  std::vector<std::uint8_t> data;
  std::uint64_t max_output = 0;

  // Checks the limit before any allocation: capacity never exceeds max_output.
  bool reserve_for(std::size_t extra) {
    std::uint64_t needed = static_cast<std::uint64_t>(data.size()) + extra;
    if (needed > max_output) {
      return false;
    }
    if (needed > data.capacity()) {
      std::uint64_t grown = std::max<std::uint64_t>(needed, data.capacity() * 2);
      data.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(grown, max_output)));
    }
    return true;
  }

  bool push(std::uint8_t b) {
    if (!reserve_for(1)) {
      return false;
    }
    data.push_back(b);
    return true;
  }
};

InflateStatus inflate_stored(BitReader* br, OutputBuffer* out) {
  br->align_to_byte();
  std::uint32_t len = 0;
  std::uint32_t nlen = 0;
  if (!br->get(16, &len) || !br->get(16, &nlen)) {
    return InflateStatus::Truncated;
  }
  if (len != (nlen ^ 0xFFFFu)) {
    return InflateStatus::BadStoredLen;
  }
  br->align_to_byte();
  if (static_cast<std::uint64_t>(br->pos) - (br->count / 8) + len > br->size) {
    return InflateStatus::Truncated;
  }
  if (!out->reserve_for(len)) {
    return InflateStatus::OutputLimit;
  }
  std::size_t start = br->pos - br->count / 8;
  out->data.insert(out->data.end(), br->data + start, br->data + start + len);
  std::size_t advance = len;
  // Drop buffered whole bytes first, then move pos for the rest.
  std::size_t buffered = static_cast<std::size_t>(br->count / 8);
  std::size_t from_buf = std::min(buffered, advance);
  br->count -= static_cast<int>(from_buf * 8);
  br->buf >>= from_buf * 8;
  br->pos += advance - from_buf;
  return InflateStatus::Ok;
}

InflateStatus inflate_huffman_block(BitReader* br, OutputBuffer* out,
                                    const DecodeTable& lit_table,
                                    const DecodeTable& dist_table) {
  for (;;) {
    std::uint32_t sym = 0;
    InflateStatus err = InflateStatus::Ok;
    if (!decode_symbol(br, lit_table, &sym, &err)) {
      return err;
    }
    if (sym < 256) {
      if (!out->push(static_cast<std::uint8_t>(sym))) {
        return InflateStatus::OutputLimit;
      }
      continue;
    }
    if (sym == 256) {
      return InflateStatus::Ok;
    }
    if (sym > 285) {
      return InflateStatus::BadSymbol;  // 286, 287
    }
    int li = static_cast<int>(sym) - 257;
    std::uint32_t extra = 0;
    if (kLenExtra[li] != 0 && !br->get(kLenExtra[li], &extra)) {
      return InflateStatus::Truncated;
    }
    std::size_t length = kLenBase[li] + extra;
    std::uint32_t dsym = 0;
    if (!decode_symbol(br, dist_table, &dsym, &err)) {
      return err;
    }
    if (dsym >= kNumDistSymbols) {
      return InflateStatus::BadSymbol;  // 30, 31
    }
    if (kDistExtra[dsym] != 0 && !br->get(kDistExtra[dsym], &extra)) {
      return InflateStatus::Truncated;
    }
    std::size_t dist = kDistBase[dsym] + extra;
    if (dist > out->data.size()) {
      return InflateStatus::BadDistance;
    }
    if (!out->reserve_for(length)) {
      return InflateStatus::OutputLimit;
    }
    std::size_t from = out->data.size() - dist;
    for (std::size_t i = 0; i < length; ++i) {
      out->data.push_back(out->data[from + i]);  // overlap is intentional (LZ77)
    }
  }
}

InflateStatus inflate_dynamic_tables(BitReader* br, DecodeTable* lit_table,
                                     DecodeTable* dist_table) {
  std::uint32_t hlit = 0;
  std::uint32_t hdist = 0;
  std::uint32_t hclen = 0;
  if (!br->get(5, &hlit) || !br->get(5, &hdist) || !br->get(4, &hclen)) {
    return InflateStatus::Truncated;
  }
  hlit += 257;
  hdist += 1;
  hclen += 4;
  std::uint8_t cl_lengths[19] = {};
  for (std::uint32_t i = 0; i < hclen; ++i) {
    std::uint32_t v = 0;
    if (!br->get(3, &v)) {
      return InflateStatus::Truncated;
    }
    cl_lengths[kCodeLengthOrder[i]] = static_cast<std::uint8_t>(v);
  }
  DecodeTable cl_table;
  if (!build_table(cl_lengths, 19, &cl_table)) {
    return InflateStatus::BadHuffman;
  }
  std::uint8_t lengths[288 + 32] = {};
  std::uint32_t total = hlit + hdist;
  std::uint32_t i = 0;
  while (i < total) {
    std::uint32_t sym = 0;
    InflateStatus err = InflateStatus::Ok;
    if (!decode_symbol(br, cl_table, &sym, &err)) {
      return err == InflateStatus::BadHuffman ? InflateStatus::BadHuffman : err;
    }
    if (sym < 16) {
      lengths[i++] = static_cast<std::uint8_t>(sym);
      continue;
    }
    std::uint32_t repeat = 0;
    std::uint8_t value = 0;
    if (sym == 16) {
      if (i == 0) {
        return InflateStatus::BadHuffman;  // repeat with no previous length
      }
      value = lengths[i - 1];
      if (!br->get(2, &repeat)) {
        return InflateStatus::Truncated;
      }
      repeat += 3;
    } else if (sym == 17) {
      if (!br->get(3, &repeat)) {
        return InflateStatus::Truncated;
      }
      repeat += 3;
    } else {  // 18
      if (!br->get(7, &repeat)) {
        return InflateStatus::Truncated;
      }
      repeat += 11;
    }
    if (i + repeat > total) {
      return InflateStatus::BadHuffman;  // repeats past the declared count
    }
    while (repeat-- != 0) {
      lengths[i++] = value;
    }
  }
  if (lengths[256] == 0) {
    return InflateStatus::BadHuffman;  // no end-of-block code
  }
  if (!build_table(lengths, static_cast<int>(hlit), lit_table)) {
    return InflateStatus::BadHuffman;
  }
  if (!build_table(lengths + hlit, static_cast<int>(hdist), dist_table)) {
    return InflateStatus::BadHuffman;
  }
  return InflateStatus::Ok;
}

const DecodeTable& fixed_lit_table() {
  static const DecodeTable table = [] {
    DecodeTable t;
    build_table(kFixedLitLen, 288, &t);
    return t;
  }();
  return table;
}

const DecodeTable& fixed_dist_table() {
  static const DecodeTable table = [] {
    DecodeTable t;
    build_table(kFixedDistLen, 32, &t);
    return t;
  }();
  return table;
}

}  // namespace

InflateResult inflate_raw(const std::uint8_t* data, std::size_t size, std::uint64_t max_output) {
  InflateResult result;
  BitReader br{data, size, 0, 0, 0};
  OutputBuffer out;
  out.max_output = max_output;
  bool final = false;
  while (!final) {
    std::uint32_t bfinal = 0;
    std::uint32_t btype = 0;
    if (!br.get(1, &bfinal) || !br.get(2, &btype)) {
      result.status = InflateStatus::Truncated;
      return result;
    }
    final = bfinal != 0;
    InflateStatus status = InflateStatus::Ok;
    if (btype == 0) {
      status = inflate_stored(&br, &out);
    } else if (btype == 1) {
      status = inflate_huffman_block(&br, &out, fixed_lit_table(), fixed_dist_table());
    } else if (btype == 2) {
      DecodeTable lit_table;
      DecodeTable dist_table;
      status = inflate_dynamic_tables(&br, &lit_table, &dist_table);
      if (status == InflateStatus::Ok) {
        status = inflate_huffman_block(&br, &out, lit_table, dist_table);
      }
    } else {
      status = InflateStatus::BadBlockType;
    }
    if (status != InflateStatus::Ok) {
      result.status = status;
      return result;
    }
  }
  result.status = InflateStatus::Ok;
  result.data = std::move(out.data);
  result.consumed = br.consumed_bytes();
  return result;
}

}  // namespace lumora::package
