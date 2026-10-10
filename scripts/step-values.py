#!/usr/bin/env python3
"""derive's steps judged by value (make check; work/steps-derive.md, oracle 2 to 4). Seeded random expressions in X
go through `build/casim --tsv steps`, both ways (two lines a rule, and one), and each step is evaluated in Python's
math, which shares no code with casim:
  - a result line, d/dX(u) = v: v is u's derivative (a central difference) at fixed points in RAD and DEG, where
    that estimate is stable (it abstains on a huge or steep value);
  - a template line, d/dX(u) = t: t, each d/dX(w) in it taken numerically, is u's derivative too;
  - the root's result is plain derive's answer, byte for byte; k runs 0..count-1; a second run is identical.
    scripts/step-values.py [N] [SEED]     (default 1000 expressions, seed 20261010)"""
import math
import os
import random
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "build", "casim")
FN = ["SIN", "COS", "TAN", "LN", "EXP", "SQRT", "ATAN", "SINH", "COSH", "LOG", "ALOG"]
POINTS = [0.37, 0.81, 1.23]
# deep cases plain derive answers at the depth guard's edge, which steps must page too: the S3 bench found a 44-deep
# SIN chain paging NO_ROOM when steps were printed inside the walk, and a walk that counts a node's level twice
# fails LN(...44...)×X^2. Then shapes random inputs rarely make.
FIXED = ["SIN(" * 44 + "X" + ")" * 44, "X^(" * 20 + "X" + ")" * 20, "LN(" * 44 + "X" + ")" * 44 + "×X^2",
         "(X^-1)^(X+1)", "(SIN(X)^-2)^X×X"]      # a general power's base with a ÷ (its template's divisor is whole)


def expr(rnd, depth):
    r = rnd.random()
    if depth <= 0 or r < 0.25:
        return rnd.choice(["X", "X", "2", "3", "0.5", "π", "Y"])
    if r < 0.45:
        return rnd.choice(FN) + "(" + expr(rnd, depth - 1) + ")"
    if r < 0.6:
        return "(" + expr(rnd, depth - 1) + ")^" + rnd.choice(["2", "3", "0.5", "-1", "X", "(X+1)"])
    op = rnd.choice(["+", "-", "×", "÷", "×"])
    return "(" + expr(rnd, depth - 1) + op + expr(rnd, depth - 1) + ")"


def py(e):
    e = e.replace("×", "*").replace("÷", "/").replace("^", "**").replace("π", "pi")
    for a, b in [("ALOG(", "alog("), ("LOG(", "log10("), ("LN(", "log("), ("SQRT(", "sqrt("), ("ATAN(", "atan("),
                 ("SINH(", "sinh("), ("COSH(", "cosh("), ("SIN(", "sin("), ("COS(", "cos("), ("TAN(", "tan("),
                 ("EXP(", "exp("), ("ABS(", "abs("), ("SGN(", "sgn(")]:
        e = e.replace(a, b)
    return e


def env(x, deg):
    k = math.pi / 180 if deg else 1.0
    return {"__builtins__": {}, "pi": math.pi, "log": math.log, "log10": math.log10, "sqrt": math.sqrt,
            "exp": math.exp, "alog": lambda u: 10 ** u, "abs": abs, "sgn": lambda u: (u > 0) - (u < 0),
            "sin": lambda u: math.sin(u * k), "cos": lambda u: math.cos(u * k), "tan": lambda u: math.tan(u * k),
            "atan": lambda u: math.atan(u) / k, "sinh": math.sinh, "cosh": math.cosh, "X": x, "Y": 1.7}


def value(e, x, deg):
    try:
        v = eval(py(e), env(x, deg))
        if isinstance(v, complex):
            return None
        v = float(v)
    except (ValueError, ZeroDivisionError, OverflowError, TypeError):
        return None
    return v if math.isfinite(v) else None


def deriv(e, x, deg):
    """e's derivative at x by central differences, or None where the estimate is not stable (two step sizes
    disagree: a huge or steep value, where the judge abstains rather than guesses)"""
    est = []
    for h in (1e-4, 1e-5):
        h *= max(1.0, abs(x))
        a, b = value(e, x + h, deg), value(e, x - h, deg)
        if a is None or b is None:
            return None
        est.append((a - b) / (2 * h))
        if 1e-15 * max(abs(a), abs(b)) / h > 1e-5 * max(1.0, abs(est[-1])):
            return None                 # the rounding of a huge value swamps the difference
    return est[1] if close(est[0], est[1]) else None


def close(a, b):
    return abs(a - b) <= 1e-4 * max(1.0, abs(a), abs(b))


