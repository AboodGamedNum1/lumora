// Structural audit of a .lumora ZIP container, before any entry is read or inflated.
// Pure function on a byte buffer: no file system, no inflate, no allocation beyond the result.
// It checks the container layout and the agreement between central directory and local headers,
// then returns the entry list. The caller applies core/package/entry_policy.h to that list.
// Source of the rules: docs/security-model.md, "Archive defenses". Everything not allowed below
// is rejected; the audit never repairs or guesses. Evidence label: unit tests only.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "entry_policy.h"

namespace lumora::package {

struct ZipAuditLimits {
  std::uint64_t max_archive_bytes = 4096ull << 20;
  std::uint64_t max_entries = 10000;
  std::uint64_t max_name_bytes = 255;     // bytes of the stored name; longer is BadCentralHeader
  std::uint64_t max_extra_bytes = 1024;   // per entry, central and local each
  std::uint64_t max_comment_bytes = 1024; // archive comment and each entry comment
};

enum class ZipVerdict {
  Ok,
  TooLarge,              // buffer longer than max_archive_bytes
  TooSmall,              // shorter than an end record
  NoEndRecord,           // no valid end of central directory record
  MultipleEndRecords,    // a second end-record signature anywhere the scan can see it (comment included)
  TrailingData,          // bytes after the end record that the comment length does not cover
  PrependedData,         // first local header is not at offset 0
  MultiDisk,             // disk numbers other than 0, or entry counts that differ per disk
  Zip64Mismatch,         // locator/record missing, inconsistent, or disagreeing with the 32-bit fields
  TooManyEntries,
  CentralDirectoryBounds,// central directory outside the buffer, overlapping the end record, or not
                         // ending exactly where the end record starts
  BadCentralHeader,      // bad signature, truncated header, field out of bounds, name or extra over limit
  BadLocalHeader,        // bad signature, truncated header, name or extra over limit
  HeaderMismatch,        // central and local header disagree (name, flags, method, crc, sizes, time)
  Encrypted,             // flag bit 0, 6 or 13
  UnsupportedFeature,    // data descriptor (flag bit 3), patched data, unknown flag bits
  UnsupportedMethod,     // method other than 0 (stored) or 8 (deflate)
  SpecialFile,           // symlink, device, fifo, socket (Unix mode in the external attributes)
  BadSizes,              // stored entry with different sizes, directory with data, offset or sum overflow
  Overlap,               // entry data overlaps another entry, a header, or the central directory
  Gap,                   // bytes between entries not covered by any header or data
  OutOfOrder             // local header offsets not strictly ascending in central directory order
};

struct ZipEntry {
  std::string name;                  // as stored, bytes
  std::uint16_t method = 0;
  std::uint32_t crc32 = 0;           // declared, not verified here
  std::uint64_t compressed_bytes = 0;
  std::uint64_t uncompressed_bytes = 0;
  std::uint64_t local_header_offset = 0;
  std::uint64_t data_offset = 0;     // first byte of the entry data
};

struct ZipAuditResult {
  ZipVerdict verdict = ZipVerdict::NoEndRecord;
  std::size_t entry_index = 0;  // offending central-directory index when an entry caused the verdict
  std::uint64_t offset = 0;     // byte offset where the problem was found (0 when Ok)
  std::vector<ZipEntry> entries;  // file entries only (directories are checked, then omitted); empty unless Ok
  std::uint64_t directory_count = 0;
};

// Check order (the first failure wins, binding):
//   1. TooLarge (size above max_archive_bytes), then TooSmall (below 22 bytes).
//   2. End record: search the last min(size, 22 + 65535) bytes. A record is valid when its comment ends exactly
//      at the buffer end. Two end-record signatures in that window is MultipleEndRecords (ZIP64 record and
//      locator signatures inside a comment do not count). With no valid record: TrailingData when a signature
//      is followed by bytes beyond its declared comment, else NoEndRecord. A comment above max_comment_bytes is
//      NoEndRecord at the record offset. Disk fields not zero, or per-disk count different from total: MultiDisk.
//   3. ZIP64: optional, but when any end-record field holds its sentinel (0xFFFF count, 0xFFFFFFFF size or
//      offset) the locator must sit immediately before the end record and the ZIP64 record immediately before the
//      locator (disk 0, one disk, record size field exactly 44). A locator without any sentinel, a value that
//      would fit the ordinary field (canonical form is value >= sentinel), or any disagreement between the two
//      records is Zip64Mismatch. Versions are not checked.
//   4. TooManyEntries (count from the end record, directories included), then central directory bounds: it must
//      end exactly where the ZIP64 record (or end record) starts, and hold exactly the declared count.
//   5. Per central entry, in order: header fields and limits (BadCentralHeader, empty name included), then
//      Encrypted (bits 0, 6, 13), UnsupportedFeature (bit 3 and every bit except 0, 1, 2, 6, 11, 13; bits 1 and 2
//      only with method 8), UnsupportedMethod, SpecialFile (Unix mode type only when version-made-by OS is 3;
//      regular, directory or unspecified pass), the directory rule (name ends with '/' if and only if the mode
//      or the name says directory; a directory is method 0 with both sizes 0, else BadSizes). ZIP64 entry extra:
//      only sentinel fields, in uncompressed, compressed, offset order, exact length, canonical; malformed is
//      Zip64Mismatch (central) or BadLocalHeader (local). Other extra content is not interpreted.
//   6. Local header at the recorded offset: BadLocalHeader, then HeaderMismatch for any difference in name,
//      flags, method, CRC, sizes, modification time or date. Extras may differ.
//   7. Sizes (BadSizes): stored needs equal sizes; deflate with 0 compressed bytes is BadSizes; sums and
//      endpoints are checked in uint64, a wrap or a data end beyond the buffer is BadSizes at that entry.
//   8. Layout in central order: first offset not 0 is PrependedData; then per entry: offset not above the
//      previous one is OutOfOrder, below the previous data end is Overlap, above it is Gap; data reaching into
//      the central directory is Overlap; a last data end below the central directory start is Gap.
// An empty archive is Ok when the central directory is empty, starts at offset 0 and ends where the end record
// starts; with a central directory offset above 0 it is PrependedData.
// Offsets: end record problems report the end record (or locator) offset; entry problems report the offending
// header or field offset and the central index; other verdicts report 0 for the index. On any failure entries and
// directory_count are empty. Duplicate names and collisions are left to entry_policy.
//
// Layout accepted (everything else is rejected):
//   [local header + data] x N, in central directory order, with no gap, starting at offset 0;
//   central directory, starting exactly where the last entry data ends;
//   optionally the ZIP64 end record and locator, then the end record, ending exactly at the buffer end
//   after its comment.
// Directory entries: name ends with '/', method 0, both sizes 0. They are audited like any entry and
// returned only through directory_count.
ZipAuditResult audit_zip(const std::uint8_t* data, std::size_t size, const ZipAuditLimits& limits = {});

// The entry list in the form validate_entries expects: names and sizes copied in order, no filtering.
std::vector<EntryInfo> to_entry_infos(const std::vector<ZipEntry>& entries);

const char* to_string(ZipVerdict v);

}  // namespace lumora::package
