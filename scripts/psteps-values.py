#!/usr/bin/env python3
"""partfrac's steps judged exactly (make check; work/steps-partfrac.md). The visible examples and the seeded fractions
of scripts/partfrac-values.py go through `build/casim --tsv steps partfrac` (and --one-line); every page's sides are
evaluated here with exact fractions, by eval of the text with the unknown letters bound, sharing no code with casim:
  - COMMON FACTOR, POLYNOMIAL PART, FACTOR, FORM and CLEARED: both sides equal at five points (FORM and CLEARED with
    the letters bound to the values the result pages give);
  - COVER-UP: X=a is a root of CLEARED's cofactor structure: its value is CLEARED's left side at a, its coefficient
    is CLEARED's right side at a with that letter 1 and the others 0, and the result is value ÷ coefficient;
  - COEFFICIENTS: each template is exactly the X^m coefficient of CLEARED (left minus right, an affine form in the
    letters, the cover-up values put in), one for every power below deg d, and the result satisfies all of them;
  - the order of the pages, their depths, T before R; the letters A, B, ... skipping the variable;
  - the last page is partfrac's own answer byte for byte (PARTIAL as partfrac's), the one-line pages are the results;
  - planted faults, each one page wrong in one way, are refused, and the right pages accepted.
    scripts/psteps-values.py [N] [SEED]      (default 300 seeded fractions, seed 20261012)"""
import importlib.util
import os
import random
import re
import subprocess
import sys
from fractions import Fraction as F

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "build", "casim")
spec = importlib.util.spec_from_file_location("pfvalues", os.path.join(ROOT, "scripts", "partfrac-values.py"))
pv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pv)
POINTS = [F(3, 7), F(-5, 11), F(13, 9), F(17, 5), F(-29, 13)]
ORDER = ["COMMON FACTOR", "POLYNOMIAL PART", "FACTOR", "FORM", "CLEARED", "COVER-UP", "COEFFICIENTS", "ANSWER"]


def ev(text, env):
    """text's value with the letters in env; numbers exact (Q_ is the fraction, so no letter can shadow it)"""
    t = text.replace("×", "*").replace("÷", "/").replace("^", "**")
    t = re.sub(r"(\d+)", r"Q_(\1)", t)
    return eval(t, {"__builtins__": {}, "Q_": F, **env})


def run(*args):
    p = subprocess.run([CLI, *args], stdout=subprocess.PIPE, text=True, encoding="utf-8")
    return p.stdout.rstrip("\n")


def pages_of(text, one=False):
    out = run(*(["--one-line"] if one else []), "--tsv", "steps", "partfrac", text, "X", "RAD").split("\n")
    rows, status = [], "OK"
    for line in out:
        f = line.split("\t")
        if len(f) == 5:
            rows.append((int(f[1]), f[2], f[3], f[4]))
        elif line:
            status = line
    return rows, status


def coef_m(f, m, deg=12):
    """the X^m coefficient of f(x), f a polynomial of degree <= deg, by Lagrange through deg+1 points"""
    xs = [F(i) for i in range(deg + 1)]
    ys = [f(x) for x in xs]
    d = ys[:]
    for j in range(1, len(xs)):
        for i in range(len(xs) - 1, j - 1, -1):
            d[i] = (d[i] - d[i - 1]) / (xs[i] - xs[i - j])
    c, basis = [F(0)] * (deg + 2), [F(1)]
    for j in range(len(xs)):
        for k, b in enumerate(basis):
            c[k] += d[j] * b
        nb = [F(0)] * (len(basis) + 1)
        for k, b in enumerate(basis):
            nb[k + 1] += b
            nb[k] -= xs[j] * b
        basis = nb
    return c[m]


