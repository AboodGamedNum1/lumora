// Load-time resource budgets for a scene package (docs/security-model.md, "Resource limits": load-time budgets
// for texture memory, script size, node count, audio size and duration, with graceful rejection). Pure function
// on numbers the loader already read from validated headers: no decoding, no file system, no allocation beyond
// the result. A package that exceeds a budget is rejected with a verdict and the offending item, never
// partially loaded. Evidence label: unit tests only.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lumora::package {

enum class TextureFormat : std::uint8_t {
  R8,       // 1 byte per texel
  Rg8,      // 2 bytes per texel
  Rgba8,    // 4 bytes per texel
  Bc1,      // 8 bytes per 4x4 block
  Bc7       // 16 bytes per 4x4 block
};

struct TextureInfo {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  TextureFormat format = TextureFormat::Rgba8;
  std::uint32_t mip_levels = 1;  // 1 to the full chain length for the size; 0 is invalid
  std::uint32_t layers = 1;      // array layers or cube faces; 0 is invalid
};

struct AudioInfo {
  std::uint64_t encoded_bytes = 0;
  std::uint64_t duration_ms = 0;
};

struct SceneStats {
  std::vector<TextureInfo> textures;
  std::vector<std::uint64_t> script_bytes;  // one per script file
  std::uint64_t node_count = 0;
  std::vector<AudioInfo> audio;
};

struct LoadBudgetLimits {
  std::uint64_t max_textures = 1024;
  std::uint32_t max_texture_dimension = 8192;            // width and height each
  std::uint64_t max_texture_bytes_each = 128ull << 20;   // one texture with all mips and layers
  std::uint64_t max_texture_bytes_total = 512ull << 20;
  std::uint64_t max_script_bytes_each = 1ull << 20;
  std::uint64_t max_script_bytes_total = 4ull << 20;
  std::uint64_t max_nodes = 100000;
  std::uint64_t max_audio_bytes_each = 32ull << 20;
  std::uint64_t max_audio_bytes_total = 128ull << 20;
  std::uint64_t max_audio_ms_each = 10ull * 60 * 1000;
};

enum class BudgetVerdict {
  Ok,
  TooManyTextures,
  BadTexture,          // zero size or layers, mip_levels 0 or above the full chain, unknown format value
  TextureDimension,    // width or height above max_texture_dimension
  TextureTooLarge,     // one texture above max_texture_bytes_each
  TextureBudget,       // running total above max_texture_bytes_total
  ScriptTooLarge,
  ScriptBudget,
  TooManyNodes,
  AudioTooLarge,       // one clip above max_audio_bytes_each
  AudioTooLong,        // one clip above max_audio_ms_each
  AudioBudget,         // running total above max_audio_bytes_total
  SizeOverflow         // a texture size or a running total does not fit 64 bits
};

struct BudgetResult {
  BudgetVerdict verdict = BudgetVerdict::Ok;
  std::size_t item_index = 0;          // offending texture, script or audio index; 0 for counts
  std::uint64_t texture_bytes = 0;     // running texture total at the point of failure, or the final total
  std::uint64_t script_bytes = 0;
  std::uint64_t audio_bytes = 0;
};

// Bytes of one texture: every mip level l below mip_levels has max(1, w >> l) by max(1, h >> l) texels (block
// formats: ceil(that / 4) blocks per side), times the block or texel size, summed, times layers. Tight packing,
// no padding or alignment. Returns false when the arithmetic would overflow 64 bits or the texture is invalid
// (the caller maps that to SizeOverflow or BadTexture; see check_load_budget for which).
bool texture_bytes(const TextureInfo& t, std::uint64_t* out);

// Check order (first failure wins): textures in order (count first, then per texture BadTexture,
// TextureDimension, SizeOverflow, TextureTooLarge, TextureBudget), then scripts in order (ScriptTooLarge,
// ScriptBudget with overflow as SizeOverflow), then TooManyNodes, then audio in order (AudioTooLarge,
// AudioTooLong, AudioBudget). Limits accept the value itself and reject the value plus one.
// Binding details:
//   - Per texture: validate first (zero width, height or layers, mip_levels 0, mip_levels above floor(log2(max(w, h)))
//     + 1, an unknown format value: BadTexture, before any dimension check), then TextureDimension, then compute
//     the bytes (arithmetic failure: SizeOverflow at that texture), then TextureTooLarge, then the running total
//     (its overflow: SizeOverflow, then TextureBudget). texture_bytes returns false for an invalid texture or an
//     overflow and leaves *out untouched.
//   - TooManyTextures (strict excess of max_textures) is checked before any single texture and reports index 0;
//     TooManyNodes likewise. There are no count limits for scripts or audio.
//   - Zero limits accept equality and reject any positive value; max_textures 0 rejects any texture and
//     max_texture_dimension 0 rejects every otherwise valid texture. Empty lists, zero-byte scripts and clips,
//     zero duration and zero nodes pass.
//   - Totals in the result count only items accepted before the offending one; the rejected item is never added.
//     A later stage's total stays 0 when an earlier stage fails; a script failure keeps the final texture total; a
//     node or audio failure keeps the final texture and script totals; Ok carries all final totals.
//   - Inputs must come from validated headers of the real files; this module trusts nothing else about them.
//   - Block formats: ceil(w / 4) * ceil(h / 4) blocks per level, at least one; Bc1 is 8 bytes and Bc7 16 bytes per
//     block. to_string of an out-of-range enum value is "Unknown".
BudgetResult check_load_budget(const SceneStats& stats, const LoadBudgetLimits& limits = {});

const char* to_string(BudgetVerdict v);
const char* to_string(TextureFormat f);

}  // namespace lumora::package
