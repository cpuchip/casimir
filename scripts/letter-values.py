#!/usr/bin/env python3
"""CAS 023's public judge (make check; work/023-conditions.md): letter rows for limit and taylor, each answer judged
numerically at sample values of K (Python floats). The answer must be right at every K its rows allow; and every value
a row K≠v excludes, or a row K>v or K<v leaves out, must be one where the answer's formula has no value or the truth
differs (no row for nothing). A limit's answer is compared with the text on both sides near the point (far out at ∞);
a taylor answer P with the text f by f - P at two small x, whose error must fall like x^(n+1). Controls: a row dropped,
a wrong row and a row on an answer right for every K must each fail.
    scripts/letter-values.py [CLI]"""
import math
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "casim")
KS = [0.0, 1.0, -1.0, 2.0, -2.0, 0.5, math.pi / 2, -math.pi / 2, math.pi, 3.0]

LIMITS = [  # (text, point, side)
    ("K×ABS(X)÷X", "0", ""), ("(K^2-1)×ABS(X)÷X", "0", ""), ("(K+X)÷X", "0", ""), ("(K+X)÷X", "0", "+"),
    ("K÷X", "0", "+"), ("SIN(K×X)÷X", "0", ""), ("SIN(K×X)÷(K×X)", "0", ""), ("SIN(X)÷(K×X)", "0", ""),
    ("(X-SIN(X))÷SIN(K×X)", "0", ""), ("X÷(K×X+X^2)", "0", ""), ("(EXP(K×X)-1)÷X", "0", ""),
    ("(EXP(X)-1)÷(K×X)", "0", ""), ("(1-COS(K×X))÷X^2", "0", ""), ("(K×X+X^2)÷(K×X)", "0", ""),
    ("X^2÷(K×X^2+X^3)", "0", ""), ("(K-1)×X÷X^2", "0", "+"), ("SIN(X)÷((K-1)×X)", "0", ""),
    ("(K×X^2+1)÷(X^2+1)", "∞", ""), ("(X^2+1)÷(K×X^2+1)", "∞", ""), ("(K×X+1)÷X", "∞", ""),
    ("X÷(K×X+1)", "∞", ""), ("(K×X^2+X)÷(2×X^2+1)", "∞", ""), ("SQRT(X^2+1)÷(K×X)", "∞", ""),
    ("(K+X)÷(K-X)", "0", ""), ("(X+K)^2÷(X+K)", "0", ""), ("COS(K×X)", "0", ""), ("K×X÷X", "0", ""),
    ("(K+X)÷(K-X)", "0", "+"), ("(K+X)÷(K-X)", "0", "-"), ("(K+X^2)÷(K-X)", "1", "+"), ("LN(K+X)", "0", ""),
    ("LN(K+SIN(X)÷X)", "0", ""), ("(K+SIN(X)÷X)^(1÷3)", "0", ""), ("X^K", "0", "+"), ("EXP(K×X)", "∞", ""),
    ("SIN(K×X)÷(K×X^2)", "0", "+"), ("SIN(K×X)÷(K×X)+K÷X", "0", "+"), ("(K×SIN(X)÷X)÷(K+X)", "0", ""),
    ("(1-COS(K×X))÷(K×X^3)", "0", "+"),
]
TAYLORS = [  # (text, point, order)
    ("1÷(K+X)", "0", 1), ("X÷(K+X)", "0", 2), ("1÷(K-1+X)", "0", 1), ("1÷(K^2+X)", "0", 1), ("1÷(K^2+1+X)", "0", 1),
    ("EXP(X)÷(K+X)", "0", 1), ("ABS(K+X)", "0", 1), ("TAN(K+X)", "0", 1), ("LN(K+X)", "0", 1), ("SQRT(K+X)", "0", 1),
    ("1÷(K×X+1)", "0", 2), ("EXP(K×X)", "0", 2), ("1÷(1+K×X)^2", "0", 1), ("1÷(K+X^2)", "0", 2), ("ABS(K×X+1)", "0", 1),
    ("1÷(K+SIN(X))", "0", 1), ("LN(K)+X", "0", 1), ("K÷X", "1", 1), ("1÷(X-K)", "1", 1), ("LN(1-K+X)", "0", 1),
    ("SQRT(K^2+X)", "0", 1), ("(K+X)^(1÷3)", "0", 1), ("K×X÷K", "0", 1), ("LN(K+X)÷(K+X)", "0", 1),
    ("LN(K+X)÷(K^2+X)", "0", 0), ("LN(2+X)+1÷(K+X)", "0", 0),
    ("ABS(K)×SQRT(1+X)", "0", 2), ("ABS(K)+X", "0", 1), ("ABS(K+X)", "0", 0),     # abacus #9525: no corner, no row
]
FN = {"SIN": "math.sin", "COS": "math.cos", "TAN": "math.tan", "LN": "math.log", "LOG": "math.log10",
      "EXP": "math.exp", "SQRT": "math.sqrt", "ABS": "abs", "SGN": "_sgn"}
REFUSED = ("UNDECIDED", "NO_ROOM", "TOO_LONG", "BAD_ARG", "NOT_DIFFERENTIABLE")


def _sgn(v):
    return (v > 0) - (v < 0)


def py(expr):
    s = expr.replace("×", "*").replace("÷", "/").replace("π", "math.pi")
    s = re.sub(r"\b(SIN|COS|TAN|LN|LOG|EXP|SQRT|ABS|SGN)\(", lambda m: FN[m.group(1)] + "(", s)
    return s.replace("^", "**")


