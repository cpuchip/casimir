#!/usr/bin/env python3
"""CAS 032's boundary (make check; work/032-redundant-row.md). The unit moves the golden lines
tests/golden/moved/cas-032.txt lists, and no others: with each listed line put back at its before, the corpus and
→DEC's file are 1.2.0's 2a9c354 byte for byte (the sha256s in the list's header), and each listed line's after is the
file's. Every after keeps its before's status, its first part and its kept rows in order, and drops one or more L≠0
rows. Each dropped row is judged apart from the code that drops it: a kept row has a value at L = 0 and is false
there, at generic values of the other letters and at special ones (0, ±1, ±2, ½, ±π÷2, π), one letter at a time
(abacus #10635 (1)). The check's control: a before list with one line left at its after must fail the hash.
    scripts/said-golden.py"""
import decimal
import hashlib
import importlib.util
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN = os.path.join(ROOT, "tests", "golden")
MOVED = os.path.join(GOLDEN, "moved", "cas-032.txt")
BASE = "2a9c354"
_spec = importlib.util.spec_from_file_location("sv", os.path.join(ROOT, "scripts", "sum-values.py"))
sv = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(sv)
GEN = {"X": "1÷3", "Y": "1÷5", "Z": "7÷10", "W": "2÷7"}
SPECIAL = ["0", "1", "-1", "2", "-2", "1÷2", "π÷2", "π", "-π÷2"]
BIG = decimal.Decimal(10) ** 25
ROW = re.compile(r"^(.*?)(≠|<|>)(.*)$")
NOVALUE = (ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero, OverflowError, ValueError)


def moved():
    """the list's sections (the corpus's moves, →DEC's): inputs (the first ten fields) -> (before, after), each
    [status, text]; and the header's sha256 for each file"""
    sections, lines, sha = [{}], [], {}
    for ln in open(MOVED, encoding="utf-8").read().split("\n"):
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
            sys.exit(f"FAIL scripts/said-golden.py: {MOVED} is not inputs, before, after at {inp}")
        sections[s][inp] = (b.split("\t")[1:3], a.split("\t")[1:3])
    return sections, sha


def at(text, pt):
    for L, v in pt.items():
        text = sv.sub(text, L, v)
    v = sv.value(text)
    if not v.is_finite() or abs(v) > BIG:
        raise ZeroDivisionError(text)
    return v


def says(row, L):
    """whether row is false with a value at L = 0, the other letters at every generic and special point"""
    l, op, r = ROW.match(row).groups()
    pts = [dict(GEN)] + [dict(GEN, **{M: v}) for M in GEN if M != L for v in SPECIAL]
    for pt in pts:
        pt[L] = "0"
        try:
            a, b = at(l, pt), at(r, pt)
        except NOVALUE:
            return False
        if not (abs(a - b) < sv.TOL if op == "≠" else a >= b - sv.TOL if op == "<" else a <= b + sv.TOL):
            return False
    return True


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
                print(f"FAIL scripts/said-golden.py: {name} has {f[10:12]} where cas-032.txt's after is {a}: {key}")
            pb, pa = b[1].split("; "), a[1].split("; ")
            gone = [r for r in pb[1:] if r not in pa[1:]]
            if a[0] != b[0] or pa[0] != pb[0] or [r for r in pb[1:] if r not in gone] != pa[1:] or not gone or \
                    any(not re.fullmatch(r"[A-JL-MO-Z]≠0", r) for r in gone):
                bad += 1
                print(f"FAIL scripts/said-golden.py: {name}: its after isn't its before less L≠0 rows: {key}")
            for r in gone:
                if not any(says(k, r[0]) for k in pa[1:]):
                    bad += 1
                    print(f"FAIL scripts/said-golden.py: {name}: no kept row says {r}: {key}")
            if key != keep:
                f = f[:10] + b[:2]
        out.append("\t".join(f))
    for key in set(mv) - hit:
        bad += 1
        print(f"FAIL scripts/said-golden.py: cas-032.txt lists a line {name} doesn't have: {key}")
    return bad, hashlib.sha256("\n".join(out).encode("utf-8")).hexdigest()


def main():
    (corpus_mv, dec_mv), sha = moved()
    bad = 0
    for name, mv in (("corpus.txt", corpus_mv), ("decimal.txt", dec_mv)):
        b, h = before(name, mv)
        bad += b
        if h != sha.get(name):
            bad += 1
            print(f"FAIL scripts/said-golden.py: {name} with its moves put back isn't {BASE}'s (sha256 {h})")
    _, h = before("corpus.txt", corpus_mv, keep=next(iter(corpus_mv)))
    if h == sha.get("corpus.txt"):
        bad += 1
        print("FAIL scripts/said-golden.py: its control passed (a moved line left at its after)")
    if says("X÷Y+1≠1", "X") or not says("EXP(X)≠1", "X"):
        bad += 1
        print("FAIL scripts/said-golden.py: its judge's control (X÷Y+1≠1 says nothing at X = 0; EXP(X)≠1 says it)")
    print(f"said boundary: {len(corpus_mv)} moved in the corpus, {len(dec_mv)} under →DEC, each its before less L≠0 "
          f"rows a kept row says; with them put back, both files are {BASE}'s; the controls refused\n"
          f"checks: {3 * (len(corpus_mv) + len(dec_mv)) + 4} failed: {bad}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
