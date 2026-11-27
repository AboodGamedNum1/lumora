#!/usr/bin/env python3
"""Reference for core/gauss. Prints data.inc (digests and the case table) for tests/gauss/gauss_tests.cpp.

Independent methods (none of them shares code with the library's Gauss-Jordan routine):
  1. Leibniz determinant and "largest nonzero minor" rank (also used by the test file for every small matrix); the solution kind comes from
     rank(A) against rank([A|b]) and the pivot columns from the rank of every prefix of columns.
  2. Bareiss fraction-free elimination over the integers (exact integer determinant, then reduced modulo p) for matrices up to 40 x 40.
  3. A Python Gauss-Jordan over the field (Python's own modular inverse), written separately from the C++ code, for everything that has no closed
     form; cross-checked against 1 and 2 where they apply.
  4. Matrices built so that the answer is known: A = P * L * E * U with L (rows x rows) and U (cols x cols) unit triangular, E a rectangular
     diagonal with chosen zeros and P a row permutation. Then det A = sign(P) * product of E (square, full rank), rank A = number of nonzero E,
     the pivot columns are exactly the columns j with E[j] nonzero (column j of E U is supported on the earlier pivot rows when E[j] is zero, so it
     is in the span of the earlier pivot columns), so the canonical solution of A x = A x0 is the vector x0 chosen to be zero off those columns. An
     inconsistent right-hand side is A x0 + L e_t for a row t with E[t] zero or t >= min(rows, cols).
Every method is cross-checked while the data is generated; a disagreement aborts. `python3 tools/gauss_ref.py check` checks without printing
(it takes a few minutes, mostly the 256 x 256 cases in pure Python).

Digest of a case: FNV-1a over rank(A) (4 bytes little endian), then det(A) (4 bytes) when the matrix is square, then the solve result: the kind (1
byte, 0 Unique, 1 Many, 2 Inconsistent), the rank (4 bytes) and, unless Inconsistent, x[0 .. cols) (4 bytes each).
Inputs are generated from the case fields by a 64-bit LCG (s = s * 6364136223846793005 + 1442695040888963407, value = (s >> 33) % k), the same one
as in the test file. Type 0 (random): mode 0 uniform residues; mode 1 one of {0, 1, 2, p-2, p-1}; mode 2 sparse ((v % 5 == 0) ? next % p : 0);
mode 3 low rank (k = 1 + rnd(min(rows, cols)), X rows x k, Y k x cols, A = X Y). Then b: bmode 0 = A x0 with random x0, bmode 1 random, bmode 2 zero.
"""
import sys

P = 998244353
FNV_OFF = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
M64 = 0xFFFFFFFFFFFFFFFF
UNIQUE, MANY, INCONSISTENT = 0, 1, 2


def fold(h, v, nbytes):
    for _ in range(nbytes):
        h = ((h ^ (v & 0xFF)) * FNV_PRIME) & M64
        v >>= 8
    return h


class Lcg:
    def __init__(self, s):
        self.s = s

    def rnd(self, k):
        self.s = (self.s * 6364136223846793005 + 1442695040888963407) & M64
        return (self.s >> 33) % k


# ---------------------------------------------------------------- method 1: Leibniz and minors
def det_leibniz(m):
    n = len(m)
    if n == 0:
        return 1
    total = 0
    from itertools import permutations
    for perm in permutations(range(n)):
        sign = 1
        seen = [False] * n
        for i in range(n):
            if not seen[i]:
                j = i
                ln = 0
                while not seen[j]:
                    seen[j] = True
                    j = perm[j]
                    ln += 1
                if ln % 2 == 0:
                    sign = -sign
        prod = 1
        for i in range(n):
            prod = prod * m[i][perm[i]] % P
        total += sign * prod
    return total % P


def rank_minors(a, rows, cols):
    from itertools import combinations
    for k in range(min(rows, cols), 0, -1):
        for rs in combinations(range(rows), k):
            for cs in combinations(range(cols), k):
                if det_leibniz([[a[i][j] for j in cs] for i in rs]) != 0:
                    return k
    return 0


# ---------------------------------------------------------------- method 2: Bareiss
def det_bareiss(m):
    n = len(m)
    if n == 0:
        return 1
    a = [list(r) for r in m]
    sign = 1
    prev = 1
    for k in range(n - 1):
        if a[k][k] == 0:
            sw = None
            for i in range(k + 1, n):
                if a[i][k] != 0:
                    sw = i
                    break
            if sw is None:
                return 0
            a[k], a[sw] = a[sw], a[k]
            sign = -sign
        for i in range(k + 1, n):
            for j in range(k + 1, n):
                a[i][j] = (a[i][j] * a[k][k] - a[i][k] * a[k][j]) // prev
        prev = a[k][k]
    return sign * a[n - 1][n - 1] % P


