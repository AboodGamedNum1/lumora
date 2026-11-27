// Tests for core/intmath (scope AN). Expected values come from an independent Python reference (tools/intmath_ref.py: exact Python
// integers, bisection for the square root, the product held as a Python int, and its own FNV) and are pasted below: seeded operand
// streams (the generator is mirrored here) whose every result is folded into a digest, per function. Direct cases cover the header
// rules, edge values of every function, the failure rules (out untouched), and independent property checks. Evidence label: local unit
// tests only.
#include <catch2/catch_test_macros.hpp>
#include <cstdint>

#include "intmath.h"

using namespace lumora::intmath;

namespace {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;

constexpr u64 kMax = ~0ull;
constexpr i64 kIMax = 0x7FFFFFFFFFFFFFFFll;
constexpr i64 kIMin = -kIMax - 1;
constexpr u64 kSent64 = 0x0123456789ABCDEFull;

struct Fnv {
  u64 h = 0xcbf29ce484222325ull;
  void word(u32 v) {
    for (int k = 0; k < 4; ++k) {
      h ^= (v >> (8 * k)) & 0xFFu;
      h *= 0x100000001b3ull;
    }
  }
  void w64(u64 v) {
    word(static_cast<u32>(v));
    word(static_cast<u32>(v >> 32));
  }
};

struct Lcg {
  u64 s;
  explicit Lcg(u64 seed) : s(seed) {}
  u32 next() {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<u32>(s >> 32);
  }
  u64 next64() {
    const u64 hi = next();
    return (hi << 32) | next();
  }
  u32 rng(u32 lo, u32 hi) { return static_cast<u32>(lo + next() % (static_cast<u64>(hi) - lo + 1)); }
};

const u64 kDigIsqrt = 11305404857080486073ull;
const u64 kDigGcd = 7924371671985788463ull;
const u64 kDigLcm = 16697778659826900022ull;
const u64 kDigAdd = 13998881642923389126ull;
const u64 kDigSub = 17610852796698288935ull;
const u64 kDigMul = 7690503106976068532ull;
const u64 kDigMulDiv = 12205073751874026952ull;
const u32 kN = 3000;

const u64 kSpecial[9] = {0, 1, 2, kMax, kMax - 1, 1ull << 63, (1ull << 63) - 1, 1ull << 32, (1ull << 32) - 1};

u64 gen(Lcg& r) {
  if (r.rng(0, 9) == 0) return kSpecial[r.rng(0, 8)];
  const u32 k = r.rng(0, 64);
  const u64 v = r.next64();
  return k == 64 ? v : (v & ((1ull << k) - 1));
}

i64 sgn(u64 v) { return v >= (1ull << 63) ? static_cast<i64>(v - (1ull << 63)) + kIMin : static_cast<i64>(v); }

}  // namespace

