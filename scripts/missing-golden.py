#!/usr/bin/env python3
"""CAS 036's boundary (make check; work/036-missing-values.md). The unit moves the golden lines
tests/golden/moved/cas-036.txt lists, and no others: with each listed line put back at its before, the corpus and
→DEC's file are 1.4.0's 6090b8c byte for byte (the sha256s in the list's header), and each listed line's after is the
file's. Every after keeps its before's status, its first part and its rows in order, and adds one or more rows. Each
added row is judged apart from the code that adds it: at a letter's value it excludes and the old rows don't (0, ±1,
±2, ½, ±π÷2, π, one letter at a time, the others generic), the after's answer has no value at the empty sum
(N = lo-1), where the sum is 0. The check's controls: a before list with one line left at its after must fail the
hash, and the judge must refuse a row that names no missing value.
    scripts/missing-golden.py"""
import decimal
import hashlib
import importlib.util
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN = os.path.join(ROOT, "tests", "golden")
MOVED = os.path.join(GOLDEN, "moved", "cas-036.txt")
BASE = "6090b8c"
_spec = importlib.util.spec_from_file_location("sv", os.path.join(ROOT, "scripts", "sum-values.py"))
sv = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(sv)
GEN = {"X": "1÷3", "Y": "1÷5", "Z": "7÷10", "W": "2÷7"}
SPECIAL = ["0", "1", "-1", "2", "-2", "1÷2", "π÷2", "π", "-π÷2"]
ROW = re.compile(r"^(.*?)(≠|<|>|≥|≤)(.*)$")
NOVALUE = (ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero, OverflowError, ValueError)


def moved(path=MOVED):
    """the list's sections (the corpus's moves, →DEC's): inputs (the first ten fields) -> (before, after), each
    [status, text]; and the header's sha256 for each file"""
    sections, lines, sha = [{}], [], {}
    for ln in open(path, encoding="utf-8").read().split("\n"):
        if ln.startswith("# " + BASE + " ") and " sha256 " in ln:
            name, _, h = ln[len("# " + BASE + " "):].split(" ")
            sha[name] = h
        elif ln.startswith("# Under →DEC"):
            sections.append({})
        elif ln and not ln.startswith("#"):
            lines.append((len(sections) - 1, ln))
    for i in range(0, len(lines), 3):
        (s, inp), (_, b), (_, a) = lines[i:i + 3]
        if not b.startswith("  before\t") or not a.startswith("  after\t"):
            sys.exit(f"FAIL scripts/missing-golden.py: {path} is not inputs, before, after at {inp}")
        sections[s][inp] = (b.split("\t")[1:3], a.split("\t")[1:3])
    return sections, sha


def at(text, pt):
    for L, v in pt.items():
        text = sv.sub(text, L, v)
    v = sv.value(text, strict=True)
    if not v.is_finite():
        raise ZeroDivisionError(text)
    return v


def holds(row, pt):
    """whether the row is true at pt (a row with no value there excludes nothing: true)"""
    l, op, r = ROW.match(row).groups()
    try:
        a, b = at(l, pt), at(r, pt)
    except NOVALUE:
        return True
    return {"≠": abs(a - b) >= sv.TOL, "<": a < b, ">": a > b, "≥": a >= b - sv.TOL, "≤": a <= b + sv.TOL}[op]


def names(row, old, answer, lo):
    """whether row excludes a point the old rows don't, where answer has no value at N = lo-1"""
    letters = sorted({L for L in GEN if re.search(r"(?<![A-Z])" + L + r"(?![A-Z])", row)})
    for L in letters:
        for v in SPECIAL:
            pt = dict(GEN, **{L: v})
            if holds(row, pt) or not all(holds(o, pt) for o in old):
                continue
            try:
                at(answer, dict(pt, N=str(int(lo) - 1)))
            except NOVALUE:
                return True
    return False


def before(name, mv, keep=None):
    """the file with mv's lines at their before (keep: one input left at its after): (failures, its sha256)"""
    bad, out, hit = 0, [], set()
    for ln in open(os.path.join(GOLDEN, name), encoding="utf-8").read().split("\n"):
        f = ln.split("\t")
        key = "\t".join(f[:10])
        if ln and not ln.startswith("#") and key in mv:
            b, a = mv[key]
            hit.add(key)
            if f[10:12] != a[:2]:
                bad += 1
                print(f"FAIL scripts/missing-golden.py: {name} has {f[10:12]} where cas-036.txt's after is {a}: {key}")
            pb, pa = b[1].split("; "), a[1].split("; ")
            new = [r for r in pa[1:] if r not in pb[1:]]
            if a[0] != b[0] or pa[0] != pb[0] or [r for r in pa[1:] if r not in new] != pb[1:] or not new:
                bad += 1
                print(f"FAIL scripts/missing-golden.py: {name}: its after isn't its before with rows added: {key}")
            for r in new:
                if not names(r, pb[1:], pa[0], f[8]):
                    bad += 1
                    print(f"FAIL scripts/missing-golden.py: {name}: {r} names no missing value: {key}")
            if key != keep:
                f = f[:10] + b[:2]
        out.append("\t".join(f))
    for key in set(mv) - hit:
        bad += 1
        print(f"FAIL scripts/missing-golden.py: cas-036.txt lists a line {name} doesn't have: {key}")
    return bad, hashlib.sha256("\n".join(out).encode("utf-8")).hexdigest()


def main():
    (corpus_mv, dec_mv), sha = moved()
    bad = 0
    for name, mv in (("corpus.txt", corpus_mv), ("decimal.txt", dec_mv)):
        b, h = before(name, mv)
        bad += b
        if h != sha.get(name):
            bad += 1
            print(f"FAIL scripts/missing-golden.py: {name} with its moves put back isn't {BASE}'s (sha256 {h})")
    _, h = before("corpus.txt", corpus_mv, keep=next(iter(corpus_mv)))
    if h == sha.get("corpus.txt"):
        bad += 1
        print("FAIL scripts/missing-golden.py: its control passed (a moved line left at its after)")
    answer = "1÷(X-1)+(1÷X)^(N+1)÷(1÷X-1)"
    if names("X≠2", ["X≠1"], answer, "1") or not names("X≠0", ["X≠1"], answer, "1"):
        bad += 1
        print("FAIL scripts/missing-golden.py: its judge's control (X≠2 names no missing value; X≠0 names X^(-K)'s)")
    print(f"missing boundary: {len(corpus_mv)} moved in the corpus, {len(dec_mv)} under →DEC, each its before with "
          f"rows that name a missing value at the empty sum; with them put back, both files are {BASE}'s; the "
          f"controls refused\n"
          f"checks: {3 * (len(corpus_mv) + len(dec_mv)) + 4} failed: {bad}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