# ---------------------------------------------------------------- method 3: Python Gauss-Jordan
def py_gauss(a, b, rows, cols):
    """Returns (rank, det or None, kind, x or None). det is the determinant when rows == cols."""
    w = [list(a[i]) + [b[i]] for i in range(rows)]
    rank = 0
    det = 1
    for c in range(cols):
        if rank >= rows:
            break
        p = None
        for i in range(rank, rows):
            if w[i][c] % P != 0:
                p = i
                break
        if p is None:
            continue
        if p != rank:
            w[p], w[rank] = w[rank], w[p]
            det = -det
        piv = w[rank][c]
        det = det * piv % P
        inv = pow(piv, -1, P)
        prow = [v * inv % P for v in w[rank]]
        w[rank] = prow
        for i in range(rows):
            if i != rank:
                f = w[i][c]
                if f:
                    r = w[i]
                    w[i] = [(x - f * y) % P for x, y in zip(r, prow)]
        rank += 1
    det = det % P if (rows == cols and rank == rows) else (0 if rows == cols else None)
    for i in range(rank, rows):
        if w[i][cols] % P != 0:
            return rank, det, INCONSISTENT, None
    x = [0] * cols
    for k in range(rank):
        c = next(j for j in range(cols) if w[k][j] != 0)
        x[c] = w[k][cols]
    return rank, det, (UNIQUE if rank == cols else MANY), x


def check_solution(a, b, rows, cols, x):
    for i in range(rows):
        if sum(a[i][j] * x[j] for j in range(cols)) % P != b[i] % P:
            return False
    return True


def prefix_pivots(a, rows, cols):
    """Pivot columns from method 1 ranks of prefixes of columns (small matrices only)."""
    piv = []
    prev = 0
    for j in range(cols):
        r = rank_minors([row[:j + 1] for row in a], rows, j + 1)
        if r > prev:
            piv.append(j)
        prev = r
    return piv


def verify_small(a, b, rows, cols, res):
    """Cross-check py_gauss against Leibniz/minors for small matrices."""
    rank, det, kind, x = res
    assert rank == rank_minors(a, rows, cols), "rank vs minors"
    if rows == cols:
        assert det == det_leibniz(a), "det vs leibniz"
    ab = [list(a[i]) + [b[i]] for i in range(rows)]
    rab = rank_minors(ab, rows, cols + 1)
    if rab > rank:
        assert kind == INCONSISTENT
    else:
        assert kind == (UNIQUE if rank == cols else MANY)
        assert check_solution(a, b, rows, cols, x)
        piv = prefix_pivots(a, rows, cols)
        assert len(piv) == rank
        assert all(x[j] == 0 for j in range(cols) if j not in piv), "free variables"


def case_digest(rank, det, kind, srank, x, square):
    h = fold(FNV_OFF, rank, 4)
    if square:
        h = fold(h, det, 4)
    h = fold(h, kind, 1)
    h = fold(h, srank, 4)
    if kind != INCONSISTENT:
        for v in x:
            h = fold(h, v, 4)
    return h


# ---------------------------------------------------------------- generators
def gen_random(rows, cols, mode, bmode, seed):
    g = Lcg(seed)
    if mode == 0:
        a = [[g.rnd(P) for _ in range(cols)] for _ in range(rows)]
    elif mode == 1:
        tab = (0, 1, 2, P - 2, P - 1)
        a = [[tab[g.rnd(5)] for _ in range(cols)] for _ in range(rows)]
    elif mode == 2:
        a = []
        for _ in range(rows):
            row = []
            for _ in range(cols):
                row.append(g.rnd(P) if g.rnd(5) == 0 else 0)
            a.append(row)
    else:
        k = 1 + g.rnd(min(rows, cols))
        xm = [[g.rnd(P) for _ in range(k)] for _ in range(rows)]
        ym = [[g.rnd(P) for _ in range(cols)] for _ in range(k)]
        a = [[sum(xm[i][t] * ym[t][j] for t in range(k)) % P for j in range(cols)] for i in range(rows)]
    if bmode == 0:
        x0 = [g.rnd(P) for _ in range(cols)]
        b = [sum(a[i][j] * x0[j] for j in range(cols)) % P for i in range(rows)]
    elif bmode == 1:
        b = [g.rnd(P) for _ in range(rows)]
    else:
        b = [0] * rows
    return a, b


