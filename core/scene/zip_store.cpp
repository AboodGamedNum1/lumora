#include "zip_store.h"

#include <cstring>

namespace lumora::scene {
namespace {
std::uint32_t crc32(const std::uint8_t* p, std::size_t n) {
  static std::uint32_t table[256];
  static bool init = false;
  if (!init) {
    for (std::uint32_t i = 0; i < 256; ++i) {
      std::uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      table[i] = c;
    }
    init = true;
  }
  std::uint32_t c = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < n; ++i) c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
  return c ^ 0xFFFFFFFFu;
}
void u16(std::vector<std::uint8_t>& o, std::uint32_t v) { o.push_back(v & 0xFF); o.push_back((v >> 8) & 0xFF); }
void u32(std::vector<std::uint8_t>& o, std::uint32_t v) { u16(o, v & 0xFFFF); u16(o, v >> 16); }

struct Reader {
  const std::uint8_t* d;
  std::size_t n;
  bool has(std::uint64_t off, std::uint64_t len) const { return off <= n && len <= n - off; }
  std::uint32_t r16(std::size_t o) const { return d[o] | (d[o + 1] << 8); }
  std::uint32_t r32(std::size_t o) const { return r16(o) | (r16(o + 2) << 16); }
};
}  // namespace

std::vector<std::uint8_t> write_zip_store(const FileMap& files) {
  std::vector<std::uint8_t> out;
  struct Rec { std::string name; std::uint32_t crc, size, off; };
  std::vector<Rec> recs;
  for (const auto& [name, data] : files) {
    Rec r{name, crc32(data.data(), data.size()), static_cast<std::uint32_t>(data.size()), static_cast<std::uint32_t>(out.size())};
    u32(out, 0x04034b50); u16(out, 20); u16(out, 0x0800); u16(out, 0); u16(out, 0); u16(out, 0x21);
    u32(out, r.crc); u32(out, r.size); u32(out, r.size);
    u16(out, static_cast<std::uint32_t>(name.size())); u16(out, 0);
    out.insert(out.end(), name.begin(), name.end());
    out.insert(out.end(), data.begin(), data.end());
    recs.push_back(r);
  }
  const auto cd_off = static_cast<std::uint32_t>(out.size());
  for (const auto& r : recs) {
    u32(out, 0x02014b50); u16(out, 20); u16(out, 20); u16(out, 0x0800); u16(out, 0); u16(out, 0); u16(out, 0x21);
    u32(out, r.crc); u32(out, r.size); u32(out, r.size);
    u16(out, static_cast<std::uint32_t>(r.name.size())); u16(out, 0); u16(out, 0); u16(out, 0); u16(out, 0);
    u32(out, 0); u32(out, r.off);
    out.insert(out.end(), r.name.begin(), r.name.end());
  }
  const auto cd_size = static_cast<std::uint32_t>(out.size()) - cd_off;
  u32(out, 0x06054b50); u16(out, 0); u16(out, 0);
  u16(out, static_cast<std::uint32_t>(recs.size())); u16(out, static_cast<std::uint32_t>(recs.size()));
  u32(out, cd_size); u32(out, cd_off); u16(out, 0);
  return out;
}

ZipReadResult read_zip_store(const std::uint8_t* data, std::size_t size, std::uint64_t max_entries,
                             std::uint64_t max_total_bytes) {
  ZipReadResult res;
  auto fail = [&](const char* m) { res.ok = false; res.error = m; res.files.clear(); return res; };
  Reader R{data, size};
  if (size < 22) return fail("too small for a ZIP archive");
  // End of central directory: scan back over at most a 65535-byte comment.
  std::size_t eocd = size;
  const std::size_t lo = size > 22 + 65535 ? size - 22 - 65535 : 0;
  for (std::size_t i = size - 22 + 1; i-- > lo;)
    if (R.r32(i) == 0x06054b50) { eocd = i; break; }
  if (eocd == size) return fail("end of central directory not found");
  if (R.r16(eocd + 4) != 0 || R.r16(eocd + 6) != 0) return fail("multi-disk archives are not supported");
  const std::uint32_t total = R.r16(eocd + 10), cd_size = R.r32(eocd + 12), cd_off = R.r32(eocd + 16);
  if (total == 0xFFFF || cd_size == 0xFFFFFFFFu || cd_off == 0xFFFFFFFFu) return fail("ZIP64 is not supported");
  if (R.r16(eocd + 8) != total) return fail("inconsistent entry counts");
  if (total > max_entries) return fail("too many entries");
  if (!R.has(cd_off, cd_size) || static_cast<std::uint64_t>(cd_off) + cd_size > eocd) return fail("central directory out of range");
  std::size_t p = cd_off;
  std::uint64_t sum = 0;
  for (std::uint32_t i = 0; i < total; ++i) {
    if (!R.has(p, 46) || R.r32(p) != 0x02014b50) return fail("bad central directory entry");
    const std::uint32_t flags = R.r16(p + 8), method = R.r16(p + 10), crc = R.r32(p + 16), csize = R.r32(p + 20),
                        usize = R.r32(p + 24), nlen = R.r16(p + 28), xlen = R.r16(p + 30), clen = R.r16(p + 32),
                        lho = R.r32(p + 42);
    if (!R.has(p + 46, std::uint64_t{nlen} + xlen + clen)) return fail("entry header out of range");
    if (flags & 0x1) return fail("encrypted entries are not supported");
    if (flags & 0x8) return fail("data descriptors are not supported");
    if (method != 0) return fail("only stored entries are supported");
    if (csize != usize) return fail("stored entry with different sizes");
    if (csize == 0xFFFFFFFFu) return fail("ZIP64 is not supported");
    sum += usize;
    if (sum > max_total_bytes) return fail("archive too large");
    std::string name(reinterpret_cast<const char*>(data + p + 46), nlen);
    if (name.empty() || name.back() == '/') return fail("directory or empty entry names are not supported");
    if (!R.has(lho, 30) || R.r32(lho) != 0x04034b50) return fail("bad local header");
    const std::uint64_t dstart = std::uint64_t{lho} + 30 + R.r16(lho + 26) + R.r16(lho + 28);
    if (!R.has(dstart, usize)) return fail("entry data out of range");
    if (R.r16(lho + 26) != nlen || std::memcmp(data + lho + 30, name.data(), nlen) != 0) return fail("local and central names differ");
    if (crc32(data + dstart, usize) != crc) return fail("crc mismatch");
    if (res.files.count(name)) return fail("duplicate entry name");
    res.files[name] = std::vector<std::uint8_t>(data + dstart, data + dstart + usize);
    p += 46 + std::size_t{nlen} + xlen + clen;
  }
  res.ok = true;
  return res;
}

}  // namespace lumora::scene