TEST_CASE("intmath: seeded streams match the independent reference") {
  {
    Lcg r(1);
    Fnv f;
    for (u32 i = 0; i < kN; ++i) f.word(isqrt_u64(gen(r)));
    CHECK(f.h == kDigIsqrt);
  }
  {
    Lcg r(2);
    Fnv f;
    for (u32 i = 0; i < kN; ++i) {
      u64 a, b;
      if (r.rng(0, 2) == 0) {
        const u64 g = gen(r);
        const u64 x = r.rng(0, 65535);
        const u64 y = r.rng(0, 65535);
        a = x * g;
        b = y * g;
      } else {
        a = gen(r);
        b = gen(r);
      }
      f.w64(gcd_u64(a, b));
    }
    CHECK(f.h == kDigGcd);
  }
  {
    Lcg r(3);
    Fnv f;
    for (u32 i = 0; i < kN; ++i) {
      u64 a, b;
      if (r.rng(0, 2) == 0) {
        const u64 g = gen(r) & 0xFFFFFFFFull;
        const u64 x = r.rng(0, 65535);
        const u64 y = r.rng(0, 65535);
        a = x * g;
        b = y * g;
      } else {
        a = gen(r);
        b = gen(r);
      }
      u64 out = kSent64;
      const bool ok = lcm_u64(a, b, &out);
      f.word(ok ? 1u : 0u);
      f.w64(out);
    }
    CHECK(f.h == kDigLcm);
  }
  for (int op = 0; op < 3; ++op) {
    Lcg r(4 + op);
    Fnv f;
    for (u32 i = 0; i < kN; ++i) {
      const i64 a = sgn(gen(r));
      i64 b = sgn(gen(r));
      if (r.rng(0, 3) == 0) b = op != 1 ? a : (a != kIMin ? -a : a);
      i64 out = static_cast<i64>(kSent64);
      const bool ok = op == 0 ? add_i64(a, b, &out) : op == 1 ? sub_i64(a, b, &out) : mul_i64(a, b, &out);
      f.word(ok ? 1u : 0u);
      f.w64(static_cast<u64>(out));
    }
    CHECK(f.h == (op == 0 ? kDigAdd : op == 1 ? kDigSub : kDigMul));
  }
  {
    Lcg r(7);
    Fnv f;
    for (u32 i = 0; i < kN; ++i) {
      u64 a, b, d;
      const u32 sel = r.rng(0, 3);
      if (sel == 0) {
        a = gen(r);
        b = gen(r);
        d = gen(r);
      } else if (sel == 1) {
        a = gen(r);
        b = gen(r);
        d = a;
      } else if (sel == 2) {
        a = r.next();
        b = r.next();
        d = gen(r);
      } else {
        const u64 x = static_cast<u64>(r.next()) + 1;
        d = 2 * x;
        a = x;
        b = 2 * static_cast<u64>(r.next()) + 1;
      }
      const u32 mode = r.rng(0, 4);
      u64 out = kSent64;
      const bool ok = mul_div_u64(a, b, d, static_cast<Round>(mode), &out);
      f.word(ok ? 1u : 0u);
      f.w64(out);
    }
    CHECK(f.h == kDigMulDiv);
  }
}

TEST_CASE("intmath: isqrt") {
  CHECK(isqrt_u64(0) == 0u);
  CHECK(isqrt_u64(1) == 1u);
  CHECK(isqrt_u64(2) == 1u);
  CHECK(isqrt_u64(3) == 1u);
  CHECK(isqrt_u64(4) == 2u);
  CHECK(isqrt_u64(kMax) == 4294967295u);
  CHECK(isqrt_u64(kMax - 1) == 4294967295u);
  CHECK(isqrt_u64(4294967295ull * 4294967295ull) == 4294967295u);
  CHECK(isqrt_u64(4294967295ull * 4294967295ull - 1) == 4294967294u);
  CHECK(isqrt_u64(1ull << 63) == 3037000499u);
  CHECK(isqrt_u64(1ull << 62) == 1ull << 31);
  CHECK(isqrt_u64((1ull << 62) - 1) == (1ull << 31) - 1);
  // every x below 2^20 by the defining inequality
  for (u64 x = 0; x < (1ull << 20); ++x) {
    const u64 r = isqrt_u64(x);
    REQUIRE(r * r <= x);
    REQUIRE((r + 1) * (r + 1) > x);
  }
  // around every perfect square k^2 for a spread of k up to 2^32 - 1
  Lcg g(99);
  for (u32 n = 0; n < 20000; ++n) {
    const u64 k = n < 3000 ? n + (1ull << 31) : (n < 6000 ? 4294967295ull - (n - 3000) : (n < 12000 ? g.next() : (g.next() >> (g.next() & 31))));
    const u64 sq = k * k;
    REQUIRE(isqrt_u64(sq) == k);
    if (sq > 0) REQUIRE(isqrt_u64(sq - 1) == k - 1);
    if (k > 0 && k < 4294967295ull) REQUIRE(isqrt_u64(sq + 1) == k);
    if (k < 4294967295ull) REQUIRE(isqrt_u64(sq + 2 * k) == k);
    if (k < 4294967295ull) REQUIRE(isqrt_u64(sq + 2 * k + 1) == k + 1);
  }
  // random 64-bit values by the inequality
  Lcg r(5);
  for (int n = 0; n < 20000; ++n) {
    const u64 x = r.next64() >> (r.next() & 63);
    const u64 s = isqrt_u64(x);
    REQUIRE(s * s <= x);
    REQUIRE((s == 4294967295ull ? true : (s + 1) * (s + 1) > x));
  }
}