def judge_pages(text, rows, status, plain):
    """the reasons the pages are wrong; [] when right"""
    why = []
    if not rows:
        return [f"no pages ({status})"] if plain.split(" ")[0] not in ("NO_ROOM", "TOO_LONG", "NOT_POLYNOMIAL",
                                                                      "NOT_NUMERIC", "UNDEFINED", "BAD_ARG", "SYNTAX") else []
    want = plain[8:] if plain.startswith("PARTIAL ") else plain
    if rows[-1][2] != "ANSWER" or rows[-1][3] != want:
        why.append(f"last page {rows[-1][2]}: {rows[-1][3]} is not partfrac's {want}")
    if (status == "PARTIAL") != plain.startswith("PARTIAL "):
        why.append(f"status {status} against partfrac's {plain[:8]}")
    # order, depths, T before R
    seen = [ORDER.index(r) for _, _, r, _ in rows]
    if seen != sorted(seen):
        why.append("pages out of order: " + ", ".join(r for _, _, r, _ in rows))
    for (d, line, rule, t) in rows:
        if d != (1 if rule in ("COVER-UP", "COEFFICIENTS") else 0):
            why.append(f"{rule} at depth {d}")
    # the result pages' values
    vals = {}
    for (_, line, rule, t) in rows:
        if line == "R" and rule in ("COVER-UP", "COEFFICIENTS"):
            for part in t.split("; "):
                u, v = part.split("=")
                vals[u] = ev(v, {})
    letters = sorted(vals)
    expect = [c for c in "ABCDEFGHIJKLMNOPQRSTUVWYZ"][:len(letters)]
    if letters != sorted(expect):
        why.append(f"letters {letters}, not {expect}")
    cleared = next((t for (_, _, r, t) in rows if r == "CLEARED"), None)
    for (_, line, rule, t) in rows:
        try:
            if rule in ("COMMON FACTOR", "POLYNOMIAL PART", "FACTOR", "FORM", "CLEARED"):
                eq = t.split("; ")[0]
                lhs, rhs = eq.split("=", 1)
                for x in POINTS:
                    try:
                        if ev(lhs, {"X": x}) != ev(rhs, {"X": x, **vals}):
                            why.append(f"{rule}: {t} at X={x}")
                            break
                    except ZeroDivisionError:
                        continue
                if rule == "COMMON FACTOR" and t.split("; ", 1)[1:] != want.split("; ", 1)[1:]:
                    why.append(f"COMMON FACTOR's rows {t} against the answer's {want}")
            elif rule == "COVER-UP" and line == "T":
                at, eq = t.split(": ", 1)
                a = ev(at.split("=")[1], {})
                lhs, rhs = eq.split("=", 1)
                u = re.findall(r"[A-WYZ]", rhs)[-1]
                cl, cr = cleared.split("=", 1)
                zero = {k: F(0) for k in vals}
                if ev(lhs, {}) != ev(cl, {"X": a}):
                    why.append(f"COVER-UP {t}: the value is not CLEARED's left side at {a}")
                if ev(rhs, {u: F(1)}) != ev(cr, {"X": a, **zero, u: F(1)}) or ev(cr, {"X": a, **zero}) != 0:
                    why.append(f"COVER-UP {t}: the coefficient is not CLEARED's at {a}")
            elif rule == "COVER-UP" and line == "R":
                pass                                    # checked below with its template
            elif rule == "COEFFICIENTS" and line == "T":
                xm, eq = t.split(": ", 1)
                m = 0 if xm == "X^0" else 1 if xm == "X" else int(xm.split("^")[1])
                lhs, rhs = eq.split("=", 1)
                cl, cr = cleared.split("=", 1)
                known = {u: v for u, v in vals.items() if u in covered}
                rest = [u for u in vals if u not in covered]
                # the affine form left - Σ c U against coefficient_m(CLEARED left - right), at 0 and each unit vector
                for probe in [{}] + [{u: F(1)} for u in rest]:
                    env = {**{u: F(0) for u in rest}, **probe}
                    mine = ev(lhs, {}) - ev(rhs, env)
                    full = coef_m(lambda x: ev(cl, {"X": x}) - ev(cr, {"X": x, **known, **env}), m)
                    if mine != full:
                        why.append(f"COEFFICIENTS {t}: not CLEARED's X^{m} coefficient")
                        break
                if ev(lhs, {}) != ev(rhs, vals):
                    why.append(f"COEFFICIENTS {t}: the result does not satisfy it")
        except (ValueError, IndexError, SyntaxError, NameError, TypeError) as e:
            why.append(f"{rule}: {t} unreadable ({e})")
    return why


covered = set()


