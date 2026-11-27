#!/usr/bin/env python3
"""Independent reference for core/stirling (Stirling numbers of both kinds and Bell numbers modulo 998244353).

Standard library only (no NumPy, no third-party package). Runtime: Python 3.8 or newer; `check` took about a minute on a laptop-class machine
(local measurement, see docs/stirling.md). Usage:
  python3 tools/stirling_ref.py check   verify the reference against itself (exact vs modular methods, identities, known values), print "OK"
  python3 tools/stirling_ref.py emit    print the golden digests as C++ source for tests/stirling/stirling_tests.cpp (after running the checks)
Methods, all different in shape from the C++ in-place row sweeps:
  * n <= EXACT_N: the full exact big-integer triangles (Python ints, no reduction), S(m,k) = k S(m-1,k) + S(m-1,k-1) and
    c(m,k) = (m-1) c(m-1,k) + c(m-1,k-1), reduced only when digested. Bell numbers exactly from B(m+1) = sum C(m,k) B(k) with exact binomials.
  * any n: S(n,k) by inclusion-exclusion, S(n,k) = sum_j [(-1)^(k-j) / (k-j)!] [j^n / j!] mod p (a convolution of two sequences);
    c(n,k) as the coefficients of the rising factorial x (x+1) ... (x+n-1), multiplied out by divide and conquer with schoolbook
    polynomial products; Bell numbers by B(m+1) = sum_k C(m,k) B(k) mod p with binomials from factorial tables.
  * the Aitken triangle mod p, rows 1..4096 (8394752 entries): none is congruent to 0 or to p - 1 (survivor arguments in docs/stirling.md);
  * cross-checks: row sums (c: n!, S2 row sum = Bell), sum_k (-1)^(n-k) c(n,k) S(k,m) = [n = m] exactly for n, m <= EXACT_N, the near-diagonal
    closed forms, and a few known textbook values.
Digest: FNV-1a 64-bit over the 32-bit words (one xor and one multiply per word, no byte splitting).
"""
import sys
P = 998244353
EXACT_N = 300
MAXN = 4096
ROW_SAMPLES = [512, 1000, 1024, 2047, 2048, 2049, 3000, 4095, 4096]
BELL_SAMPLES = [300, 1024, 2048, 4095, 4096]