TEST_CASE("intmath: gcd and lcm") {
  CHECK(gcd_u64(0, 0) == 0u);
  CHECK(gcd_u64(7, 0) == 7u);
  CHECK(gcd_u64(0, 7) == 7u);
  CHECK(gcd_u64(kMax, 0) == kMax);
  CHECK(gcd_u64(0, kMax) == kMax);
  CHECK(gcd_u64(12, 18) == 6u);
  CHECK(gcd_u64(18, 12) == 6u);
  CHECK(gcd_u64(17, 13) == 1u);
  CHECK(gcd_u64(9, 9) == 9u);
  CHECK(gcd_u64(kMax, kMax) == kMax);
  CHECK(gcd_u64(kMax, kMax - 1) == 1u);
  CHECK(gcd_u64(1ull << 63, 1ull << 32) == 1ull << 32);
  CHECK(gcd_u64(1000000007ull * 3, 1000000007ull * 5) == 1000000007ull);
  Lcg r(11);
  for (int n = 0; n < 5000; ++n) {
    const u64 a = gen(r);
    const u64 b = gen(r);
    const u64 g = gcd_u64(a, b);
    REQUIRE(g == gcd_u64(b, a));
    if (g != 0) {
      REQUIRE(a % g == 0);
      REQUIRE(b % g == 0);
      REQUIRE(gcd_u64(a / g, b / g) == 1u);
    } else {
      REQUIRE(a == 0);
      REQUIRE(b == 0);
    }
  }
  u64 out = kSent64;
  CHECK(lcm_u64(4, 6, &out));
  CHECK(out == 12u);
  CHECK(lcm_u64(0, 5, &out));
  CHECK(out == 0u);
  out = kSent64;
  CHECK(lcm_u64(5, 0, &out));
  CHECK(out == 0u);
  out = kSent64;
  CHECK(lcm_u64(0, 0, &out));
  CHECK(out == 0u);
  CHECK(lcm_u64(1, kMax, &out));
  CHECK(out == kMax);
  CHECK(lcm_u64(kMax, kMax, &out));
  CHECK(out == kMax);
  CHECK(lcm_u64(kMax, 1, &out));
  CHECK(out == kMax);
  CHECK(lcm_u64(1ull << 32, 1ull << 31, &out));
  CHECK(out == 1ull << 32);
  CHECK(lcm_u64(1ull << 63, 2, &out));
  CHECK(out == 1ull << 63);
  CHECK(lcm_u64(4294967295ull, 4294967297ull, &out));  // coprime, product 2^64 - 1
  CHECK(out == kMax);
  out = kSent64;
  CHECK_FALSE(lcm_u64(4294967296ull, 4294967297ull, &out));  // product 2^64 + 2^32
  CHECK_FALSE(lcm_u64(kMax, kMax - 1, &out));
  CHECK_FALSE(lcm_u64(1ull << 63, 3, &out));
  CHECK_FALSE(lcm_u64(1ull << 40, 1ull << 41 | 1, &out));
  CHECK(out == kSent64);
  // null out is false, even for a zero operand
  CHECK_FALSE(lcm_u64(4, 6, nullptr));
  CHECK_FALSE(lcm_u64(0, 6, nullptr));
  CHECK_FALSE(lcm_u64(0, 0, nullptr));
  // property: lcm * gcd == a * b when it fits
  for (int n = 0; n < 5000; ++n) {
    const u64 a = gen(r) & 0xFFFFFFFFull;
    const u64 b = gen(r) & 0xFFFFFFFFull;
    u64 l = kSent64;
    REQUIRE(lcm_u64(a, b, &l));
    if (a == 0 || b == 0) REQUIRE(l == 0);
    else REQUIRE(l * gcd_u64(a, b) == a * b);
  }
}