def split_d(text):
    """'d/dX(u) = rest' as (u, rest), the parentheses matched"""
    assert text.startswith("d/dX(")
    depth, i = 0, 4
    for i in range(4, len(text)):
        depth += {"(": 1, ")": -1}.get(text[i], 0)
        if depth == 0:
            break
    return text[5:i], text[i + 1:].removeprefix(" = ")


def templ_value(t, x, deg):
    """a template's value: each d/dX(w) in it replaced by w's numeric derivative"""
    while "d/dX(" in t:
        j = t.index("d/dX(")
        depth, i = 0, j + 4
        for i in range(j + 4, len(t)):
            depth += {"(": 1, ")": -1}.get(t[i], 0)
            if depth == 0:
                break
        d = deriv(t[j + 5:i], x, deg)
        if d is None:
            return None
        t = t[:j] + "(" + repr(d) + ")" + t[i + 1:]
    return value(t, x, deg)


ABSTAINED = [0]


def wrong(before, line, after, deg):
    """why a step is wrong (empty when it is right at every point where both sides are defined)"""
    judged = 0
    for x in POINTS:
        d = deriv(before, x, deg)
        v = value(after, x, deg) if line == "R" else templ_value(after, x, deg)
        if d is None or v is None:
            continue
        judged += 1
        if not close(d, v):
            return f"at X={x}: d/dX {d!r}, the step {v!r}"
    ABSTAINED[0] += judged == 0
    return ""


def steps(e, deg, one):
    args = [CLI, "--tsv"] + (["--one-line"] if one else []) + ["steps", e, "X", "DEG" if deg else "RAD"]
    p = subprocess.run(args, stdout=subprocess.PIPE, text=True, encoding="utf-8")
    return p.returncode, p.stdout


def plain(e, deg):
    p = subprocess.run([CLI, "derive", e, "X", "DEG" if deg else "RAD"], stdout=subprocess.PIPE, text=True,
                       encoding="utf-8")
    return p.returncode, p.stdout.strip()


def main():
    # the judge first: a wrong result and a wrong template must be refused, or it checks nothing
    for before, line, after in [("X^2×SIN(X)", "R", "2×X×SIN(X)+X^2×SIN(X)"),
                                ("X^2×SIN(X)", "T", "d/dX(X^2)×SIN(X)+X×d/dX(SIN(X))")]:
        if not wrong(before, line, after, False):
            print(f"FAIL scripts/step-values.py: its judge accepts d/dX({before}) = {after}")
            sys.exit(1)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 1000
    rnd = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 20261010)
    failed = lines = 0
    cases = [(e, False) for e in FIXED] + [(e, True) for e in FIXED]
    cases += [(expr(rnd, rnd.randint(1, 4)), rnd.random() < 0.25) for _ in range(n)]
    for e, deg in cases:
        pc, pout = plain(e, deg)
        for one in (False, True):
            rc, out = steps(e, deg, one)
            why = []
            if (rc == 0) != (pc == 0):
                why.append(f"steps exit {rc}, derive exit {pc}")
            if rc == 0:
                rows = [r.split("\t") for r in out.rstrip("\n").split("\n")]
                if [int(r[0]) for r in rows] != list(range(len(rows))):
                    why.append("k is not 0..count-1")
                root = rows[0] if one else rows[-1]
                if root[1] != "0" or root[2] != "R":
                    why.append(f"the root's result is not step {0 if one else len(rows) - 1}")
                elif (root[4] if root[3] == "BOTH SIDES" else split_d(root[4])[1]) != pout:
                    why.append(f"the root's result {root[4]} is not derive's {pout}")
                if steps(e, deg, one)[1] != out:
                    why.append("a second run differs")
                for r in rows:
                    if r[3] == "BOTH SIDES":
                        continue
                    lines += 1
                    before, after = split_d(r[4])
                    w = wrong(before, r[2], after, deg)
                    if w:
                        why.append(f"step {r[0]} {r[2]} {r[4]}: {w}")
            if why:
                failed += 1
                print(f"FAIL scripts/step-values.py: {e} ({'DEG' if deg else 'RAD'}, "
                      f"{'one line' if one else 'two lines'})\n  " + "\n  ".join(why))
    print(f"step values: {len(cases)} expressions both ways ({len(FIXED)} fixed ones in RAD and DEG), {lines} steps, {lines - ABSTAINED[0]} judged at a point at least "
          f"(the rest undefined or unstable there); the judge refused its two planted steps\n"
          f"checks: {2 * len(cases)} failed: {failed}")
    sys.exit(1 if failed else 0)


main()
