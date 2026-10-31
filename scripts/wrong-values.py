#!/usr/bin/env python3
"""CAS 037's check (make check; work/037-wrong-values.md, abacus #11884 (5)): no sum line has a value where the typed
sum has none. Each sum in tests/wrong-values.txt is run through the CLI and added term by term from the typed summand,
each term valued strictly by scripts/sum-values.py's evaluator. A line fails where its first part has a value at a
point the sum has none and no row excludes it (a row with no value there excludes nothing). Points: each letter at
0, ±1, 2, 1÷2, π÷2 and π (three letters or more, a fixed sample), N from lo to lo+4 and, for a lo below 0, up to 0
(the empty sum is 0); to ∞ the sum has none where a term from lo to lo+6 has none. A summand with SQ or INV, or a chained
power, is left out: the evaluator can't read the first two and reads a^b^c from the right. The control: CAS 036's
line for Y^K×Y^(-K) from 1 to N, `N`, must fail, and CAS 037's, `N; Y≠0`, must pass.
    scripts/wrong-values.py [CLI]"""
import decimal
import importlib.util
import itertools
import os
import random
import re
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor
from decimal import Decimal as D

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "casim")
SUMS = os.path.join(ROOT, "tests", "wrong-values.txt")
_spec = importlib.util.spec_from_file_location("sv", os.path.join(ROOT, "scripts", "sum-values.py"))
sv = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(sv)
CTX = decimal.getcontext().copy()            # the evaluator's digits: Decimal's context is per process
NOVALUE = (ValueError, ZeroDivisionError, ArithmeticError, OverflowError, KeyError, IndexError, TypeError)
VALS = ["0", "1", "-1", "2", "1÷2", "π÷2", "π"]
ROW = re.compile(r"^(.*?)(≠|<|>|≥|≤)(.*)$")
STATUS = ("NO_", "TOO_", "UNDEF", "UNDEC", "BAD_", "NOT_", "DECIMAL", "SYNTAX", "PARTIAL", "(rc")
TOL = D("1e-25")
BIG = D("1e30")


def val(text, pt):
    for L, v in pt.items():
        text = sv.sub(text, L, v)
    try:
        v = sv.value(text, "RAD", strict=True)
    except NOVALUE:
        return None
    return v if v.is_finite() and abs(v) <= BIG else None


def letters(s):
    return sorted(set(re.findall(r"(?<![A-Z])([A-JL-MO-Z])(?![A-Z(])", s)))


def the_sum(s, lo, n, pt):
    tot = D(0)
    for k in range(lo, n + 1):
        t = val(s, dict(pt, K=str(k)))
        if t is None:
            return None
        tot += t
    return tot


def excluded(rows, pt):
    for r in rows:
        m = ROW.match(r)
        if not m:
            continue
        a, b = val(m.group(1), pt), val(m.group(3), pt)
        if a is None or b is None:
            continue
        ok = {"≠": abs(a - b) > TOL, "<": a < b - TOL, ">": a > b + TOL, "≥": a >= b - TOL, "≤": a <= b + TOL}[m.group(2)]
        if not ok:
            return True
    return False


def wrong(s, lo, hi, out):
    """the first point where the line has a value and the typed sum none, or None ("left" where it can't judge)"""
    decimal.setcontext(CTX.copy())
    if hi not in ("N", "∞") or out.startswith(STATUS):
        return None
    if re.search(r"(?<![A-Z])(SQ|INV)\(", s) or re.search(r"\^[^()×÷+\-]*\^|\^\([^()]*\)\^", s):
        return "left"
    parts = out.split("; ")
    first, rows = parts[0], parts[1:]
    ls = letters(s)
    rnd = random.Random(sum(map(ord, s + lo)))
    combos = list(itertools.product(VALS, repeat=len(ls))) if len(ls) <= 2 else \
        [tuple(rnd.choice(VALS) for _ in ls) for _ in range(80)] + [tuple("0" if j == i else "1÷2" for j in range(len(ls)))
                                                                    for i in range(len(ls))]
    lo_i = int(lo)
    ns = [lo_i + 6] if hi == "∞" else sorted(set(list(range(lo_i, lo_i + 5)) + list(range(lo_i, 1))))
    for combo in combos:
        P = dict(zip(ls, combo))
        for n in ns:
            pt = P if hi == "∞" else dict(P, N=str(n))
            if the_sum(s, lo_i, n, P) is not None or excluded(rows, pt):
                continue
            if val(first, pt) is not None:
                return pt
    return None


def judge(item):
    s, lo, hi = item
    p = subprocess.run([CLI, "sum", s, "K", lo, hi, "RAD"], capture_output=True, text=True, timeout=120)
    out = p.stdout.strip() or f"(rc {p.returncode})"
    return s, lo, hi, out, wrong(s, lo, hi, out)


def main():
    failed = 0
    if wrong("Y^K×Y^(-K)", "1", "N", "N") is None or wrong("Y^K×Y^(-K)", "1", "N", "N; Y≠0") is not None:
        print("FAIL scripts/wrong-values.py: the control (`N` must fail, `N; Y≠0` pass) didn't hold")
        failed += 1
    items = []
    for ln in open(SUMS, encoding="utf-8"):
        if "|" in ln and not ln.startswith("#"):
            f = ln.rstrip("\n").split("|")
            items.append((f[0], f[1], f[2]))
    with ProcessPoolExecutor(min(4, os.cpu_count() or 1)) as ex:   # the evaluator is Python: processes; the box is shared
        res = list(ex.map(judge, items, chunksize=8))
    left = 0
    for s, lo, hi, out, w in res:
        if w == "left":
            left += 1
        elif w is not None:
            failed += 1
            print(f"FAIL scripts/wrong-values.py: sum {s} K {lo} {hi}: `{out}` has a value at {w}, where the sum has none")
    print(f"wrong values: {len(items)} sums in {os.path.relpath(SUMS, ROOT)} ({left} left out), the control run\n"
          f"checks: {len(items) - left + 1} failed: {failed}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