def ev(expr, X, K):
    """expr's value, or None where it has none (a fractional power of a negative number is complex in Python, and has
    no value in Casimir: docs/language.md)"""
    try:
        v = eval(py(expr), {"math": math, "abs": abs, "_sgn": _sgn, "X": X, "K": K})
        return float(v) if isinstance(v, (int, float)) else None
    except (ZeroDivisionError, ValueError, OverflowError, TypeError, SyntaxError, NameError):
        return None


def casim(*args):
    p = subprocess.run([CLI] + list(args), capture_output=True, text=True, encoding="utf-8")
    return " / ".join(p.stdout.strip().split("\n"))


def near(a, b, tol):
    return a is not None and b is not None and abs(a - b) <= tol * max(1.0, abs(a), abs(b))


def split(ans):
    """the answer's text and its rows [(l, op, r)]"""
    parts = ans.split("; ")
    rows = []
    for row in parts[1:]:
        m = re.match(r"^(.*?)(≠|>|<)(.*)$", row)
        rows.append((m.group(1), m.group(2), m.group(3)) if m else (row, "?", ""))
    return parts[0], rows


def allowed(rows, K):
    """False when a row leaves this K out (or can't be valued there)"""
    for l, op, r in rows:
        a, b = ev(l, 0, K), ev(r, 0, K)
        if a is None or b is None or (near(a, b, 1e-9) if op == "≠" else (a <= b if op == ">" else a >= b)):
            return False
    return True


def lim_truth(text, point, side, K):
    """the limit's value, "+inf", "-inf", "none" (no limit, or no value near), or None (the probe can't tell)"""
    if point == "∞":
        vals = [[ev(text, x, K) for x in (1e5, 1e7)]]
    else:
        a = ev(point, 0, K)
        hs = (1e-5, 1e-7)
        sides = [1] if side == "+" else [-1] if side == "-" else [1, -1]
        vals = [[ev(text, a + s * h, K) for h in hs] for s in sides]
    lims = []
    for vs in vals:
        if any(v is None for v in vs):
            lims.append("none")
        elif abs(vs[1]) > 1e5 and abs(vs[1]) > 10 * abs(vs[0]):
            lims.append("+inf" if vs[1] > 0 else "-inf")
        elif near(vs[0], vs[1], 1e-3):
            lims.append(vs[1])
        else:
            return None
    if all(isinstance(l, str) for l in lims) and len(set(lims)) == 1:
        return lims[0]
    if all(not isinstance(l, str) for l in lims) and all(near(l, lims[0], 1e-3) for l in lims):
        return lims[0]
    return "none"


def lim_right(body, truth, K):
    """True or False, or None when the probe can't tell"""
    if truth is None:
        return None
    if body == "UNDEFINED":
        return truth == "none"
    if body == "∞" or body == "-∞":
        return truth == ("+inf" if body == "∞" else "-inf")
    v = ev(body, 0, K)
    return v is not None and not isinstance(truth, str) and near(v, truth, 1e-3)


def tay_right(text, point, n, body, K):
    a = ev(point, 0, K)
    if a is None:
        return None
    errs = []
    for h in (1e-2, 1e-3):
        f, p = ev(text, a + h, K), ev(body, a + h, K)
        if p is None or f is None:
            return False
        errs.append(abs(f - p))
    return errs[1] < 1e-11 or errs[0] / errs[1] > 10 ** (n + 1) / 3


def judge(kind, row, ans):
    """the failures of one answer: a wrong value where its rows allow K, a row that leaves out a K where it holds"""
    body, rows = split(ans)
    bad = []
    if body.split(" ")[0] in REFUSED:
        return bad if not rows else [f"rows on a refusal: {ans}"]
    def right(K):
        if kind == "limit":
            return lim_right(body, lim_truth(row[0], row[1], row[2], K), K)
        return tay_right(row[0], row[1], row[2], body, K) if body != "UNDEFINED" else None

    # every sampled K, and each K≠v row's own v
    for K in KS + [ev(r, 0, 0) for l, op, r in rows if op == "≠" and l == "K" and ev(r, 0, 0) is not None]:
        r_ = right(K)
        if allowed(rows, K):
            if r_ is False:
                bad.append(f"wrong at K = {K:.4g}")
        elif r_ is True and (body == "UNDEFINED" or ev(body, 0, K) is not None):
            bad.append(f"a row leaves out K = {K:.4g}, where the answer holds")
    return bad


def main():
    n = failed = 0
    for kind, rows in (("limit", LIMITS), ("taylor", TAYLORS)):
        for row in rows:
            args = [kind, row[0], "X", row[1], row[2] if kind == "limit" else str(row[2]), "RAD"]
            ans = casim(*args)
            n += 1
            for b in judge(kind, row, ans):
                failed += 1
                print(f"FAIL scripts/letter-values.py: {kind} {row[0]} at {row[1]}{row[2] if kind == 'limit' else ''}: "
                      f"{ans}: {b}")
    # the controls: each must fail
    controls = [("limit", ("SIN(K×X)÷(K×X)", "0", ""), "1"),                      # the row dropped
                ("taylor", ("1÷(K+X)", "0", 1), "1÷K-X÷K^2; K≠1"),                # a wrong row
                ("limit", ("SIN(K×X)÷X", "0", ""), "K; K≠2")]                     # a row on an answer right for every K
    for kind, row, ans in controls:
        n += 1
        if not judge(kind, row, ans):
            failed += 1
            print(f"FAIL scripts/letter-values.py: its control passed: {kind} {row[0]} as {ans}")
    print(f"letter values: {len(LIMITS)} limits and {len(TAYLORS)} taylors with letters, judged at {len(KS)} values of "
          f"K and at each row's excluded value; the 3 controls refused\nchecks: {n} failed: {failed}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
