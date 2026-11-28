# calkinwilf: Stern's diatomic sequence and the Calkin-Wilf tree

Code: `core/calkinwilf/calkinwilf.cpp` (contract in `core/calkinwilf/calkinwilf.h`). Tests: `tests/calkinwilf/calkinwilf_tests.cpp`. Reference: `tools/calkinwilf_ref.py`. Evidence label: unit tests only.

## Rule

- `fusc(n)` gives s(n), Stern's diatomic sequence: s(0) = 0, s(1) = 1, s(2k) = s(k), s(2k + 1) = s(k) + s(k + 1). Any u64 n is valid, including 0.
- `calkin_wilf(n)` gives the fraction of node n of the Calkin-Wilf tree, s(n) / s(n + 1), in lowest terms, for 1 <= n <= 2^64 - 2. The root is node 1 = 1/1; node 2n is the left child p / (p + q) and node 2n + 1 the right child (p + q) / q of the node n = p / q.
- `calkin_wilf_index(p, q)` gives the node n of the fraction p / q (the inverse), as a u64.
- Check order: a null output pointer is NullArgument (for `calkin_wilf`, either pointer); then n = 0 (or p = 0 or q = 0) is BadArgument; then, for the index only, gcd(p, q) above 1 is NotCoprime; then n above 2^64 - 2 (`calkin_wilf`) or an index of 2^64 or more (`calkin_wilf_index`) is TooLarge. Nothing is written on a failure.
- Integer only: no floating point, no 128-bit type, no builtin, no heap, no recursion, no mutable static, no array. fusc takes at most 64 steps; the index takes one gcd and a number of quotient steps that is at most 64.

## Method and proof

**fusc by a binary loop.** Start with a = 1, b = 0. For the bits of n from the lowest: a bit 1 does b += a, a bit 0 does a += b. The result is b. Invariant: after the low j bits are used, with h = n >> j (the part left), s(n) = a * s(h) + b * s(h + 1). At j = 0 it holds (a = 1, b = 0, h = n). If h = 2g (bit 0): s(h) = s(g) and s(h + 1) = s(g) + s(g + 1), so a * s(h) + b * s(h + 1) = (a + b) * s(g) + b * s(g + 1), which is a += b. If h = 2g + 1 (bit 1): s(h) = s(g) + s(g + 1) and s(h + 1) = s(g + 1), giving a * s(g) + (a + b) * s(g + 1), which is b += a. When h = 0 the value is a * s(0) + b * s(1) = b.

**Node n is s(n) / s(n + 1), in lowest terms.** s(2m) / s(2m + 1) = s(m) / (s(m) + s(m + 1)) and s(2m + 1) / s(2m + 2) = (s(m) + s(m + 1)) / s(m + 1) are the left and right children of s(m) / s(m + 1), so the formula follows from the root (s(1), s(2)) = (1, 1) by induction on n. The two child pairs come from (s(m), s(m + 1)) by adding one entry to the other, which keeps the gcd, so gcd(s(n), s(n + 1)) = 1 for every n >= 1.

**The index is the path.** Every coprime positive p / q has exactly one parent: if p > q it is the right child of (p - q) / q; if p < q it is the left child of p / (q - p); p = q = 1 is the root. Walking up until 1/1 ends because p + q falls at every step. With S the number of steps and the path bits read from the root (a right step is 1, a left step is 0), the index is 2^S + (the S bits as a binary number); the leaf-most step is the lowest bit, which is the first step of the walk up. This is the number of subtractions of Euclid's algorithm in its subtraction form (the last run stops at 1/1, not at 0).

**Runs.** With x > y the walk subtracts y from x while x > y: when y = 1 that is x - 1 steps (it stops at 1); when y > 1 (coprime, so y does not divide x) it is floor(x / y) steps and x falls below y. The same holds with the roles swapped. A run of t right steps adds t one-bits, a run of t left steps t zero-bits. So the code walks by runs, not by single subtractions.

**Fit.** The index 2^S + bits is below 2^(S + 1), so it fits a u64 exactly when S <= 63. The code returns TooLarge when a run is longer than 63 - steps, before any shift, so every shift is by at most 63. 64/1 has 63 steps and the index 2^64 - 1 (it is Ok for `calkin_wilf_index`; `calkin_wilf` rejects n = 2^64 - 1 as TooLarge because n + 1 does not fit). 65/1 and 1/65 have 64 steps: TooLarge. Also p/1 and 1/p for p = 63, 64, 65 and the u32 maximum answer in one quotient step, not 4 billion (a single-subtraction loop is a cost mutant below, not a timing assertion).