def perm_sign(perm):
    seen = [False] * len(perm)
    sign = 1
    for i in range(len(perm)):
        if not seen[i]:
            j = i
            ln = 0
            while not seen[j]:
                seen[j] = True
                j = perm[j]
                ln += 1
            if ln % 2 == 0:
                sign = -sign
    return sign


def gen_constructed(rows, cols, zero_den, bmode, perm_flag, seed):
    """Returns a, b and the expected (rank, det, kind, x)."""
    g = Lcg(seed)
    mn = min(rows, cols)
    e = []
    for _ in range(mn):
        if zero_den > 0 and g.rnd(zero_den) == 0:
            e.append(0)
        else:
            e.append(1 + g.rnd(P - 1))
    L = [[0] * rows for _ in range(rows)]
    for i in range(rows):
        for k in range(i):
            L[i][k] = g.rnd(P)
        L[i][i] = 1
    U = [[0] * cols for _ in range(cols)]
    for k in range(cols):
        U[k][k] = 1
        for j in range(k + 1, cols):
            U[k][j] = g.rnd(P)
    T = [[0] * cols for _ in range(rows)]
    for k in range(mn):
        T[k] = [e[k] * U[k][j] % P for j in range(cols)]
    a = []
    for i in range(rows):
        acc = [0] * cols
        for k in range(min(i, mn - 1) + 1):
            f = L[i][k]
            if f and e[k]:
                tk = T[k]
                acc = [s + f * t for s, t in zip(acc, tk)]
        a.append([s % P for s in acc])
    pivots = [j for j in range(mn) if e[j] != 0]
    x0 = []
    for j in range(cols):
        v = g.rnd(P)
        x0.append(v if (j < mn and e[j] != 0) else 0)
    b = [sum(a[i][j] * x0[j] for j in range(cols) if x0[j]) % P for i in range(rows)]
    consistent = True
    if bmode == 1:
        t = next(t for t in range(rows) if not (t < mn and e[t] != 0))
        for i in range(t, rows):
            b[i] = (b[i] + L[i][t]) % P
        consistent = False
    perm = list(range(rows))
    sign = 1
    if perm_flag:
        for i in range(rows - 1, 0, -1):
            j = g.rnd(i + 1)
            perm[i], perm[j] = perm[j], perm[i]
        a = [a[perm[i]] for i in range(rows)]
        b = [b[perm[i]] for i in range(rows)]
        sign = perm_sign(perm)
    rank = len(pivots)
    det = None
    if rows == cols:
        if rank == rows:
            prod = 1
            for v in e:
                prod = prod * v % P
            det = (sign * prod) % P
        else:
            det = 0
    if not consistent:
        kind = INCONSISTENT
        x = None
    else:
        kind = UNIQUE if rank == cols else MANY
        x = x0
    return a, b, (rank, det, kind, x)


# ---------------------------------------------------------------- case table
def constructed_rank(rows, cols, zero_den, seed):
    """The rank of a constructed matrix: the number of nonzero E entries, which are the first draws of the generator."""
    g = Lcg(seed)
    r = 0
    for _ in range(min(rows, cols)):
        if zero_den > 0 and g.rnd(zero_den) == 0:
            continue
        g.rnd(P - 1)
        r += 1
    return r