def judge(text):
    global covered
    rows, status = pages_of(text)
    plain = run("partfrac", text, "X", "RAD")
    covered = {re.findall(r"[A-WYZ]", t.split("=", 1)[1])[-1]
               for (_, line, rule, t) in rows if rule == "COVER-UP" and line == "T"}
    why = judge_pages(text, rows, status, plain)
    one, _ = pages_of(text, one=True)
    if [r for r in one] != [r for r in rows if r[1] == "R"]:
        why.append("the one-line pages are not the result pages")
    # the COVER-UP results: value ÷ coefficient, in the template's order
    t_rows = [r for r in rows if r[2] == "COVER-UP"]
    for t, r in zip(t_rows[0::2], t_rows[1::2]):
        eq = t[3].split(": ", 1)[1]
        lhs, rhs = eq.split("=", 1)
        u = re.findall(r"[A-WYZ]", rhs)[-1]
        if r[3] != f"{u}={r[3].split('=')[1]}" or ev(r[3].split("=")[1], {}) * ev(rhs, {u: F(1)}) != ev(lhs, {}):
            why.append(f"COVER-UP {t[3]} then {r[3]}")
    # every power of X below deg d has its equation, when coefficients are equated
    co = [r for r in rows if r[2] == "COEFFICIENTS" and r[1] == "T"]
    form = next((r[3] for r in rows if r[2] == "FORM"), None)
    if co and form:
        lhs = form.split("=", 1)[0]
        den = pv.ssv.top_split(lhs, "÷")[-1]
        deg = len(pv.ssv.coeffs(den)) - 1
        if len(co) != deg:
            why.append(f"{len(co)} COEFFICIENTS equations for a denominator of degree {deg}")
    return why


def planted():
    """one page wrong in one way: each must be refused, and the right pages accepted"""
    global covered
    text = "(X^4+1)÷(X^3+1)"
    rows, status = pages_of(text)
    plain = run("partfrac", text, "X", "RAD")
    covered = {"A"}

    def with_page(rule, line, new, which=0):
        out, seen = [], 0
        for r in rows:
            if r[2] == rule and r[1] == line:
                if seen == which:
                    r = (r[0], r[1], r[2], new)
                seen += 1
            out.append(r)
        return out

    faults = {
        "a cover-up value": with_page("COVER-UP", "R", "A=1÷3"),
        "a cover-up coefficient": with_page("COVER-UP", "T", "X=-1: 2=2×A"),
        "an equation's coefficient": with_page("COEFFICIENTS", "T", "X: -1÷3=2×B+C", 1),
        "an equation's value": with_page("COEFFICIENTS", "T", "X^0: 2÷3=C", 2),
        "the results": with_page("COEFFICIENTS", "R", "B=-2÷3; C=2÷3"),
        "the form": with_page("FORM", "R", "(-X+1)÷(X^3+1)=A÷(X+1)+B÷(X^2-X+1)"),
        "the cleared form": with_page("CLEARED", "R", "-X+1=A×(X^2-X+1)+(B×X+C)"),
        "the polynomial part": with_page("POLYNOMIAL PART", "R", "(X^4+1)÷(X^3+1)=X+(X+1)÷(X^3+1)"),
        "the factors": with_page("FACTOR", "R", "X^3+1=(X+1)×(X^2+X+1)"),
        "the order": [rows[1], rows[0]] + rows[2:],
        "the answer": rows[:-1] + [(0, "R", "ANSWER", "X+1÷(X+1)")],
    }
    caught, bad = 0, []
    if judge_pages(text, rows, status, plain):
        bad.append("the right pages were refused: " + "; ".join(judge_pages(text, rows, status, plain)))
    for name, fault in faults.items():
        if fault == rows:
            bad.append(f"planted {name}: no fault made")
        elif judge_pages(text, fault, status, plain):
            caught += 1
        else:
            bad.append(f"planted {name}: accepted")
    return caught, bad


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    rnd = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 20261012)
    caught, bad = planted()
    texts = [t for t, _, _ in pv.examples()] + [pv.seeded(rnd)[0] for _ in range(count)]
    fails, judged, pages = [], 0, 0
    for text in texts:
        why = judge(text)
        judged += 1
        if why:
            fails.append(f"{text}\n     " + "\n     ".join(why[:4]))
        pages += len(pages_of(text)[0])
    for f in fails[:10]:
        print("FAIL partfrac steps " + f)
    for b in bad:
        print("FAIL scripts/psteps-values.py: " + b)
    if fails or bad:
        sys.exit(1)
    print(f"psteps-values: {judged} fractions' steps ({pages} pages) judged exactly; the judge refused its {caught} "
          f"planted faults")


if __name__ == "__main__":
    main()
