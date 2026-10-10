#!/usr/bin/env python3
"""integrate's steps judged by value (make check; work/steps-integrate.md). Seeded random integrable sums go through
`build/casim --tsv steps integrate`, both ways, and each step is evaluated in Python's math (scripts/step-values.py's
evaluator, which shares no code with casim):
  - a result line, ∫(f)dX = F, and a CHECK line, d/dX(F) = f: F's derivative (a central difference) is f;
  - a template line, ∫(f)dX = T: T's derivative is f, taking each ∫(w)dX as w and a substitution
    ∫(H(U))dU÷a, U=L as H(L)×L'÷a (so a wrong a or L is seen);
  - the root's result (the last step) is plain integrate's answer, byte for byte; k runs 0..count-1; a second run is
    identical.
    scripts/istep-values.py [N] [SEED]     (default 600 expressions, seed 20261010)"""
import importlib.util
import os
import random
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "build", "casim")
spec = importlib.util.spec_from_file_location("stepvalues", os.path.join(ROOT, "scripts", "step-values.py"))
sv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sv)
POINTS = [0.37, 0.81, 1.23]
LIN = ["X", "2×X", "2×X+1", "3-X", "X+0.5", "0.5×X-1"]


def term(rnd):
    r, u = rnd.random(), rnd.choice(LIN)
    if r < 0.25:
        return rnd.choice(["", "2×", "3×", "0.5×", "-", "π×"]) + rnd.choice(["X", "X^2", "X^3", "X^(-2)", "SQRT(X)", "1"])
    if r < 0.55:
        return rnd.choice(["", "2×", "-3×", "0.5×"]) + rnd.choice(["SIN", "COS", "EXP", "SINH", "COSH", "ALOG"]) + "(" + u + ")"
    if r < 0.7:
        return rnd.choice(["1÷(" + u + ")", "3÷(" + u + ")", "(" + u + ")^3", "SQRT(" + u + ")", "(" + u + ")^(-2)"])
    if r < 0.85:
        return "(" + rnd.choice(LIN) + ")×(" + rnd.choice(LIN) + ")"
    return rnd.choice(["2×(", "-(", "(", "0.5×("]) + term(rnd) + "+" + term(rnd) + ")"


def deriv_value(expr, x, deg):
    return sv.deriv(expr, x, deg)


def template_derivative(t, x, deg):
    """T's derivative at x: each ∫(w)dX as w, each ∫(H)dU÷a, U=L as H(L)×L'(x)÷a; None where undefined"""
    m = re.fullmatch(r"(-?)∫\((.*)\)d([UV])(?:÷(.+?))?, ([UV])=(.*)", t)
    if m:
        minus, h, letter, a, _, l = m.groups()
        a = (sv.value(a, x, deg) if a else 1.0) * (-1 if minus else 1)
        dl = sv.deriv(l, x, deg)
        lv = sv.value(l, x, deg)
        if a is None or dl is None or lv is None:
            return None
        hv = sv.value(h.replace(letter, "(" + repr(lv) + ")"), x, deg)
        return None if hv is None else hv * dl / a
    out, i = "", 0
    while i < len(t):
        if t.startswith("∫(", i):
            depth, j = 0, i + 1
            for j in range(i + 1, len(t)):
                depth += {"(": 1, ")": -1}.get(t[j], 0)
                if depth == 0:
                    break
            out += "(" + t[i + 2:j] + ")"
            i = j + 3                   # past ")dX"
        else:
            out += t[i]
            i += 1
    return sv.value(out, x, deg)


def split_int(text):
    """'∫(f)dX = rest' as (f, rest)"""
    depth = 0
    for i in range(1, len(text)):
        depth += {"(": 1, ")": -1}.get(text[i], 0)
        if depth == 0:
            break
    return text[2:i], text[i + 3:].removeprefix(" = ")


def wrong(rule, line, text, deg):
    judged = 0
    for x in POINTS:
        if rule == "CHECK":
            f_side = text.split(") = ", 1)
            F, f = f_side[0][len("d/dX("):], f_side[1]
            d, v = sv.deriv(F, x, deg), sv.value(f, x, deg)
        else:
            f, rest = split_int(text)
            v = sv.value(f, x, deg)
            d = sv.deriv(rest, x, deg) if line == "R" else template_derivative(rest, x, deg)
        if d is None or v is None:
            continue
        judged += 1
        if not sv.close(d, v):
            return f"at X={x}: the step's derivative {d!r}, the integrand {v!r}"
    return ""


BIG = ["--arena", "524280"]         # values judged with room to spare; the 8 KB budget's soft NO_ROOM counted apart


def steps(e, deg, one, arena=BIG):
    args = [CLI, "--tsv"] + arena + (["--one-line"] if one else []) + ["steps", "integrate", e, "X", "DEG" if deg else "RAD"]
    p = subprocess.run(args, stdout=subprocess.PIPE, text=True, encoding="utf-8")
    return p.returncode, p.stdout


def main():
    for rule, line, text in [("SIN", "R", "∫(SIN(2×X))dX = -COS(2×X)"),
                             ("SIN", "T", "∫(SIN(2×X+1))dX = ∫(SIN(U))dU÷3, U=2×X+1"),
                             ("CHECK", "R", "d/dX(X^3÷3) = X^3")]:
        if not wrong(rule, line, text, False):
            print(f"FAIL scripts/istep-values.py: its judge accepts {text}")
            sys.exit(1)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    rnd = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 20261010)
    failed = lines = ok = soft = 0
    for _ in range(n):
        e = "+".join(term(rnd) for _ in range(rnd.randint(1, 3))).replace("+-", "-")
        deg = rnd.random() < 0.2
        p = subprocess.run([CLI] + BIG + ["integrate", e, "X", "DEG" if deg else "RAD"], stdout=subprocess.PIPE,
                           text=True, encoding="utf-8")
        if p.returncode != 0:
            continue                    # only what integrate answers has steps to judge
        ok += 1
        plain = p.stdout.strip()
        small = subprocess.run([CLI, "integrate", e, "X", "DEG" if deg else "RAD"], stdout=subprocess.PIPE).returncode
        if small == 0 and steps(e, deg, False, [])[0] != 0:
            soft += 1                   # fits the 8 KB budget plain, not with its steps: NO_ROOM, softly
        for one in (False, True):
            rc, out = steps(e, deg, one)
            why = []
            if rc != 0:
                why.append(f"steps exit {rc}: {out.strip()[:80]}")
            else:
                rows = [r.split("\t") for r in out.rstrip("\n").split("\n")]
                if [int(r[0]) for r in rows] != list(range(len(rows))):
                    why.append("k is not 0..count-1")
                if rows[-1][1] != "0" or rows[-1][2] != "R" or rows[-1][3] == "CHECK" or split_int(rows[-1][4])[1] != plain:
                    why.append(f"the last step {rows[-1][4]} is not the root's result {plain}")
                if steps(e, deg, one)[1] != out:
                    why.append("a second run differs")
                for r in rows:
                    lines += 1
                    w = wrong(r[3], r[2], r[4], deg)
                    if w:
                        why.append(f"step {r[0]} {r[2]} {r[3]}: {r[4]}: {w}")
            if why:
                failed += 1
                print(f"FAIL scripts/istep-values.py: {e} ({'DEG' if deg else 'RAD'}, "
                      f"{'one line' if one else 'two lines'})\n  " + "\n  ".join(why))
    print(f"integrate step values: {ok} of {n} expressions integrable, both ways, {lines} steps; {soft} fit 8 KB plain "
          f"but not with steps; the judge refused its three planted steps\nchecks: {2 * ok} failed: {failed}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
