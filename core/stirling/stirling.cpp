// Stirling and Bell numbers modulo 998244353 (contract in stirling.h). Pure integer, no allocation, no recursion, no global state.
// Each table is built by one in-place row update repeated n times, so every function takes O(n^2) steps. Reduction: every product is formed in
// std::uint64_t from one factor below 4096 and one reduced entry, so it is below 2^42 and one % per step suffices.
#include "stirling.h"
namespace lumora::stirling {
namespace {
constexpr std::uint64_t kM = kMod;
}  // namespace
StirResult stirling2_row(std::uint32_t n, std::uint32_t* out) {
  if (out == nullptr) return StirResult::NullArgument;
  if (n > kMaxN) return StirResult::TooLarge;
  // Row m of S(m, k): S(m, k) = k * S(m-1, k) + S(m-1, k-1). Sweeping k downward keeps S(m-1, k-1) intact while out[k] is updated.
  out[0] = 1u;
  for (std::uint32_t k = 1; k <= n; ++k) out[k] = 0u;
  for (std::uint32_t m = 1; m <= n; ++m) {
    for (std::uint32_t k = m; k >= 1u; --k) out[k] = static_cast<std::uint32_t>((static_cast<std::uint64_t>(k) * out[k] + out[k - 1u]) % kM);
    out[0] = 0u;
  }
  return StirResult::Ok;
}
StirResult stirling1_row(std::uint32_t n, std::uint32_t* out) {
  if (out == nullptr) return StirResult::NullArgument;
  if (n > kMaxN) return StirResult::TooLarge;
  // Row m of c(m, k): c(m, k) = (m-1) * c(m-1, k) + c(m-1, k-1), swept downward in k.
  out[0] = 1u;
  for (std::uint32_t k = 1; k <= n; ++k) out[k] = 0u;
  for (std::uint32_t m = 1; m <= n; ++m) {
    const std::uint64_t f = m - 1u;
    for (std::uint32_t k = m; k >= 1u; --k) out[k] = static_cast<std::uint32_t>((f * out[k] + out[k - 1u]) % kM);
    out[0] = static_cast<std::uint32_t>((f * out[0]) % kM);
  }
  return StirResult::Ok;
}
StirResult bell_table(std::uint32_t n, std::uint32_t* scratch, std::uint32_t scratch_words, std::uint32_t* out) {
  if (out == nullptr || scratch == nullptr) return StirResult::NullArgument;
  if (n > kMaxN) return StirResult::TooLarge;
  if (scratch_words < n + 1u) return StirResult::ScratchTooSmall;
  // Aitken (Bell) triangle held as ONE row in scratch[0 .. i] while row i is current. Row i starts with the last entry of row i-1, and each further
  // entry is the previous entry of the new row plus the entry above it. B(i) is the first entry of row i. The row is rebuilt in place with a carry:
  // carry starts as the old last entry, and slot j receives the carry before the carry absorbs the old slot j. Row i uses i + 1 <= n + 1 words.
  // out is write-only: out[i] = B(i) once row i exists, so out ends holding exactly B(0) .. B(n).
  scratch[0] = 1u;
  out[0] = 1u;
  for (std::uint32_t i = 1; i <= n; ++i) {
    std::uint32_t carry = scratch[i - 1u];
    for (std::uint32_t j = 0; j < i; ++j) {
      const std::uint32_t old = scratch[j];
      scratch[j] = carry;
      std::uint32_t s = carry + old;  // both below kMod: the sum is below 2^31
      if (s >= kMod) s -= kMod;
      carry = s;
    }
    scratch[i] = carry;
    out[i] = scratch[0];
  }
  return StirResult::Ok;
}
}  // namespace lumora::stirling
