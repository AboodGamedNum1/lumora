// Necklace, bracelet and Lyndon word counts modulo 998244353 (contract in necklace.h). Pure integer, no allocation, no recursion, no global state, no
// tables. Burnside's lemma over the divisors of n, found by trial division up to sqrt(n); phi and the Moebius function of each divisor by trial division.
#include "necklace.h"
namespace lumora::necklace {
namespace {
constexpr std::uint64_t kM = kMod;
constexpr std::uint64_t kInv2 = 499122177u;  // 2 * kInv2 = kMod + 1
constexpr std::uint64_t kInv4 = 748683265u;  // 4 * kInv4 = 3 * kMod + 1

std::uint64_t pow_mod(std::uint64_t base, std::uint64_t e) {
  std::uint64_t r = 1u;
  base %= kM;
  while (e != 0u) {
    if ((e & 1u) != 0u) r = r * base % kM;
    base = base * base % kM;
    e >>= 1;
  }
  return r;
}

// Euler's totient of d (d >= 1) by trial division.
std::uint64_t totient(std::uint64_t d) {
  std::uint64_t result = d;
  std::uint64_t m = d;
  for (std::uint64_t q = 2u; q * q <= m; ++q) {
    if (m % q == 0u) {
      while (m % q == 0u) m /= q;
      result -= result / q;
    }
  }
  if (m > 1u) result -= result / m;
  return result;
}

// Moebius function of d (d >= 1): 0 when a square divides d, else (-1)^(number of prime factors).
int moebius(std::uint64_t d) {
  int sign = 1;
  std::uint64_t m = d;
  for (std::uint64_t q = 2u; q * q <= m; ++q) {
    if (m % q == 0u) {
      m /= q;
      if (m % q == 0u) return 0;
      sign = -sign;
    }
  }
  if (m > 1u) sign = -sign;
  return sign;
}

// One divisor d of n, with the count e = n / d, added into the running sum for the necklace (weight phi(d)) or the Lyndon (weight mu(d)) formula.
void add_term(bool lyndon, std::uint64_t d, std::uint64_t e, std::uint64_t kr, std::uint64_t& acc) {
  const std::uint64_t t = pow_mod(kr, e);
  if (!lyndon) {
    acc = (acc + totient(d) * t) % kM;
  } else {
    const int mu = moebius(d);
    if (mu > 0) acc = (acc + t) % kM;
    if (mu < 0) acc = (acc + kM - t) % kM;
  }
}

// (1 / n) * sum over d | n of w(d) k^(n / d), for n >= 1; w = phi (lyndon false) or mu (lyndon true).
std::uint64_t burnside(std::uint32_t n, std::uint64_t kr, bool lyndon) {
  std::uint64_t acc = 0u;
  for (std::uint64_t i = 1u; i * i <= n; ++i) {
    if (n % i != 0u) continue;
    const std::uint64_t j = n / i;
    add_term(lyndon, i, j, kr, acc);
    if (j != i) add_term(lyndon, j, i, kr, acc);
  }
  return acc * pow_mod(n, kM - 2u) % kM;
}

std::uint64_t reduce_k(std::uint32_t k) { return static_cast<std::uint64_t>(k) % kM; }
}  // namespace

NeckResult necklace_count(std::uint32_t n, std::uint32_t k, std::uint32_t* out) {
  if (out == nullptr) return NeckResult::NullArgument;
  if (n > kMaxN) return NeckResult::TooLarge;
  *out = (n == 0u) ? 1u : static_cast<std::uint32_t>(burnside(n, reduce_k(k), false));
  return NeckResult::Ok;
}

NeckResult bracelet_count(std::uint32_t n, std::uint32_t k, std::uint32_t* out) {
  if (out == nullptr) return NeckResult::NullArgument;
  if (n > kMaxN) return NeckResult::TooLarge;
  if (n == 0u) {
    *out = 1u;
    return NeckResult::Ok;
  }
  const std::uint64_t kr = reduce_k(k);
  const std::uint64_t necklaces = burnside(n, kr, false);
  // Reflections: odd n, n reflections each fixing k^((n+1)/2) strings; even n, n/2 reflections fixing k^(n/2+1) and n/2 fixing k^(n/2).
  // Dihedral average = necklaces / 2 + (reflection sum) / (2 n), the reflection sum / (2 n) being k^((n+1)/2) / 2 (odd n) or
  // (k^(n/2+1) + k^(n/2)) / 4 (even n).
  std::uint64_t refl = 0u;
  if ((n & 1u) != 0u) {
    refl = pow_mod(kr, (static_cast<std::uint64_t>(n) + 1u) / 2u) * kInv2 % kM;
  } else {
    const std::uint64_t h = n / 2u;
    refl = (pow_mod(kr, h + 1u) + pow_mod(kr, h)) % kM * kInv4 % kM;
  }
  *out = static_cast<std::uint32_t>((necklaces * kInv2 + refl) % kM);
  return NeckResult::Ok;
}

NeckResult lyndon_count(std::uint32_t n, std::uint32_t k, std::uint32_t* out) {
  if (out == nullptr) return NeckResult::NullArgument;
  if (n > kMaxN) return NeckResult::TooLarge;
  *out = (n == 0u) ? 0u : static_cast<std::uint32_t>(burnside(n, reduce_k(k), true));
  return NeckResult::Ok;
}
}  // namespace lumora::necklace