TEST_CASE("intmath: checked signed arithmetic") {
  const i64 sent = static_cast<i64>(kSent64);
  i64 out = sent;
  CHECK(add_i64(1, 2, &out));
  CHECK(out == 3);
  CHECK(add_i64(kIMax, 0, &out));
  CHECK(out == kIMax);
  CHECK(add_i64(kIMin, 0, &out));
  CHECK(out == kIMin);
  CHECK(add_i64(kIMax, kIMin, &out));
  CHECK(out == -1);
  CHECK(add_i64(kIMin, kIMax, &out));
  CHECK(out == -1);
  CHECK(add_i64(kIMax - 1, 1, &out));
  CHECK(out == kIMax);
  CHECK(add_i64(kIMin + 1, -1, &out));
  CHECK(out == kIMin);
  CHECK(add_i64(-5, -7, &out));
  CHECK(out == -12);
  out = sent;
  CHECK_FALSE(add_i64(kIMax, 1, &out));
  CHECK_FALSE(add_i64(1, kIMax, &out));
  CHECK_FALSE(add_i64(kIMax, kIMax, &out));
  CHECK_FALSE(add_i64(kIMin, -1, &out));
  CHECK_FALSE(add_i64(-1, kIMin, &out));
  CHECK_FALSE(add_i64(kIMin, kIMin, &out));
  CHECK(out == sent);
  CHECK_FALSE(add_i64(1, 2, nullptr));

  CHECK(sub_i64(5, 7, &out));
  CHECK(out == -2);
  CHECK(sub_i64(kIMin, 0, &out));
  CHECK(out == kIMin);
  CHECK(sub_i64(0, kIMax, &out));
  CHECK(out == -kIMax);
  CHECK(sub_i64(-1, kIMax, &out));
  CHECK(out == kIMin);
  CHECK(sub_i64(kIMin, kIMin, &out));
  CHECK(out == 0);
  CHECK(sub_i64(kIMax, kIMax, &out));
  CHECK(out == 0);
  CHECK(sub_i64(kIMin, -1, &out));
  CHECK(out == kIMin + 1);
  CHECK(sub_i64(kIMax, -1 + 1, &out));
  CHECK(out == kIMax);
  CHECK(sub_i64(-1, kIMin, &out));
  CHECK(out == kIMax);
  out = sent;
  CHECK_FALSE(sub_i64(0, kIMin, &out));
  CHECK_FALSE(sub_i64(kIMin, 1, &out));
  CHECK_FALSE(sub_i64(kIMax, -1, &out));
  CHECK_FALSE(sub_i64(kIMax, kIMin, &out));
  CHECK_FALSE(sub_i64(kIMin, kIMax, &out));
  CHECK_FALSE(sub_i64(-2, kIMax, &out));
  CHECK(out == sent);
  CHECK_FALSE(sub_i64(1, 2, nullptr));

  CHECK(mul_i64(6, 7, &out));
  CHECK(out == 42);
  CHECK(mul_i64(-6, 7, &out));
  CHECK(out == -42);
  CHECK(mul_i64(-6, -7, &out));
  CHECK(out == 42);
  CHECK(mul_i64(kIMin, 1, &out));
  CHECK(out == kIMin);
  CHECK(mul_i64(1, kIMin, &out));
  CHECK(out == kIMin);
  CHECK(mul_i64(kIMin, 0, &out));
  CHECK(out == 0);
  CHECK(mul_i64(0, kIMin, &out));
  CHECK(out == 0);
  CHECK(mul_i64(kIMax, 1, &out));
  CHECK(out == kIMax);
  CHECK(mul_i64(kIMax, -1, &out));
  CHECK(out == -kIMax);
  CHECK(mul_i64(-1, kIMax, &out));
  CHECK(out == -kIMax);
  CHECK(mul_i64(1ll << 31, 1ll << 31, &out));
  CHECK(out == 1ll << 62);
  CHECK(mul_i64(-(1ll << 31), 1ll << 32, &out));
  CHECK(out == kIMin);
  CHECK(mul_i64(1ll << 32, -(1ll << 31), &out));
  CHECK(out == kIMin);
  CHECK(mul_i64(3037000499ll, 3037000499ll, &out));
  CHECK(out == 3037000499ll * 3037000499ll);
  CHECK(mul_i64(-3037000499ll, 3037000499ll, &out));
  CHECK(out == -3037000499ll * 3037000499ll);
  out = sent;
  CHECK_FALSE(mul_i64(kIMin, -1, &out));
  CHECK_FALSE(mul_i64(-1, kIMin, &out));
  CHECK_FALSE(mul_i64(kIMin, 2, &out));
  CHECK_FALSE(mul_i64(2, kIMin, &out));
  CHECK_FALSE(mul_i64(kIMax, 2, &out));
  CHECK_FALSE(mul_i64(kIMax, -2, &out));
  CHECK_FALSE(mul_i64(kIMin, kIMin, &out));
  CHECK_FALSE(mul_i64(kIMax, kIMax, &out));
  CHECK_FALSE(mul_i64(1ll << 32, 1ll << 31, &out));
  CHECK_FALSE(mul_i64(-(1ll << 32), -(1ll << 31), &out));
  CHECK_FALSE(mul_i64(3037000500ll, 3037000500ll, &out));
  CHECK_FALSE(mul_i64(-3037000500ll, 3037000500ll, &out));
  CHECK(out == sent);
  CHECK_FALSE(mul_i64(1, 2, nullptr));
  CHECK_FALSE(mul_i64(0, 0, nullptr));
  CHECK_FALSE(add_i64(0, 0, nullptr));
  CHECK_FALSE(sub_i64(0, 0, nullptr));
}