def fnv(words, h=0xcbf29ce484222325):
    for w in words:
        h = ((h ^ w) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return h


def exact_triangles(n):
    s2 = [[1]]
    c1 = [[1]]
    for m in range(1, n + 1):
        ps, pc = s2[-1], c1[-1]
        s2.append([0] + [k * (ps[k] if k < m else 0) + ps[k - 1] for k in range(1, m + 1)])
        c1.append([0] + [(m - 1) * (pc[k] if k < m else 0) + pc[k - 1] for k in range(1, m + 1)])
    return s2, c1


def exact_bell(n, binom):
    b = [1]
    for m in range(n):
        b.append(sum(binom[m][k] * b[k] for k in range(m + 1)))
    return b


def exact_binom(n):
    t = [[1]]
    for m in range(1, n + 1):
        t.append([1] + [t[-1][k - 1] + t[-1][k] for k in range(1, m)] + [1])
    return t


def fact_tables(n):
    f = [1] * (n + 1)
    for i in range(1, n + 1):
        f[i] = f[i - 1] * i % P
    inv = [1] * (n + 1)
    inv[n] = pow(f[n], P - 2, P)
    for i in range(n, 0, -1):
        inv[i - 1] = inv[i] * i % P
    return f, inv


def s2_row_ie(n, invf):
    a = [pow(j, n, P) * invf[j] % P for j in range(n + 1)] if n > 0 else [1] + [0] * 0
    if n == 0:
        return [1]
    b = [(invf[i] if i % 2 == 0 else (P - invf[i]) % P) for i in range(n + 1)]
    return [sum(a[j] * b[k - j] for j in range(k + 1)) % P for k in range(n + 1)]


def polymul(a, b):
    r = [0] * (len(a) + len(b) - 1)
    for i, x in enumerate(a):
        if x:
            for j, y in enumerate(b):
                r[i + j] += x * y
    return [v % P for v in r]


def rising(lo, hi):
    """coefficients (low to high) of prod_{t=lo}^{hi-1} (x + t); empty product is [1]."""
    if hi - lo == 0:
        return [1]
    if hi - lo == 1:
        return [lo % P, 1]
    mid = (lo + hi) // 2
    return polymul(rising(lo, mid), rising(mid, hi))


def c1_row_poly(n):
    return rising(0, n)  # x (x+1) ... (x+n-1); n = 0 gives [1]


def bell_mod(n, f, invf):
    b = [1]
    for m in range(n):
        fm = f[m]
        b.append(sum(fm * invf[k] % P * invf[m - k] % P * b[k] for k in range(m + 1)) % P)
    return b


def aitken_extremes(n):
    """Counts the Aitken (Bell) triangle entries, rows 1..n, that are congruent to 0 or to p - 1 modulo p (used by the survivor arguments in
    docs/stirling.md: the branches `s >= kMod` of the C++ differ from `s > kMod` and `s >= kMod - 1` only on such an entry)."""
    row = [1]
    zeros = minus_one = total = 0
    for _ in range(n):
        new = [row[-1]]
        for x in row:
            new.append((new[-1] + x) % P)
        row = new
        zeros += row.count(0)
        minus_one += row.count(P - 1)
        total += len(row)
    return zeros, minus_one, total


def compute():
    s2, c1 = exact_triangles(EXACT_N)
    binom = exact_binom(EXACT_N)
    bell_ex = exact_bell(EXACT_N, binom)
    f, invf = fact_tables(MAXN)
    return s2, c1, binom, bell_ex, f, invf


def check_exact(s2, c1, binom, bell_ex):
    fa = 1
    for n in range(EXACT_N + 1):
        if n:
            fa *= n
        assert sum(c1[n]) == fa, ("c row sum", n)
        assert sum(s2[n]) == bell_ex[n], ("S2 row sum vs exact Bell", n)
        if n:
            assert s2[n][0] == 0 and c1[n][0] == 0 and s2[n][n] == 1 and c1[n][n] == 1
            assert s2[n][1] == 1 and c1[n][1] == fa // n
        if n >= 2:
            assert s2[n][2] == 2 ** (n - 1) - 1
            assert s2[n][n - 1] == n * (n - 1) // 2 and c1[n][n - 1] == n * (n - 1) // 2
    # exact orthogonality of the signed first-kind and second-kind matrices, both products, every n, m <= EXACT_N
    for n in range(EXACT_N + 1):
        for m in range(EXACT_N + 1):
            a = sum((-1) ** (n - k) * c1[n][k] * s2[k][m] for k in range(m, n + 1))
            b = sum(s2[n][k] * (-1) ** (k - m) * c1[k][m] for k in range(m, n + 1))
            assert a == (1 if n == m else 0) and b == (1 if n == m else 0), ("orthogonality", n, m)


def check_known():
    known_bell = [1, 1, 2, 5, 15, 52, 203, 877, 4140, 21147, 115975, 678570, 4213597, 27644437, 190899322, 1382958545 % P]
    f, invf = fact_tables(32)
    assert bell_mod(15, f, invf) == known_bell
    s2, c1 = exact_triangles(20)
    assert s2[10][5] == 42525 and c1[10][5] == 269325 and s2[7][3] == 301 and c1[6][3] == 225
    assert s2[10][5] == 42525 and c1[10][5] == 269325 and s2[7][3] == 301 and c1[6][3] == 225
    assert c1[20][19] == 190 and s2[20][2] == 2 ** 19 - 1


def rows_and_digests(f, invf):
    d = {}
    for n in ROW_SAMPLES:
        d[("s2", n)] = s2_row_ie(n, invf)
        d[("c1", n)] = c1_row_poly(n)
    return d


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "check"
    s2, c1, binom, bell_ex, f, invf = compute()
    check_exact(s2, c1, binom, bell_ex)
    check_known()
    # modular methods against the exact triangles for every row through 80, then sampled rows through EXACT_N
    for n in list(range(0, 81)) + [150, 255, 256, 257, 300]:
        assert s2_row_ie(n, invf) == [v % P for v in s2[n]], ("s2 ie", n)
        assert c1_row_poly(n) == [v % P for v in c1[n]], ("c1 poly", n)
    assert aitken_extremes(MAXN) == (0, 0, 8394752), "Aitken triangle entries equal to 0 or p-1 mod p"
    bm = bell_mod(MAXN, f, invf)
    assert bm[:EXACT_N + 1] == [v % P for v in bell_ex]
    big = rows_and_digests(f, invf)
    for n in ROW_SAMPLES:
        r1, r2 = big[("s2", n)], big[("c1", n)]
        assert len(r1) == n + 1 and len(r2) == n + 1
        assert sum(r1) % P == bm[n], ("S2 row sum vs modular Bell", n)
        assert sum(r2) % P == f[n], ("c row sum vs n!", n)
        assert r1[n] == 1 and r2[n] == 1 and r1[0] == 0 and r2[0] == 0
        assert r1[2] == (pow(2, n - 1, P) - 1) % P and r1[n - 1] == n * (n - 1) // 2 % P and r2[n - 1] == n * (n - 1) // 2 % P
        assert r2[1] == f[n - 1]
    if mode == "check":
        print("OK")
        return
    allS = fnv([v % P for n in range(EXACT_N + 1) for v in s2[n]])
    allC = fnv([v % P for n in range(EXACT_N + 1) for v in c1[n]])
    print("constexpr std::uint64_t kDigestAllS2 = 0x%016xull;  // rows 0..%d concatenated" % (allS, EXACT_N))
    print("constexpr std::uint64_t kDigestAllC1 = 0x%016xull;  // rows 0..%d concatenated" % (allC, EXACT_N))
    print("struct RowDigest { std::uint32_t n; std::uint64_t digest; };")
    for kind, name in (("s2", "kS2Digests"), ("c1", "kC1Digests")):
        print("constexpr RowDigest %s[] = {" % name)
        for n in ROW_SAMPLES:
            print("    {%du, 0x%016xull}," % (n, fnv(big[(kind, n)])))
        print("};")
    print("constexpr RowDigest kBellDigests[] = {  // digest of B(0) .. B(n)")
    for n in BELL_SAMPLES:
        print("    {%du, 0x%016xull}," % (n, fnv(bm[:n + 1])))
    print("};")


main()
