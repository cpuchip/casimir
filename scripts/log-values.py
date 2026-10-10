#!/usr/bin/env python3
"""The logs and π fractions (cas-pi-form) judged by value (make check): seeded random sums of k×LN(a), k×LOG(a),
letters, logs of letters and multiples of π go through build/casim simplify, and each input and answer is evaluated
in Python's math, which shares no code with casim. Every answer must
  - have the input's value, at X = 0.7, Y = 1.9;
  - be defined exactly where the input is, at X = -0.7, Y = -1.9 too (LN(X)+LN(Y) as LN(X×Y) would be defined there:
    logs of letters never combine);
  - simplify to itself.
    scripts/log-values.py [N] [SEED]     (default 2000 sums, seed 20261010)"""
import math
import os
import random
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "build", "casim")
NUMS = ["2", "3", "4", "5", "6", "8", "9", "10", "12", "0.5", "0.25", "1.5", "2÷3", "100", "0.01", "1", "7", "1000"]
KS = ["", "2×", "3×", "-", "0.5×", "-2×", "1.5×", "1÷3×", "4×", "2÷3×"]
OTHER = ["X", "Y", "X×LN(2)", "LN(X)", "LN(Y)", "LOG(X)", "π", "π×X", "π^2", "1÷π", "SQRT(π)"]


def term(rnd):
    r = rnd.random()
    if r < 0.6:
        return rnd.choice(KS) + rnd.choice(["LN", "LOG"]) + "(" + rnd.choice(NUMS) + ")"
    if r < 0.85:
        return rnd.choice(KS) + rnd.choice(OTHER)
    return rnd.choice(["1", "2", "0.5", "π÷4", "0.25×π"])


def value(e, x, y):
    """e's value at X = x, Y = y, or None where it is undefined"""
    for a, b in [("×", "*"), ("÷", "/"), ("^", "**"), ("π", "pi"), ("LOG(", "log10("), ("LN(", "log("),
                 ("SQRT(", "sqrt(")]:
        e = e.replace(a, b)
    try:
        v = eval(e, {"__builtins__": {}, "log": math.log, "log10": math.log10, "pi": math.pi, "sqrt": math.sqrt,
                     "X": x, "Y": y})
    except (ValueError, ZeroDivisionError):
        return None
    return None if isinstance(v, complex) else v


def simplify(e):
    p = subprocess.run([CLI, "simplify", e, "RAD"], stdout=subprocess.PIPE, text=True, encoding="utf-8")
    return p.stdout.strip() if p.returncode == 0 else None


def wrong(e, out):
    """why out is not e simplified (empty when it is)"""
    why = []
    if out is None:
        return ["not OK"]
    for x, y in [(0.7, 1.9), (-0.7, -1.9)]:
        v0, v1 = value(e, x, y), value(out, x, y)
        if (v0 is None) != (v1 is None):
            why.append(f"defined {v0 is not None} -> {v1 is not None} at X={x}")
        elif v0 is not None and abs(v0 - v1) > 1e-9 * max(1, abs(v0)):
            why.append(f"value {v0!r} -> {v1!r} at X={x}")
    return why


def main():
    # the judge first: it must refuse a combined letter and a wrong value, or it checks nothing
    for e, out in [("LN(X)+LN(Y)", "LN(X×Y)"), ("LN(6)-LN(2)", "LN(4)")]:
        if not wrong(e, out):
            print(f"FAIL scripts/log-values.py: its judge accepts {e} as {out}")
            sys.exit(1)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    rnd = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 20261010)
    failed = fewer = 0
    for _ in range(n):
        e = "+".join(term(rnd) for _ in range(rnd.randint(1, 5))).replace("+-", "-")
        out = simplify(e)
        why = wrong(e, out)
        if out is not None:
            again = simplify(out)
            if again != out:
                why.append(f"simplifies again to {again}")
            fewer += out.count("LN(") + out.count("LOG(") < e.count("LN(") + e.count("LOG(")
        if why:
            failed += 1
            print(f"FAIL scripts/log-values.py: {e}\n  casim {out}\n  " + "; ".join(why))
    print(f"log values: {n} sums, {fewer} with fewer logs; the judge refused its two planted answers\n"
          f"checks: {n} failed: {failed}")
    sys.exit(1 if failed else 0)


main()