TEST_CASE("intmath: mul_div_u64 rounding table") {
  struct Row {
    u64 a, b, d;
    bool ok;
    u64 down, up, nearest;
  };
  const Row rows[] = {
      {7, 3, 2, true, 10, 11, 11},       // 21/2 = 10.5: tie rounds up
      {7, 3, 4, true, 5, 6, 5},          // 5.25
      {7, 3, 5, true, 4, 5, 4},          // 4.2
      {7, 3, 6, true, 3, 4, 4},          // 3.5: tie rounds up
      {7, 3, 7, true, 3, 3, 3},          // exact
      {10, 1, 3, true, 3, 4, 3},         // 3.33
      {10, 2, 3, true, 6, 7, 7},         // 6.67
      {1, 1, 2, true, 0, 1, 1},          // 0.5: tie rounds up
      {1, 1, 3, true, 0, 1, 0},
      {1, 2, 3, true, 0, 1, 1},
      {0, 5, 3, true, 0, 0, 0},
      {5, 0, 3, true, 0, 0, 0},
      {0, 0, 1, true, 0, 0, 0},
      {0, kMax, kMax, true, 0, 0, 0},
      {kMax, kMax, kMax, true, kMax, kMax, kMax},
      {kMax, 1, 1, true, kMax, kMax, kMax},
      {kMax, 2, 2, true, kMax, kMax, kMax},
      {kMax, kMax, 1, false, 0, 0, 0},
      {kMax, 2, 1, false, 0, 0, 0},
      {1ull << 63, 2, 1, false, 0, 0, 0},
      {1ull << 63, 2, 2, true, 1ull << 63, 1ull << 63, 1ull << 63},
      {1ull << 32, 1ull << 32, 1, false, 0, 0, 0},
      {1ull << 32, 1ull << 32, 2, true, 1ull << 63, 1ull << 63, 1ull << 63},
      {1ull << 32, 1ull << 32, 3, true, 6148914691236517205ull, 6148914691236517206ull, 6148914691236517205ull},
      {kMax, kMax, kMax - 1, false, 0, 0, 0},  // floor is 2^64 + 1
      {kMax, kMax - 1, kMax, true, kMax - 1, kMax - 1, kMax - 1},
      {kMax, 3, 4, true, 13835058055282163711ull, 13835058055282163712ull, 13835058055282163711ull},  // 3*(2^64-1)/4
      {kMax - 1, 2, 2, true, kMax - 1, kMax - 1, kMax - 1},
      {kMax, 2, 3, true, 12297829382473034410ull, 12297829382473034410ull, 12297829382473034410ull},  // 2(2^64-1)/3 exact
      {123456789, 987654321, 1000, true, 121932631112635ull, 121932631112636ull, 121932631112635ull},
  };
  for (const Row& r : rows) {
    u64 out = kSent64;
    const bool okd = mul_div_u64(r.a, r.b, r.d, Round::Down, &out);
    CHECK(okd == r.ok);
    CHECK(out == (r.ok ? r.down : kSent64));
    out = kSent64;
    const bool oku = mul_div_u64(r.a, r.b, r.d, Round::Up, &out);
    CHECK(oku == r.ok);
    CHECK(out == (r.ok ? r.up : kSent64));
    out = kSent64;
    const bool okn = mul_div_u64(r.a, r.b, r.d, Round::NearestUp, &out);
    CHECK(okn == r.ok);
    CHECK(out == (r.ok ? r.nearest : kSent64));
  }
}

