// Byte string search: first and last occurrence, count of non-overlapping occurrences, ASCII case-insensitive equality. See bytesearch.h
// for the contract. Integer only: no allocation, no recursion, no global state, no libc string functions. A plain scan is used.
#include "bytesearch.h"

namespace lumora::bytesearch {

namespace {

using u32 = std::uint32_t;
using u8 = std::uint8_t;

// True when needle[0 .. nn) equals hay[p .. p + nn). The caller guarantees p + nn <= nh.
bool matches_at(const u8* hay, u32 p, const u8* needle, u32 nn) {
  for (u32 i = 0; i < nn; ++i) {
    if (hay[p + i] != needle[i]) return false;
  }
  return true;
}

u8 fold(u8 c) { return (c >= 'A' && c <= 'Z') ? static_cast<u8>(c + ('a' - 'A')) : c; }

}  // namespace

SearchResult bytes_find(const u8* hay, u32 nh, const u8* needle, u32 nn, u32* out_pos, bool* out_found) {
  if (out_pos == nullptr || out_found == nullptr) return SearchResult::NullArgument;
  if ((hay == nullptr && nh > 0) || (needle == nullptr && nn > 0)) return SearchResult::NullArgument;
  if (nh > kMaxLen || nn > kMaxLen) return SearchResult::TooLarge;
  if (nn == 0) {
    *out_pos = 0;
    *out_found = true;
    return SearchResult::Ok;
  }
  if (nn <= nh) {
    const u32 last = nh - nn;
    for (u32 p = 0; p <= last; ++p) {
      if (matches_at(hay, p, needle, nn)) {
        *out_pos = p;
        *out_found = true;
        return SearchResult::Ok;
      }
    }
  }
  *out_found = false;
  return SearchResult::Ok;
}

SearchResult bytes_find_last(const u8* hay, u32 nh, const u8* needle, u32 nn, u32* out_pos, bool* out_found) {
  if (out_pos == nullptr || out_found == nullptr) return SearchResult::NullArgument;
  if ((hay == nullptr && nh > 0) || (needle == nullptr && nn > 0)) return SearchResult::NullArgument;
  if (nh > kMaxLen || nn > kMaxLen) return SearchResult::TooLarge;
  if (nn == 0) {
    *out_pos = nh;
    *out_found = true;
    return SearchResult::Ok;
  }
  if (nn <= nh) {
    u32 p = nh - nn;
    for (;;) {
      if (matches_at(hay, p, needle, nn)) {
        *out_pos = p;
        *out_found = true;
        return SearchResult::Ok;
      }
      if (p == 0) break;
      --p;
    }
  }
  *out_found = false;
  return SearchResult::Ok;
}

SearchResult bytes_count(const u8* hay, u32 nh, const u8* needle, u32 nn, u32* out_count) {
  if (out_count == nullptr) return SearchResult::NullArgument;
  if ((hay == nullptr && nh > 0) || (needle == nullptr && nn > 0)) return SearchResult::NullArgument;
  if (nh > kMaxLen || nn > kMaxLen) return SearchResult::TooLarge;
  if (nn == 0) return SearchResult::EmptyNeedle;
  u32 count = 0;
  if (nn <= nh) {
    const u32 last = nh - nn;
    u32 p = 0;
    while (p <= last) {
      if (matches_at(hay, p, needle, nn)) {
        ++count;
        p += nn;
      } else {
        ++p;
      }
    }
  }
  *out_count = count;
  return SearchResult::Ok;
}

SearchResult bytes_equal_ascii_nocase(const u8* a, u32 na, const u8* b, u32 nb, bool* out_equal) {
  if (out_equal == nullptr) return SearchResult::NullArgument;
  if ((a == nullptr && na > 0) || (b == nullptr && nb > 0)) return SearchResult::NullArgument;
  if (na > kMaxLen || nb > kMaxLen) return SearchResult::TooLarge;
  bool equal = (na == nb);
  if (equal) {
    for (u32 i = 0; i < na; ++i) {
      if (fold(a[i]) != fold(b[i])) {
        equal = false;
        break;
      }
    }
  }
  *out_equal = equal;
  return SearchResult::Ok;
}

}  // namespace lumora::bytesearch