def cases():
    """(rows, cols, type, mode, bmode, perm, seed). Type 0 random (mode, bmode); type 1 constructed (mode = zero_den)."""
    t = []
    seed = 9000
    for rows in range(1, 7):
        for cols in range(1, 7):
            for mode in range(4):
                for bmode in range(3):
                    t.append((rows, cols, 0, mode, bmode, 0, seed))
                    seed += 1
    shapes = [(8, 8), (10, 7), (7, 10), (12, 12), (16, 16), (20, 5), (5, 20), (33, 33), (40, 17), (17, 40), (64, 64), (100, 100)]
    for (rows, cols) in shapes:
        for mode in range(4):
            for bmode in range(3):
                t.append((rows, cols, 0, mode, bmode, 0, seed))
                seed += 1
    for (rows, cols, mode, bmode) in ((256, 256, 0, 0), (256, 256, 3, 0), (256, 256, 1, 0), (256, 255, 0, 0), (255, 256, 2, 0), (256, 1, 0, 1), (1, 256, 0, 0),
                                      (256, 128, 3, 0), (128, 256, 0, 0)):
        t.append((rows, cols, 0, mode, bmode, 0, seed))
        seed += 1
    cs = [(3, 3), (4, 4), (5, 5), (6, 6), (8, 8), (5, 3), (3, 5), (7, 4), (4, 7), (10, 10), (16, 12), (12, 16), (32, 32), (1, 1), (1, 4), (4, 1)]
    for (rows, cols) in cs:
        for zd in (0, 2, 3):
            for perm in (0, 1):
                for bmode in (0, 1):
                    if bmode == 1 and constructed_rank(rows, cols, zd, seed) == rows:
                        continue
                    t.append((rows, cols, 1, zd, bmode, perm, seed))
                    seed += 1
    for (rows, cols, zd, bmode) in ((256, 256, 0, 0), (256, 256, 4, 0), (256, 256, 4, 1), (255, 256, 0, 0), (256, 255, 0, 0), (256, 255, 0, 1), (256, 128, 0, 0),
                                    (128, 256, 0, 0), (256, 256, 2, 0), (256, 1, 0, 0), (1, 256, 0, 0), (256, 200, 3, 0)):
        assert bmode == 0 or constructed_rank(rows, cols, zd, seed) < rows
        t.append((rows, cols, 1, zd, bmode, 1, seed))
        seed += 1
    return t


def run_case(c):
    rows, cols, typ, mode, bmode, perm, seed = c
    if typ == 0:
        a, b = gen_random(rows, cols, mode, bmode, seed)
        res = py_gauss(a, b, rows, cols)
        rank, det, kind, x = res
        if rows <= 4 and cols <= 4:
            verify_small(a, b, rows, cols, res)
        if rows == cols and rows <= 40:
            assert det == det_bareiss(a), "det vs bareiss"
        if kind != INCONSISTENT:
            assert check_solution(a, b, rows, cols, x)
        if rows == cols:
            assert (det != 0) == (rank == rows)
        return case_digest(rank, det, kind, rank, x, rows == cols)
    a, b, exp = gen_constructed(rows, cols, mode, bmode, perm, seed)
    rank, det, kind, x = exp
    res = py_gauss(a, b, rows, cols)
    assert res[0] == rank and res[2] == kind, "constructed rank/kind"
    if rows == cols:
        assert res[1] == det, "constructed det"
    if kind != INCONSISTENT:
        assert res[3] == x, "constructed x"
    if rows == cols and rows <= 40:
        assert det == det_bareiss(a)
    return case_digest(rank, det, kind, rank, x, rows == cols)


# ---------------------------------------------------------------- exhaustive groups
def bits_matrix(idx, rows, cols):
    return [[(idx >> (i * cols + j)) & 1 for j in range(cols)] for i in range(rows)]


def solve_fold(h, a, b, rows, cols, check_small=False):
    res = py_gauss(a, b, rows, cols)
    if check_small:
        verify_small(a, b, rows, cols, res)
    rank, det, kind, x = res
    h = fold(h, kind, 1)
    h = fold(h, rank, 4)
    if kind != INCONSISTENT:
        for v in x:
            h = fold(h, v, 4)
    return h


def grp_binary_square():
    h = FNV_OFF
    for n in range(1, 5):
        for idx in range(1 << (n * n)):
            a = bits_matrix(idx, n, n)
            rank, det, _, _ = py_gauss(a, [0] * n, n, n)
            if idx % 16 == 0:
                assert det == det_bareiss(a)
                if n <= 3:
                    assert det == det_leibniz(a) and rank == rank_minors(a, n, n)
            h = fold(h, det, 4)
            h = fold(h, rank, 4)
    return h


def grp_binary_rank():
    h = FNV_OFF
    for rows in range(1, 5):
        for cols in range(1, 5):
            for idx in range(1 << (rows * cols)):
                a = bits_matrix(idx, rows, cols)
                rank = py_gauss(a, [0] * rows, rows, cols)[0]
                if idx % 64 == 0:
                    assert rank == rank_minors(a, rows, cols)
                h = fold(h, rank, 4)
    return h


def grp_binary_systems():
    h = FNV_OFF
    for rows in range(1, 4):
        for cols in range(1, 4):
            for idx in range(1 << (rows * cols + rows)):
                a = bits_matrix(idx, rows, cols)
                b = [(idx >> (rows * cols + i)) & 1 for i in range(rows)]
                h = solve_fold(h, a, b, rows, cols, check_small=(idx % 8 == 0))
    return h


