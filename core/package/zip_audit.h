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
  std::uint64_t max_comment_bytes = 1024; // archive comment
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

// Layout accepted (everything else is rejected):
//   [local header + data] x N, in central directory order, with no gap, starting at offset 0;
//   central directory, starting exactly where the last entry data ends;
//   optionally the ZIP64 end record and locator, then the end record, ending exactly at the buffer end
//   after its comment.
// Directory entries: name ends with '/', method 0, both sizes 0. They are audited like any entry and
// returned only through directory_count.
ZipAuditResult audit_zip(const std::uint8_t* data, std::size_t size, const ZipAuditLimits& limits = {});

// The entry list in the form validate_entries expects (names, compressed and uncompressed sizes).
std::vector<EntryInfo> to_entry_infos(const std::vector<ZipEntry>& entries);

const char* to_string(ZipVerdict v);

}  // namespace lumora::package
