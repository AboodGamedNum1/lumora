#!/usr/bin/env python3
"""Independent reference for core/linebreak (scope AF). Exact Python integers, no fixed width anywhere.

It follows the rule text in linebreak.h literally: the open line is a list of item indexes, items are added one at a time, and a wrap
closes the line and re-examines the items that were left over. It shares no code with the implementation. Running it prints the
constants that tests/linebreak/linebreak_tests.cpp pastes: scenarios (items, width, capacity, expected lines) and seeded random
corpora. Usage: python3 tools/linebreak_ref.py > data.inc
"""
M64 = (1 << 64) - 1
GLYPH, SPACE, NEWLINE = 0, 1, 2
OK, NULLARG, BADWIDTH, TOOMANY, BADITEM, FULL = 0, 1, 2, 3, 4, 5
MAXITEMS = 1 << 16


def fnv_words(ws):
    h = 0xcbf29ce484222325
    for v in ws:
        for k in range(4):
            h ^= ((v & 0xFFFFFFFF) >> (8 * k)) & 0xFF
            h = (h * 0x100000001b3) & M64
    return h


def lines_of(items, width):
    """items: list of (advance, kind). Returns the list of line start indexes."""
    n = len(items)
    starts = []
    pending = 0            # index of the next item to examine
    while pending < n:
        line = []          # indexes of the items on the open line
        starts.append(pending)
        j = pending
        closed = False
        while j < n and not closed:
            adv, kind = items[j]
            if kind == SPACE:
                line.append(j)
                j += 1
            elif kind == NEWLINE:
                line.append(j)
                j += 1
                closed = True
                pending = j
            else:
                used = sum(items[i][0] for i in line)
                if used + adv <= width:
                    line.append(j)
                    j += 1
                else:
                    spaces = [i for i in line if items[i][1] == SPACE]
                    if spaces:
                        pending = spaces[-1] + 1
                    elif line:
                        pending = j
                    else:
                        pending = j + 1     # a lone glyph wider than the width
                    closed = True
        if not closed:
            pending = n
    return starts


def status_of(items, width, capacity):
    if width <= 0: return BADWIDTH, []
    if len(items) > MAXITEMS: return TOOMANY, []
    for adv, kind in items:
        if adv < 0 or kind > 2: return BADITEM, []
    st = lines_of(items, width)
    if len(st) > capacity: return FULL, []
    return OK, st


class Lcg:
    def __init__(s, seed): s.s = seed & M64
    def next(s):
        s.s = (s.s * 6364136223846793005 + 1442695040888963407) & M64
        return s.s >> 32
    def rng(s, lo, hi): return lo + s.next() % (hi - lo + 1)


def parse(text):
    """'G3 S1 N0' -> items."""
    out = []
    for tok in text.split():
        out.append((int(tok[1:]), {'G': GLYPH, 'S': SPACE, 'N': NEWLINE}[tok[0]]))
    return out


def sc(name, text, width, cap=100):
    items = parse(text)
    st, starts = status_of(items, width, cap)
    o = ['const Item kItems_%s[] = {%s};' % (name, ', '.join('{%d, %d}' % it for it in items) if items else '{0, 0}')]
    o.append('const u32 kStarts_%s[] = {%s};' % (name, ', '.join('%du' % v for v in starts) if starts else '0'))
    o.append('const Scn kScn_%s = {kItems_%s, %du, %d, %du, %d, kStarts_%s, %du};' % (name, name, len(items), width, cap, st, name, len(starts)))
    return o


def main():
    o = []
    S = [
        ('empty', '', 10),
        ('one_glyph', 'G5', 10),
        ('exact_fit', 'G4 G6', 10),
        ('one_over', 'G4 G7', 10),
        ('word_wrap', 'G3 G3 S1 G3 G3 S1 G3', 8),
        ('hang', 'G5 S4 G5', 8),
        ('hang_exact', 'G5 S1 G2 G1', 8),
        ('cut_word', 'G3 G3 G3 G3 G3', 7),
        ('wide_glyph', 'G20 G1', 5),
        ('wide_glyph_mid', 'G2 S1 G20 G1', 5),
        ('last_space', 'G2 S1 G2 S1 G2 G4', 8),
        ('spaces_only', 'S1 S1 S1 S1', 2),
        ('space_then_glyph', 'S3 S3 S3 G3', 7),
        ('leading_space_line', 'G5 S1 S1 G3 G3', 6),
        ('newline_mid', 'G2 N9 G2 G2 N0 G1', 100),
        ('newline_end', 'G2 N9', 100),
        ('newline_only', 'N5 N5 N5', 100),
        ('newline_after_space', 'G1 S1 N0 S1 G1', 100),
        ('zero_adv', 'G0 G0 S0 G0 N0 G0', 1),
        ('width_one', 'G1 G1 S1 G1 G2 G1', 1),
        ('recheck_after_wrap', 'G4 S1 G3 G3 S1 G3 G3 G3', 7),
        ('rewrap_cut', 'G2 S1 G5 G5 G5', 6),
        ('space_run_overflow', 'G1 S5 S5 S5 G1', 4),
        ('glyph_after_wide_line', 'G9 G1 S1 G1', 9),
        ('big_adv', 'G2147483647 G2147483647 G2147483647 S2147483647 G1', 2147483647),
        ('big_sum', 'G2147483647 S2147483647 G2147483647 S2147483647 G2147483647', 2147483647),
        ('cap_exact', 'G5 G5 G5', 5, 3),
        ('cap_short', 'G5 G5 G5', 5, 2),
        ('cap_zero_empty', '', 5, 0),
        ('cap_zero_one', 'G1', 5, 0),
        ('bad_width0', 'G1', 0),
        ('bad_width_neg', 'G1', -3),
        ('bad_width_min', '', -2147483648),
    ]
    for t in S: o += sc(*t)
    o.append('const Scn* const kScns[] = {%s};' % ', '.join('&kScn_%s' % t[0] for t in S))
    # random corpora: (seed, cases, max items, max advance, max width)
    for seed, cases, mi, ma, mw in ((0xF1, 400, 12, 5, 12), (0xF2, 300, 40, 9, 30), (0xF3, 150, 200, 20, 60), (0xF4, 120, 60, 1000, 3000),
                                    (0xF5, 200, 25, 3, 3)):
        r = Lcg(seed)
        ws = []
        for _ in range(cases):
            n = r.rng(0, mi)
            items = []
            for _ in range(n):
                k = r.rng(0, 99)
                kind = GLYPH if k < 62 else SPACE if k < 90 else NEWLINE
                adv = r.rng(0, ma)
                items.append((adv, kind))
            width = r.rng(1, mw)
            cap = r.rng(0, n + 1)
            st, starts = status_of(items, width, cap)
            ws += [st, len(starts)] + starts
        o.append('const CorpusRef kCorpus_%x = {%dull, %du, %du, %du, %du, %du, %du};' % (seed, fnv_words(ws), seed, cases, mi, ma, mw, len(ws)))
    print('\n'.join(o))


if __name__ == '__main__':
    main()