**u64 bound of fusc and the Fibonacci maximum.** Let F(1) = F(2) = 1. Claim: for a node n with d + 1 bits (2^d <= n < 2^(d + 1)), a = s(n) and b = s(n + 1) satisfy a, b <= F(d + 2) and a + b <= F(d + 3). Induction on d: the root has (1, 1), F(2) = 1 and 2 = F(3). A child has the pair (a, a + b) or (a + b, b): both entries are at most a + b <= F(d + 3) = F((d + 1) + 2), and the new sum is a + 2b or 2a + b, at most (a + b) + max(a, b) <= F(d + 3) + F(d + 2) = F(d + 4). Hence s(n) <= F(k + 1) for every n below 2^k, and for n below 2^64 it is at most F(65) = 17167680177565, which is below 2^45. The loop state (a, b) grows the same way (each step adds one entry to the other from (1, 0)), so after j steps both are at most F(j + 1) <= F(65): no u64 overflow. The bound is reached: n_k = (2^(k + 1) + (-1)^k) / 3 has k bits and s(n_k) = F(k + 1); n_64 = 12297829382473034411 gives F(65). The tests check s(n_k) = F(k + 1) for every k = 1..64 against Fibonacci numbers built by addition (the values were also verified by brute force to k = 22 and the Python reference to k = 64).

## Tests

Expected values come from the header's examples, from an in-test array of s(n) built by the recurrence for n up to 2^22, from the definition walk in the test (one subtraction at a time for p, q up to 1024), from closed forms (s(2^k) = 1, s(2^k - 1) = k, s(2^k + 1) = k + 1), from the maximisers, from Fibonacci ratios, and from FNV-1a 64-bit digests made by `tools/calkinwilf_ref.py` (Python standard library only, a different method: memoised recursion and an actual walk up the tree with Python integers). Every call passes its result cells between guard words.

Extents and local runtimes (gcc 11.4.0, `-O1`, ASan + UBSan, one core, base 57b65e2; the whole binary has 22 cases and 109 assertions and runs about 2 s; the longest case is 0.40 s; none asserts time; declaration order):
- `header examples`, `failure cases`, `closed forms`, `maximisers` (k = 1..64): each under 0.01 s. The failure case checks the order null, zero, NotCoprime, TooLarge and that nothing is written.
- `chunk 0` to `chunk 3`: exhaustive, every n = 1..2^22 (chunk 0 from 1; chunk 3 holds 2^22): fusc, `calkin_wilf`, lowest terms, the index round trip, s(n) <= F(bits + 1), digest. 0.37 to 0.40 s each.
- `pairs 0` and `pairs 1`: exhaustive, every p, q in 0..1024 (p <= 512 and p >= 513), return codes and index against the one-subtraction walk and the digest. 0.09 and 0.10 s.
- `sampled nodes 0` to `3`: sampled, 4 x 50000 nodes over the whole u64 range from a splitmix64 stream (digest and round trip); 0.04 s each. `sampled fractions 0` and `1`: sampled, 2 x 50000 raw u32 pairs p, q in 1..2^32 - 1 (return code, index and round trip against the digest); 0.014 s each. Of the 100000 raw pairs 39060 are not coprime (19519 and 19541 per shard; counted by the reference) and answer NotCoprime, including the ones whose index would not fit.
- `non-coprime pairs: 1000 sampled`: 1000 pairs g * a, g * b with 2 <= g <= 65536 and a, b random, so the gcd is at least 2 (digest; all NotCoprime, nothing written). With the 39060 above, 40060 non-coprime pairs are tested. Under 0.01 s.
- `deep pairs: 20000 sampled nodes of depth 40..63 and their children`: fixed splitmix64 stream; a node n = 2^d + r (d in 40..63, r random below 2^d) is taken when s(n) and s(n + 1) both fit a u32, until 20000 nodes are taken (30619 tries). Each node is checked for calkin_wilf (lowest terms, equals the digest) and the index of its fraction must give n back. Every child of it that fits a u32 (the left child p / (p + q) and the right child (p + q) / q, 37628 in all) is checked: Ok with index 2n and 2n + 1 when the child has depth 63 or less, TooLarge for depth 64 (22 children), and the digest of all rows. Nodes by depth 40..63: 1308, 1289, 1195, 1236, 1249, 1315, 1268, 1284, 1298, 1276, 1274, 1210, 1152, 1013, 816, 608, 450, 301, 187, 130, 67, 33, 22, 19. Children by depth 41..64: 2616, 2578, 2390, 2472, 2498, 2630, 2536, 2568, 2596, 2538, 2488, 2272, 2022, 1702, 1256, 864, 634, 398, 240, 160, 84, 40, 24, 22. 0.03 s.
- `depth boundary`: 1594 fractions made from the root by run-length shapes (long single runs, many short runs, both orientations), 520 at depth 62, 532 at depth 63 (Ok, exact index) and 542 at depth 64 (TooLarge). Under 0.01 s.
- `boundary`: k / 1 and 1 / k for k = 1..70 and the u32 maximum. Under 0.01 s.
- `edges`: the nodes 2^k - 1, 2^k, 2^k + 1 for every k and 2^64 - 3 .. 2^64 - 1 (191 values) against a digest. Under 0.01 s.
- `Fibonacci ratios`: F(i + 1) / F(i) and F(i) / F(i + 1) for every pair that fits a u32 (46 rows), exact index. Under 0.01 s.

