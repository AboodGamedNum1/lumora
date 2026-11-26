// Extraction plan for an audited and policy-checked package. Pure function on names and sizes: no file
// system, no free-space query, no I/O. The caller (the importer) passes the free bytes it measured and
// creates exactly what the plan lists, in the plan's order. Source of the rules: docs/security-model.md,
// "Archive defenses" (free-space check before extraction, cancellable extraction with full cleanup of partial
// output, integer-overflow-safe size handling). Evidence label: unit tests only.
//
// Input contract: names already passed validate_name and validate_entries (core/package/entry_policy.h), so
// they are valid UTF-8, relative, without "..", empty components, backslashes or reserved names. The plan
// still re-checks what only the whole set can show: a name that is both a file and the parent directory of
// another entry.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "entry_policy.h"

namespace lumora::package {

struct ExtractLimits {
  std::uint32_t max_depth = 16;                 // components per path; "a/b/c" is depth 3
  std::uint64_t max_path_bytes = 1024;          // whole relative path, separators included
  std::uint64_t max_directories = 4096;         // created directories, parents included
  std::uint64_t reserve_bytes = 64ull << 20;    // free space that must remain after extraction
};

enum class ExtractVerdict {
  Ok,
  TooManyDirectories,   // more distinct directories than max_directories
  PathTooDeep,          // an entry path with more than max_depth components
  PathTooLong,          // an entry path longer than max_path_bytes
  FileDirectoryConflict,// one name is a file and also the parent directory of another name, compared with the
                        // same folding the entry policy uses (ASCII case, then the Latin-1 normalization)
  SizeOverflow,         // the sum of uncompressed sizes plus reserve_bytes does not fit 64 bits
  InsufficientSpace     // free_bytes < total uncompressed bytes + reserve_bytes
};

struct ExtractPlan {
  ExtractVerdict verdict = ExtractVerdict::Ok;
  std::size_t entry_index = 0;                  // offending entry when one caused the verdict
  std::vector<std::string> directories;         // every directory to create, parents before children, each once,
                                                // sorted by byte order within one depth, shallow depth first
  std::vector<std::size_t> file_order;          // entry indices in creation order: input order
  std::uint64_t total_bytes = 0;                // sum of uncompressed sizes (valid when verdict is Ok or
                                                // InsufficientSpace)
  std::uint64_t required_free_bytes = 0;        // total_bytes + reserve_bytes (same validity)
};

// Directory names are derived from the entry names (every proper prefix ending at a '/'), never taken from
// the archive's directory entries. The check order is: PathTooLong, PathTooDeep (per entry in order), then
// FileDirectoryConflict, TooManyDirectories, SizeOverflow, InsufficientSpace. On any failure directories and
// file_order are empty.
// Binding details:
//   - Per-entry stage: the first failing entry in input order wins, PathTooLong before PathTooDeep for that entry.
//     Depth counts components including the file name, equality with the limit passes, a limit of 0 rejects every
//     entry. Path length is the name bytes including separators, equality passes.
//   - FileDirectoryConflict: two names conflict when one is a file name and the other has it as a parent
//     directory, and the two strings are equal exactly, or equal after ASCII case folding alone, or equal after
//     the Latin-1 normalization alone. The foldings are never composed (uppercase E plus combining acute does not
//     match lowercase precomposed e-acute), as in the entry policy's open cases. entry_index is the earliest input
//     entry whose name is the conflicting file. The folding helpers stay private to entry_policy.cpp: the plan
//     keeps its own copy and cross-checks it against validate_entries in tests.
//   - TooManyDirectories counts distinct exact parent strings (no folding; a/x and A/y count two), proper prefixes
//     ending at a '/'. Equality with max_directories passes, 0 allows top-level files only. entry_index is the
//     entry that introduces the first directory above the limit.
//   - SizeOverflow: entry_index is the entry whose size overflows the running sum; when only total plus
//     reserve_bytes overflows, entry_index is 0. total_bytes and required_free_bytes are 0 on every failure except
//     InsufficientSpace (entry_index 0, both filled). Equality of free_bytes with the requirement passes.
//   - Directories that differ only in case (a/x and A/y) both appear in the plan. The caller must treat "already
//     exists" as success when creating directories. Open case, documented.
//   - Extraction goes into a new, empty target directory chosen by the caller. cleanup_paths lists every directory
//     of the plan and would delete one that existed before, so the caller must not point it at an existing tree.
//   - Names are not revalidated. A name that breaks the input contract must not crash the plan; it is split on '/'
//     as given and gets no new verdict.
ExtractPlan plan_extraction(const std::vector<EntryInfo>& entries, std::uint64_t free_bytes,
                            const ExtractLimits& limits = {});

// Cleanup after a cancelled or failed extraction: given how many entries of file_order were fully written
// (the next one may be partial), returns the relative paths to delete, in deletion order: the partial file,
// the written files in reverse, then every directory of the plan in reverse (children before parents).
// completed_files is clamped to the number of files in file_order; partial_file_exists names the file at
// file_order[completed_files] only when completed_files is below that number. A plan whose verdict is not Ok
// gives an empty list. An index in file_order outside entries is skipped.
// Deleting a path that does not exist is the caller's concern. Never returns a path outside the plan.
std::vector<std::string> cleanup_paths(const ExtractPlan& plan, const std::vector<EntryInfo>& entries,
                                       std::size_t completed_files, bool partial_file_exists);

const char* to_string(ExtractVerdict v);

}  // namespace lumora::package
