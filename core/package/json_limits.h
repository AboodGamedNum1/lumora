// Structural limits for untrusted JSON (scene.json, settings.json), from docs/security-model.md:
// "explicit nesting, size, string-length and duplicate-key limits". This is a pre-scan: it reads
// the text once, builds no document, and says whether the text stays inside the limits. A real
// parser runs only on text that passed. It is not a full JSON validator, but it must reject
// text it cannot scan safely (unterminated strings, bad escapes, unbalanced brackets).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace lumora::package {

struct JsonLimits {
  std::uint64_t max_bytes = 4ull << 20;
  std::uint32_t max_depth = 32;             // arrays and objects together; the top level is depth 1
  std::uint64_t max_string_bytes = 64 * 1024;  // decoded length of one string or one key
  std::uint64_t max_keys_per_object = 4096;
  std::uint64_t max_array_items = 65536;
  std::uint64_t max_total_values = 1ull << 20;
};

enum class JsonVerdict {
  Ok,
  TooLarge,
  TooDeep,
  StringTooLong,
  TooManyKeys,
  TooManyArrayItems,
  TooManyValues,
  DuplicateKey,   // same decoded key twice in one object
  Malformed       // unbalanced or unterminated structure, bad escape, control character in a string
};

struct JsonScanResult {
  JsonVerdict verdict = JsonVerdict::Ok;
  std::size_t offset = 0;  // byte offset where the problem was found (0 when Ok)
};

JsonScanResult scan_json(std::string_view text, const JsonLimits& limits = {});
const char* to_string(JsonVerdict v);

}  // namespace lumora::package