TEST_CASE("intmath: mul_div_u64 carry into 2^64 and failure rules") {
  u64 out = kSent64;
  // floor is exactly UINT64_MAX with a remainder (witnesses found with the Python reference): Down fits; Up never fits; NearestUp fits
  // only when the remainder is below half of d
  struct W {
    u64 a, b, d;
    bool nearest_fits;
  };
  const W ws[] = {{31, 1190112520884487201ull, 2, false},          // remainder 1 of 2: exactly half, rounds up, carries
                  {7, 10540996613548315209ull, 4, false},          // remainder 3 of 4
                  {623347480303ull, 18446740157248869839ull, 623347347959ull, true}};  // remainder 177787877432 of 623347347959
  for (const W& w : ws) {
    out = kSent64;
    REQUIRE(mul_div_u64(w.a, w.b, w.d, Round::Down, &out));
    CHECK(out == kMax);
    out = kSent64;
    CHECK_FALSE(mul_div_u64(w.a, w.b, w.d, Round::Up, &out));
    CHECK(out == kSent64);
    const bool ok = mul_div_u64(w.a, w.b, w.d, Round::NearestUp, &out);
    CHECK(ok == w.nearest_fits);
    CHECK(out == (w.nearest_fits ? kMax : kSent64));
  }
  // the exact product 2^64 over 1 does not fit in any mode
  out = kSent64;
  CHECK_FALSE(mul_div_u64(1ull << 32, 1ull << 32, 1, Round::Down, &out));
  CHECK_FALSE(mul_div_u64(1ull << 32, 1ull << 32, 1, Round::Up, &out));
  CHECK_FALSE(mul_div_u64(1ull << 32, 1ull << 32, 1, Round::NearestUp, &out));
  CHECK(out == kSent64);
  // d == 0, null out and an invalid mode are false, in every combination, with a zero product too
  const Round bad[] = {static_cast<Round>(3), static_cast<Round>(4), static_cast<Round>(255)};
  const u64 prods[][2] = {{0, 0}, {0, 5}, {5, 0}, {6, 7}, {kMax, kMax}};
  for (const auto& p : prods) {
    for (Round m : {Round::Down, Round::Up, Round::NearestUp}) {
      out = kSent64;
      CHECK_FALSE(mul_div_u64(p[0], p[1], 0, m, &out));
      CHECK(out == kSent64);
      CHECK_FALSE(mul_div_u64(p[0], p[1], 1, m, nullptr));
      CHECK_FALSE(mul_div_u64(p[0], p[1], 0, m, nullptr));
    }
    for (Round m : bad) {
      out = kSent64;
      CHECK_FALSE(mul_div_u64(p[0], p[1], 1, m, &out));
      CHECK_FALSE(mul_div_u64(p[0], p[1], 7, m, &out));
      CHECK_FALSE(mul_div_u64(p[0], p[1], kMax, m, &out));
      CHECK(out == kSent64);
    }
  }
  // a zero product gives 0 in every mode
  for (Round m : {Round::Down, Round::Up, Round::NearestUp}) {
    out = kSent64;
    REQUIRE(mul_div_u64(0, kMax, 3, m, &out));
    CHECK(out == 0u);
    out = kSent64;
    REQUIRE(mul_div_u64(kMax, 0, kMax, m, &out));
    CHECK(out == 0u);
  }
}

TEST_CASE("intmath: mul_div_u64 against 64-bit arithmetic when the product fits") {
  Lcg r(21);
  for (int n = 0; n < 20000; ++n) {
    const u64 a = r.next();
    const u64 b = r.next();
    u64 d = gen(r);
    if (d == 0) d = 1;
    const u64 p = a * b;  // below 2^64
    const u64 q = p / d;
    const u64 rem = p % d;
    u64 out = kSent64;
    REQUIRE(mul_div_u64(a, b, d, Round::Down, &out));
    REQUIRE(out == q);
    REQUIRE(mul_div_u64(a, b, d, Round::Up, &out));
    REQUIRE(out == q + (rem != 0 ? 1u : 0u));
    REQUIRE(mul_div_u64(a, b, d, Round::NearestUp, &out));
    REQUIRE(out == q + (rem >= d - rem ? 1u : 0u));
  }
}