Reference commands (all run): `check` (the identities; about 1.5 s), `emit` (all digests and tables as C++; about 22 s), and `chunk K`, `pairs K`, `nodes K`, `fracs K`, `edges` as pieces of it. The measured times vary by about a factor of two between runs.

## Mutation testing

120 source mutants of `core/calkinwilf/calkinwilf.cpp` (script outside the repository), built with ASan + UBSan against the final tests (md5 9c2a7249ac79fca0fae17e85e74e3041, not edited after the run; the whole set was run once on this file in declaration order), declaration order (`--order decl`), the whole suite for every mutant. Mutated: the fusc loop (bit test, both updates, initial values, shift, return value, a fixed 64-trip loop, widths), the gcd, the null, zero, NotCoprime and TooLarge checks, their order and return codes, the outputs of `calkin_wilf` (n + 1, n - 1, swap), the index loop test, the direction test, both run formulas with and without the y = 1 and x = 1 cases, the `63 - steps` boundary (62 and 64, with and without steps), the bit shifts and the combination, the final index and the types. One further mutant did not apply (its pattern matched twice) and is not counted.

- 90 killed by a failed assertion, and 12 killed by an abnormal end (a sanitizer abort or a crash: for example the null check removed, or a shift by 64).
- 2 non-terminating, not counted as kills: `gc_div` (t = a / b: gcd(1, 1) leaves a = b = 1 for ever) and `ix_right_lt` (the direction test x < y: for 2/1 the left run is y / x = 0, so x and y never change). Probed on the final file: each mutant was rebuilt with a check that aborts when the loop state at the top of the loop (a, b; or x, y, steps, bits) equals the state at the top of the previous trip, which proves non-termination because the body depends only on that state; both aborted (SIGABRT, no sanitizer report) in the first test case that reaches them.
- 13 survivors, each equivalent:
  - `fv_fixed64` (always 64 loop trips): after n is 0 every further bit is 0 and does only a += b, which leaves b, the result, unchanged.
  - `cw_swap2` (the zero check and the TooLarge check swapped): n = 0 and n > 2^64 - 2 never both hold.
  - `ix_right_ge` (x >= y): equality with coprime x, y means x = y = 1, where the loop has ended.
  - `ix_runr_le`, `ix_runl_le` (y <= 1 and x <= 1 instead of == 1): x and y are never 0 inside the loop.
  - `ix_bits_xor`, `ix_bits_add`: the bits written are above position `steps` and the old bits are below it, so or, xor and add agree.
  - `ix_out_xor`, `ix_out_add`: bits < 2^steps, so the leading 2^steps combines the same way.
  - `ix_x32`, `ix_y32`, `ix_steps32`, `ix_run32`: x <= p and y <= q fit a u32, steps <= 63 and run <= 2^32 - 1 fit too (run * y <= x).
- 3 cost mutants (the values are right, only the time grows; kept apart from kills and equivalent survivors; no test can detect them because the repository forbids timing assertions). Observed in the ASan + UBSan build, whole suite: `ix_cm_single_r` (the right run counted by single subtractions) 4.8 s in `boundary` against under 0.01 s for the source (whole suite 7 s); `ix_cm_single_l` (the left run) 3.3 s in `boundary` and 1.6 s in `header examples` (whole suite 7 s); `gc_sub` (gcd by subtraction) 8.1 s in `boundary` and 1.3 s in `header examples` (whole suite 12 s, against 2 s). Under six parallel mutants `gc_sub` ran past the 40 s limit once, so it is not counted as a kill.

## Not run

Other builds than the local gcc 11.4 ASan + UBSan, MSVC, Clang, Windows, Android, any timing assertion, `calkin_wilf` at every u64 (above 2^22 only the structured sets, the 200000 sampled nodes and the generated pairs are compared), and a mutant run on shuffled case order. The base for local evidence is 57b65e2. Nothing here is a CI result.
