#!/usr/bin/env python3
"""CAS 035's boundary (make check; work/035-typed-domain.md). The unit moves the golden lines
tests/golden/moved/cas-035.txt lists, and no others: with each listed line put back at its before, the corpus and
→DEC's file are cda0fcb's byte for byte (the sha256s in the list's header), and each listed line's after is the file's.
Every after is a refusal (UNDEFINED, IMPROPER, BAD_ARG) where its before was a value or UNDECIDED, and each is judged
apart from the code that refuses: the typed input, valued by scripts/sum-values.py's strict evaluator, has no value
somewhere it is asked about (defint: inside the interval, the other letters at 0, ±1, ±2, ½; limit: beside the point
on each side asked, or far out at ±∞). The check's control: a before list with one line left at its after must fail the
hash.
    scripts/typed-golden.py"""
import decimal
import hashlib
import importlib.util
import itertools
import os
import re
import sys
from decimal import Decimal as D

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN = os.path.join(ROOT, "tests", "golden")
MOVED = os.path.join(GOLDEN, "moved", "cas-035.txt")
BASE = "cda0fcb"
_spec = importlib.util.spec_from_file_location("sv", os.path.join(ROOT, "scripts", "sum-values.py"))
sv = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(sv)
NOVALUE = (ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero, OverflowError, ValueError)
REFUSED = ("UNDEFINED", "IMPROPER", "BAD_ARG")
LETTERS = ["0", "1", "-1", "2", "-2", "1÷2"]
ANGLE = {"0": "DEG", "1": "RAD", "2": "GRAD"}


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
            sys.exit(f"FAIL scripts/typed-golden.py: {MOVED} is not inputs, before, after at {inp}")
        sections[s][inp] = (b.split("\t")[1:3], a.split("\t")[1:3])
    return sections, sha


def has(text, pt, angle):
    for L, v in pt.items():
        text = sv.sub(text, L, v)
    try:
        v = sv.value(text, angle, strict=True)
    except NOVALUE:
        return False
    return v.is_finite()


def num(s):
    s = s.replace("π", "3.14159265358979323846264338327950288")
    if "÷" in s:
        a, b = s.split("÷")
        return D(a) / D(b)
    return D(s)


def plain(text):
    """casim's 1E3 and 1.5E-2 as plain decimals: the evaluator reads no exponent form (and an E after a digit isn't a
    letter)"""
    return re.sub(r"(\d+(?:\.\d+)?)E(-?\d+)", lambda m: format(D(m.group(1)) * D(10) ** int(m.group(2)), "f"), text)


def holeless(key):
    """'' when the typed input has no value somewhere it is asked about; else why it has one everywhere sampled"""
    f = key.split("\t")
    op, var, angle, eqn = f[0], chr(ord("A") + int(f[2])), ANGLE.get(str(int(f[3]) & 3), "RAD"), plain(f[7])
    letters = [L for L in "XYZWNMKT" if L != var and any(L == ch and (i == 0 or not eqn[i - 1].isalpha()) and
                                                         (i + 1 == len(eqn) or not eqn[i + 1].isalpha())
                                                         for i, ch in enumerate(eqn))]
    if op == "defint":
        lo, hi = num(f[8]), num(f[9])
        xs = [lo] if lo == hi else [lo + (hi - lo) * D(k) / 16 for k in range(17)]
    else:
        side = f[9]
        if f[8] in ("∞", "-∞"):
            xs = [D(10) ** 6 * (1 if f[8] == "∞" else -1)]
        else:
            a = num(f[8])
            xs = ([a + D("1E-9")] if side != "-" else []) + ([a - D("1E-9")] if side != "+" else [])
    for combo in itertools.product(LETTERS, repeat=len(letters)):
        for x in xs:
            pt = dict(zip(letters, combo), **{var: format(x, "f")})
            if not has(eqn, pt, angle):
                return ""
    return "the typed input has a value at every point sampled"


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
                print(f"FAIL scripts/typed-golden.py: {name} has {f[10:12]} where cas-035.txt's after is {a}: {key}")
            if b[0] not in ("OK", "UNDECIDED") or a[0] not in REFUSED:
                bad += 1
                print(f"FAIL scripts/typed-golden.py: {name}: a move from {b[0]} to {a[0]}: {key}")
            why = holeless(key)
            if why:
                bad += 1
                print(f"FAIL scripts/typed-golden.py: {name}: {why}: {key}")
            if key != keep:
                f = f[:10] + b[:2]
        out.append("\t".join(f))
    for key in set(mv) - hit:
        bad += 1
        print(f"FAIL scripts/typed-golden.py: cas-035.txt lists a line {name} doesn't have: {key}")
    return bad, hashlib.sha256("\n".join(out).encode("utf-8")).hexdigest()


def main():
    (corpus_mv, dec_mv), sha = moved()
    bad = 0
    for name, mv in (("corpus.txt", corpus_mv), ("decimal.txt", dec_mv)):
        b, h = before(name, mv)
        bad += b
        if h != sha.get(name):
            bad += 1
            print(f"FAIL scripts/typed-golden.py: {name} with its moves put back isn't {BASE}'s (sha256 {h})")
    _, h = before("corpus.txt", corpus_mv, keep=next(iter(corpus_mv)))
    if h == sha.get("corpus.txt"):
        bad += 1
        print("FAIL scripts/typed-golden.py: its control passed (a moved line left at its after)")
    if holeless("defint\t35\t23\t1\t35\t8800\t65536\tSQRT(X)\t0\t1") == "" or \
            holeless("defint\t35\t23\t1\t35\t8800\t65536\tSQRT(X)\t-2\t-1") != "":
        bad += 1
        print("FAIL scripts/typed-golden.py: its judge's control (SQRT(X) has values on 0..1, none on -2..-1)")
    print(f"typed boundary: {len(corpus_mv)} moved in the corpus, {len(dec_mv)} under →DEC, each to a refusal where the "
          f"typed input has no value; with them put back, both files are {BASE}'s; the controls refused\n"
          f"checks: {3 * (len(corpus_mv) + len(dec_mv)) + 4} failed: {bad}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
