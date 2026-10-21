#!/usr/bin/env python3
"""CAS 021's boundary (make check; work/021-series.md rule 1). The series route runs only where taylor's derivatives or
limit's walk refuse, so with it compiled out (build/golden_check-ser0, CASIM_SER=0) the golden corpus must be exactly
what it was before the unit: every line byte for byte, and each line tests/golden/moved/cas-021.txt lists with its
before. The same for →DEC's file (tests/golden/decimal.txt, CASIM_PRINT_DECIMAL OR'd into the angle), whose moves are
the list's second section; its third lists the unit's new inputs the route answers. Each listed line must have moved
from NO_ROOM or UNDECIDED (or a DECIMAL_FORM whose exact run was NO_ROOM), and its after must be the file's. The
check's control: the corpus with one moved line left at its after must fail. A later unit's moves in today's routes
(CAS 022's, moved/cas-022.txt, checked by scripts/signs-golden.py) stand at their after here, as the files have them.
    scripts/series-golden.py"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN = os.path.join(ROOT, "tests", "golden")
MOVED = os.path.join(GOLDEN, "moved", "cas-021.txt")
CHECK = os.path.join(ROOT, "build", "golden_check-ser0")
FROM = ("NO_ROOM", "UNDECIDED", "DECIMAL_FORM")
PRINT_DECIMAL = 16      # casim.h's CASIM_PRINT_DECIMAL


def moved():
    """the list's sections (the corpus's moves, →DEC's, the new inputs): inputs (the first ten fields) -> (before,
    after), each [status, text]"""
    sections, lines = [{}], []
    for ln in open(MOVED, encoding="utf-8").read().split("\n"):
        if ln.startswith("# Under →DEC") or ln.startswith("# New inputs"):
            sections.append({})
        elif ln and not ln.startswith("#"):
            lines.append((len(sections) - 1, ln))
    for i in range(0, len(lines), 3):
        (s, inp), (_, b), (_, a) = lines[i:i + 3]
        if not b.startswith("  before\t") or not a.startswith("  after\t"):
            sys.exit(f"FAIL scripts/series-golden.py: {MOVED} is not inputs, before, after at {inp}")
        sections[s][inp] = (b.split("\t")[1:3], a.split("\t")[1:3])
    return sections


def fails(lines, name):
    path = os.path.join(ROOT, "build", name)
    open(path, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    p = subprocess.run([CHECK, path], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8")
    last = p.stdout.strip().split("\n")[-1]
    return p.returncode != 0 or not last.endswith("failed: 0"), last


def before(path, mv, bit, name):
    """the file with mv's lines at their before, run with the route compiled out: (failures, the file, its first moved
    line's index, its result line)"""
    bad, out, hit, first = 0, [], set(), None
    for ln in open(path, encoding="utf-8").read().split("\n"):
        f = ln.split("\t")
        key = "\t".join(f[:10])
        if not ln or ln.startswith("#"):
            continue
        if key in mv:
            b, a = mv[key]
            hit.add(key)
            if f[10:12] != a[:2]:
                bad += 1
                print(f"FAIL scripts/series-golden.py: {name} has {f[10:12]} where cas-021.txt's after is {a}: {key}")
            if b[0] not in FROM:
                bad += 1
                print(f"FAIL scripts/series-golden.py: {name}: moved from {b[0]}, which rule 1 never moves: {key}")
            if first is None:
                first = len(out)
            f = f[:10] + b[:2]
        f[3] = str(int(f[3]) | bit)
        out.append("\t".join(f))
    for key in set(mv) - hit:
        bad += 1
        print(f"FAIL scripts/series-golden.py: cas-021.txt lists a line {name} doesn't have: {key}")
    failed, last = fails(out, f"series-before-{name}.txt")
    if failed:
        bad += 1
        print(f"FAIL scripts/series-golden.py: with the series route compiled out, {name} moved: {last}")
    return bad, out, first, last


def main():
    moved_mv, dec_mv, new_mv = moved()
    corpus_mv = {**moved_mv, **new_mv}
    bad, out, first, last = before(os.path.join(GOLDEN, "corpus.txt"), corpus_mv, 0, "corpus")
    dbad, _, _, dlast = before(os.path.join(GOLDEN, "decimal.txt"), dec_mv, PRINT_DECIMAL, "decimal")
    bad += dbad
    f = out[first].split("\t")
    f[10:12] = corpus_mv["\t".join(f[:10])][1][:2]       # one line left at the series route's answer
    out[first] = "\t".join(f)
    cfailed, _ = fails(out, "series-control.txt")
    if not cfailed:
        bad += 1
        print("FAIL scripts/series-golden.py: its control passed (a series answer with the route compiled out)")
    print(f"series boundary: {len(moved_mv)} moved inputs and {len(new_mv)} new in the corpus, {len(dec_mv)} under "
          f"→DEC, each from {', '.join(FROM)}; with the route compiled out, the corpus: {last}; →DEC's: {dlast}; the "
          f"control refused\n"
          f"checks: {len(corpus_mv) + len(dec_mv) + 3} failed: {bad}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