V4 = (0, 1, 2, P - 1)


def grp_grid_2x2():
    h = FNV_OFF
    for idx in range(4 ** 4):
        a = [[V4[(idx >> (2 * (i * 2 + j))) & 3] for j in range(2)] for i in range(2)]
        rank, det, _, _ = py_gauss(a, [0, 0], 2, 2)
        assert det == det_leibniz(a) and rank == rank_minors(a, 2, 2)
        h = fold(h, det, 4)
        h = fold(h, rank, 4)
    return h


def grp_grid_2x3_systems():
    h = FNV_OFF
    for idx in range(4 ** 8):
        a = [[V4[(idx >> (2 * (i * 3 + j))) & 3] for j in range(3)] for i in range(2)]
        b = [V4[(idx >> (2 * (6 + i))) & 3] for i in range(2)]
        h = solve_fold(h, a, b, 2, 3, check_small=(idx % 4 == 0))
    return h


def grp_ternary_3x3():
    """All 3x3 matrices over {0, 1, p - 1} (not {0, 1, 2})."""
    h = FNV_OFF
    V = (0, 1, P - 1)
    for idx in range(3 ** 9):
        a = [[V[(idx // 3 ** (i * 3 + j)) % 3] for j in range(3)] for i in range(3)]
        rank, det, _, _ = py_gauss(a, [0, 0, 0], 3, 3)
        if idx % 7 == 0:
            assert det == det_leibniz(a) and rank == rank_minors(a, 3, 3)
        h = fold(h, det, 4)
        h = fold(h, rank, 4)
    return h


def grp_ternary_shapes():
    """Every matrix over {0, 1, 2} for every shape rows, cols from 1 to 3: rank, and det for square shapes."""
    h = FNV_OFF
    for rows in range(1, 4):
        for cols in range(1, 4):
            for idx in range(3 ** (rows * cols)):
                a = [[(idx // 3 ** (i * cols + j)) % 3 for j in range(cols)] for i in range(rows)]
                rank, det, _, _ = py_gauss(a, [0] * rows, rows, cols)
                if idx % 5 == 0:
                    assert rank == rank_minors(a, rows, cols)
                    if rows == cols:
                        assert det == det_leibniz(a)
                if rows == cols:
                    h = fold(h, det, 4)
                h = fold(h, rank, 4)
    return h


def grp_ternary_systems():
    """Every system (matrix over {0, 1, 2}, right side over {0, 1, 2}) with rows 1 to 2 and cols 1 to 3."""
    h = FNV_OFF
    for rows in range(1, 3):
        for cols in range(1, 4):
            for idx in range(3 ** (rows * cols + rows)):
                a = [[(idx // 3 ** (i * cols + j)) % 3 for j in range(cols)] for i in range(rows)]
                b = [(idx // 3 ** (rows * cols + i)) % 3 for i in range(rows)]
                h = solve_fold(h, a, b, rows, cols, check_small=(idx % 8 == 0))
    return h


def main():
    check = len(sys.argv) > 1 and sys.argv[1] == 'check'
    groups = [("kBinarySquare", grp_binary_square), ("kBinaryRank", grp_binary_rank), ("kBinarySystems", grp_binary_systems),
              ("kGrid2x2", grp_grid_2x2), ("kGrid2x3", grp_grid_2x3_systems), ("kTernary3x3", grp_ternary_3x3),
              ("kTernaryShapes", grp_ternary_shapes), ("kTernarySystems", grp_ternary_systems)]
    gd = [(n, f()) for n, f in groups]
    rows = []
    for c in cases():
        rows.append(c + (run_case(c),))
    if check:
        print("ok", len(rows))
        return
    print("// Generated by tools/gauss_ref.py. Do not edit.")
    print("struct Case {")
    print("  u32 rows;")
    print("  u32 cols;")
    print("  u32 type;")
    print("  u32 mode;")
    print("  u32 bmode;")
    print("  u32 perm;")
    print("  u64 seed;")
    print("  u64 digest;")
    print("};")
    for n, d in gd:
        print("const u64 %s = 0x%016Xull;" % (n, d))
    print("const Case kCases[] = {")
    for r in rows:
        print("    {%du, %du, %du, %du, %du, %du, %dull, 0x%016Xull}," % r)
    print("};")


if __name__ == '__main__':
    main()
