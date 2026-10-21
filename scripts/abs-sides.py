#!/usr/bin/env python3
"""CAS 021's ABS rows judged by one-sided rewrites (make check; work/021-series.md). Each row is a shape g with
ABS(u) in it, u(0) = 0, at 0 to order n. On X > 0, ABS(u) is sR×u, and on X < 0 it is sL×u (u = c×X^k×(1+…), sR = the
sign of c, sL = sR×(-1)^k); SQRT(u^2) is ABS(u) too. Each side's Taylor polynomial comes from the derivative route alone
(build/casim-ser0, the series route compiled out) at 524,280 B on the ABS-free rewrite. Where the sides agree to order n
that polynomial is the truth, and where they don't the truth is UNDEFINED. The answer under test is build/casim's, at
the calculator's 8,800 and 12,288 B; UNDECIDED, NO_ROOM and TOO_LONG are refusals, never wrong, and counted. Values
are compared exactly at n + 1 rational points (scripts/taylor-values.py's reader). The control: each value is judged
again against the right side made with sR's sign flipped (ABS's sign read wrong), and some must fail.
    scripts/abs-sides.py"""
import importlib.util
import itertools
import os
import subprocess
import sys
from fractions import Fraction as F

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "build", "casim")
OFF = os.path.join(ROOT, "build", "casim-ser0")
STATUSES = {"NO_ROOM", "UNDECIDED", "UNDEFINED", "TOO_LONG", "BAD_ARG", "NOT_DIFFERENTIABLE", "DECIMAL_FORM", "SYNTAX"}

_spec = importlib.util.spec_from_file_location("tv", os.path.join(ROOT, "scripts", "taylor-values.py"))
tv = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(tv)

# u, the sign of its leading coefficient, and its order at 0
U = [("X", 1, 1), ("-X", -1, 1), ("X+X^2", 1, 1), ("SIN(X)", 1, 1), ("ATAN(X)", 1, 1), ("X^3", 1, 3), ("2×X-X^3", 1, 1),
     ("X^2", 1, 2), ("-X^2", -1, 2), ("X-SIN(X)", 1, 3), ("EXP(X)-1", 1, 1), ("LN(1+X)", 1, 1), ("X^2+X^3", 1, 2)]
# the shapes: @ is ABS(u), # is SQRT(u^2)
G = ["@", "@^2", "@^3", "X×@", "X^2×@", "@×COS(X)", "SIN(@)", "SIN(@)^2", "ATAN(@)^2", "@^3+X^2", "EXP(@)",
     "@-#", "X×#", "@×@", "COS(@)", "@÷(1+X)", "SQRT(1+@)", "@^4÷(2+X)"]


def taylor(cli, text, n, arena):
    p = subprocess.run([cli, "--arena", str(arena), "taylor", text, "X", "0", str(n), "RAD"], stdout=subprocess.PIPE,
                       text=True, encoding="utf-8")
    return p.stdout.strip()


def same(a, b, n):
    """two polynomials in X of degree at most n, equal at n + 1 rational points"""
    try:
        return all(tv.value(a, F(2 * j + 1, 7)) == tv.value(b, F(2 * j + 1, 7)) for j in range(n + 1))
    except (ValueError, IndexError, ZeroDivisionError):
        return False


def side(g, s, u):
    return g.replace("@", f"(({s})×({u}))").replace("#", f"(({s})×({u}))")


def main():
    tally, failed, seen = {}, 0, 0
    for (u, c, k), g, n in itertools.product(U, G, range(5)):
        text = g.replace("@", f"ABS({u})").replace("#", f"SQRT(({u})^2)")
        sR, sL = c, c * (-1) ** k
        pR, pL = taylor(OFF, side(g, sR, u), n, 524280), taylor(OFF, side(g, sL, u), n, 524280)
        if pR in STATUSES or pL in STATUSES:
            tally["no truth"] = tally.get("no truth", 0) + 1
            continue
        both = same(pR, pL, n)
        pF = taylor(OFF, side(g, -sR, u), n, 524280)
        for arena in (8800, 12288):
            got = taylor(CLI, text, n, arena)
            if got in ("UNDECIDED", "NO_ROOM", "TOO_LONG"):
                tally[got] = tally.get(got, 0) + 1
                continue
            right = got == "UNDEFINED" if not both else got not in STATUSES and same(got, pR, n)
            kind = "value" if both else "UNDEFINED"
            tally[kind] = tally.get(kind, 0) + right
            if not right:
                failed += 1
                print(f"FAIL scripts/abs-sides.py: taylor {text} X 0 {n} (arena {arena}): {got}; the truth "
                      f"{pR if both else 'UNDEFINED'} (from above {pR}, from below {pL})")
            if both and right and got not in STATUSES and pF not in STATUSES and not same(got, pF, n):
                seen += 1                       # the control: a sign read wrong would have been refused here
    if not seen:
        failed += 1
        print("FAIL scripts/abs-sides.py: its control saw nothing (no value differs with ABS's sign flipped)")
    rows = len(U) * len(G) * 5
    print(f"ABS by its sides: {rows} rows at 8,800 and 12,288 B: {tally.get('value', 0)} values and "
          f"{tally.get('UNDEFINED', 0)} UNDEFINED right, {tally.get('UNDECIDED', 0)} UNDECIDED, "
          f"{tally.get('NO_ROOM', 0) + tally.get('TOO_LONG', 0)} NO_ROOM or TOO_LONG, {tally.get('no truth', 0)} rows "
          f"without a truth; the control: {seen} values ABS's sign flipped would refuse\n"
          f"checks: {2 * rows} failed: {failed}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
