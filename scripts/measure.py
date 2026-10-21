#!/usr/bin/env python3
"""What a derivative costs, through the host CLI (build/casim): the smallest arena each case needs
(--min-arena; a node is 24 bytes on the host and on the ESP32-S3, so the figure holds on the chip),
and the derivative's length against the equation's, in characters as the calculator shows them.

The cases: unit CAS 001's visible examples, and equations of about 100 characters (the unit's
budget case) shaped to make the derivative large: long products, quotients, powers and nests of
functions in DEG, where every level carries the angle factor.
"""
import os
import shlex
import statistics
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "build", "casim")


# CAS 002b: probes past the depth guard, one for each kind of recursion (longer than 192 bytes: the guard
# must bound any input the API takes). Each must return TOO_LONG or OK, never more than 12 KB of stack.
PROBES = {
    "probe X^( nest": ("X^(" * 150 + "X" + ")" * 150, "RAD"),
    "probe SIN nest": ("SIN(" * 150 + "X" + ")" * 150, "DEG"),
    "probe SQ nest": ("SQ(" * 150 + "X" + ")" * 150, "RAD"),
    "probe X+Y×( nest": ("X+Y×(" * 150 + "X" + ")" * 150, "RAD"),
    "probe X^X^ chain 400": ("^".join(["X"] * 400), "RAD"),
    "probe deep compare": ("SIN(" * 100 + "X" + ")" * 100 + "+" + "SIN(" * 100 + "Y" + ")" * 100, "RAD"),
    "probe -(X+( nest": ("-(X+" * 150 + "X" + ")" * 150, "RAD"),
    "probe SQRT(1+ nest": ("SQRT(1+" * 150 + "X" + ")" * 150, "RAD"),
    "probe 1÷(1+1÷( nest": ("1÷(1+" * 150 + "X" + ")" * 150, "RAD"),
}


# The sweep: each nested shape at depths around the guard, so some case of every kind reaches it exactly
# where its walk is deepest (a probe far past the guard stops early, in the canonical form, and would not
# show the derivative's deeper recursion).
SWEEP = {}
for _n in range(36, 58, 2):
    SWEEP[f"sweep X^( {_n}"] = ("X^(" * _n + "X" + ")" * _n, "RAD")
    SWEEP[f"sweep SIN {_n}"] = ("SIN(" * _n + "X" + ")" * _n, "DEG")
    SWEEP[f"sweep X+Y×( {_n}"] = ("X+Y×(" * _n + "X" + ")" * _n, "RAD")
    SWEEP[f"sweep 1÷(1+ {_n}"] = ("1÷(1+" * _n + "X" + ")" * _n, "RAD")
    SWEEP[f"sweep SQRT(1+ {_n}"] = ("SQRT(1+" * _n + "X" + ")" * _n, "RAD")
    SWEEP[f"sweep -(X+ {_n}"] = ("-(X+" * _n + "X" + ")" * _n, "RAD")


def chain(op, n):
    return op.join(["X"] * n)


def nest(f, n, inner="X"):
    return f + "(" if n == 0 else f"{f}(" * n + inner + ")" * n


def to_len(make, limit=100):
    """The largest n whose make(n) has at most limit characters."""
    n = 1
    while len(make(n + 1)) <= limit:
        n += 1
    return make(n)


def to_bytes(make, limit=191):
    """The largest n whose make(n) fits the core's equation slot: AB_EQN_LEN, 192 bytes with the NUL."""
    n = 1
    while len(make(n + 1).encode()) <= limit:
        n += 1
    return make(n)


# CAS 001's budget case: about 100 characters, shaped to make the derivative large.
SHAPES = {
    "product X×X×…": (to_len(lambda n: chain("×", n)), "RAD"),
    "quotient X÷X÷…": (to_len(lambda n: chain("÷", n)), "RAD"),
    "power X^X^…": (to_len(lambda n: chain("^", n)), "RAD"),
    "SIN nest, DEG": (to_len(lambda n: "SIN(" * n + "X" + ")" * n), "DEG"),
    "ASIN nest, DEG": (to_len(lambda n: "ASIN(" * n + "X" + ")" * n), "DEG"),
    "TAN nest, GRAD": (to_len(lambda n: "TAN(" * n + "X" + ")" * n), "GRAD"),
    "SQRT(X)÷… quotients": (to_len(lambda n: "÷".join(["SQRT(X)"] * n)), "RAD"),
    "(X+1)÷(X-1)×…": (to_len(lambda n: "×".join(["(X+1)÷(X-1)"] * n)), "RAD"),
    "X×SIN(X)÷COS(X)^X…": (to_len(lambda n: "+".join(["X×SIN(X)÷COS(X)^X"] * n)), "DEG"),
    "equation, mixed": (to_len(lambda n: "+".join(["SQ(ASIN(X)-SIN(Y))÷(0.25+SQRT(X))"] * n) + "=X^X"), "DEG"),
}

# CAS 002's budget case: a full equation slot, 191 bytes, at the nesting and chain shapes that cost
# stack and arena the most (measured for CAS 001's parser: 24.6 KB for the parentheses, 30.7 KB for the
# minus signs), and realistic mixed ones.
SHAPES192 = {
    "192 parens": (to_bytes(lambda n: "(" * n + "X" + ")" * n), "RAD"),
    "192 minus": (to_bytes(lambda n: "-" * n + "X"), "RAD"),
    "192 -( nest": (to_bytes(lambda n: "-(" * n + "X" + ")" * n), "RAD"),
    "192 SQ nest": (to_bytes(lambda n: "SQ(" * n + "X" + ")" * n), "RAD"),
    "192 SIN nest, DEG": (to_bytes(lambda n: "SIN(" * n + "X" + ")" * n), "DEG"),
    "192 X^(X^( nest": (to_bytes(lambda n: "X^(" * n + "X" + ")" * n), "RAD"),
    "192 X^X^… chain": (to_bytes(lambda n: chain("^", n)), "RAD"),
    "192 X×Y×… product": (to_bytes(lambda n: "×".join("XY"[i % 2] for i in range(n))), "RAD"),
    "192 X÷Y÷… quotient": (to_bytes(lambda n: "÷".join("XY"[i % 2] for i in range(n))), "RAD"),
    "192 X+Y-… sum": (to_bytes(lambda n: "".join(("+" if i % 2 else "-") + "XY"[i % 3 % 2] for i in range(n))[1:]), "RAD"),
    "192 (X+1)÷(X-1)×…": (to_bytes(lambda n: "×".join(["(X+1)÷(X-1)"] * n)), "RAD"),
    "192 mixed, DEG": (to_bytes(lambda n: "+".join(["SQ(ASIN(X)-SIN(Y))÷(0.25+SQRT(X))"] * n) + "=X"), "DEG"),
    "192 pairs S0005+…": (to_bytes(lambda n: "+".join(["(Y)-((((Y)^3)×((Y)+(X)+(X)))×(SQRT((1)-(X))))"] * n)), "RAD"),
}


def cli_args(expr, var, angle, op, repl=None, model="35"):
    """The CLI's arguments for one case (model: the text's stamp, CAS 004; the replacement's too)."""
    m = ["--model", model, "--repl-model", model]
    if op in ("derive", "collect", "solve", "integrate", "factor", "cancel", "partfrac"):
        return m + [op, expr, var, angle]
    if op in ("subst", "pdiv", "pgcd"):
        return m + [op, expr, var, repl, angle]
    if op in ("defint", "taylor", "limit", "sum"):     # CAS 009, 018: repl holds "lo|hi"; 014: "point|n"; 015: "point|side"
        return m + [op, expr, var] + repl.split("|", 1) + [angle]
    return m + [op, expr, angle]


# CAS 003: 191-byte and short-but-heavy inputs for expand, collect and subst (the unit's budgets are for a
# 192-byte input; a short power can cost more than a long text).
def _upto(make, limit=191):
    n = 1
    while len(make(n + 1).encode()) <= limit:
        n += 1
    return make(n)


SHAPES003 = [
    ("e (X+1)^20", "(X+1)^20", "X", "RAD", "expand", None),
    ("e (X+Y)^10", "(X+Y)^10", "X", "RAD", "expand", None),
    ("e (X+Y+Z+1)^3", "(X+Y+Z+1)^3", "X", "RAD", "expand", None),
    ("e 191 binomials", _upto(lambda n: "×".join(f"(X+{i % 9 + 1})" for i in range(n))), "X", "RAD", "expand", None),
    ("e 191 sum of products", _upto(lambda n: "+".join(f"(X+{i % 9 + 1})×(Y-{i % 7 + 1})" for i in range(n))), "X",
     "RAD", "expand", None),
    ("e 191 SIN of squares", _upto(lambda n: "+".join(f"SIN((X+{i % 9 + 1})^2)" for i in range(n))), "X", "RAD",
     "expand", None),
    ("e 191 quotients", _upto(lambda n: "+".join(f"X÷((X+{i % 9 + 1})×(X-{i % 7 + 1}))" for i in range(n))), "X",
     "RAD", "expand", None),
    ("c (X+SIN(Y))^8", "(X+SIN(Y))^8", "X", "RAD", "collect", None),
    ("c 191 products", _upto(lambda n: "+".join(f"(X+Y+{i % 9 + 1})×(X-{i % 7 + 1})" for i in range(n))), "X",
     "RAD", "collect", None),
    ("c 191 with a remainder", _upto(lambda n: "+".join(f"(X+{i % 9 + 1})^2+SIN(X×{i % 5 + 1})" for i in range(n))),
     "X", "RAD", "collect", None),
    ("u 191 powers", _upto(lambda n: "+".join(f"X^{i % 6 + 1}×{i % 9 + 1}" for i in range(n))), "X", "RAD", "subst",
     "Y+1"),
    ("u 191 nested", _upto(lambda n: "SIN(" * n + "X" + ")" * n), "X", "RAD", "subst", "X^2+Y"),
]


# CAS 003 review: expand's and collect's walks at depth, swept across the guard (each nest wraps a square
# of a sum, so the deepest level still has an expansion to do).
for _n in range(36, 58, 4):
    SHAPES003.append((f"e sweep IP( {_n}", "IP(" * _n + "(X+1)^2" + ")" * _n, "X", "RAD", "expand", None))
    SHAPES003.append((f"e sweep X×(Y+ {_n}", "X×(Y+" * _n + "(X+1)^2" + ")" * _n, "X", "RAD", "expand", None))
    SHAPES003.append((f"c sweep X×(Y+ {_n}", "X×(Y+" * _n + "(X+1)^2" + ")" * _n, "X", "RAD", "collect", None))
    SHAPES003.append((f"e sweep SIN( {_n}", "SIN(" * _n + "(X+1)^2" + ")" * _n, "X", "RAD", "expand", None))


def run(expr, var, angle, op="derive", repl=None, model="35"):
    args = cli_args(expr, var, angle, op, repl, model)
    p = subprocess.run([CLI, "--arena", "65536", "--min-arena"] + args,
                       stdout=subprocess.PIPE, text=True)
    lines = p.stdout.splitlines()
    if p.returncode != 0:
        return None, None, lines[0] if lines else "?"
    return int(lines[1].split()[1]), lines[0], None


def all_cases():
    """(name, expression, variable, angle, op) for every case the measurements and the bench use."""
    rows = []
    with open(os.path.join(ROOT, "work", "001-examples.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            parts = [p.strip() for p in line.split(" | ")]
            rows.append((parts[0], parts[1], parts[2], parts[3], "derive"))
    for name, (expr, angle) in SHAPES.items():
        rows.append((name, expr, "X", angle, "derive"))
    with open(os.path.join(ROOT, "work", "002-examples.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            parts = [p.strip() for p in line.split(" | ")]
            rows.append((parts[0], parts[1], "X", parts[3], "simplify"))
    for name, (expr, angle) in list(SHAPES192.items()) + list(PROBES.items()) + list(SWEEP.items()):
        rows.append((name + " S", expr, "X", angle, "simplify"))
        rows.append((name + " D", expr, "X", angle, "derive"))
    rows = [r + (None,) for r in rows]
    with open(os.path.join(ROOT, "work", "003-examples.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            p = [x.strip() for x in line.split(" | ")]
            if p[1] == "expand":
                rows.append((p[0], p[2], "X", p[4], "expand", None))
            elif p[1] == "collect":
                rows.append((p[0], p[2], p[3], p[4], "collect", None))
            elif p[1] == "subst":
                rows.append((p[0], p[2], p[3], p[5], "subst", p[4]))
    # CAS 005: solve, its visible examples and the prototype's slow shapes (high multiplicities, a divisor-rich
    # leading coefficient, twelve quadratic roots)
    with open(os.path.join(ROOT, "work", "005-examples.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            p = [x.strip() for x in line.split(" | ")]
            rows.append((p[0], p[2], p[3], "RAD", "solve", None, p[4]))
    for name, expr in [("v (X^2+X+6)^6", "(X^2+X+6)^6"), ("v (X-1)^12", "(X-1)^12"),
                       ("v (2X-3)^4(X^2-2)^4", "(2×X-3)^4×(X^2-2)^4"),
                       ("v divisor-rich lead", "8648640×X^12+X^11+X+8648640"),
                       ("v X^12-1", "X^12-1"), ("v X^12-2", "X^12-2"), ("v X^4-10X^2+1", "X^4-10×X^2+1"),
                       ("v (X-1)..(X-9)", "(X-1)×(X-2)×(X-3)×(X-4)×(X-5)×(X-6)×(X-7)×(X-8)×(X-9)"),
                       ("v (2X^2+X+3)^6", "(2×X^2+X+3)^6")]:
        rows.append((name, expr, "X", "RAD", "solve", None, "35"))
    # CAS 006: integrate, its visible examples and the heaviest forms
    with open(os.path.join(ROOT, "work", "006-examples.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            p = [x.strip() for x in line.split(" | ")]
            rows.append((p[0], p[2], p[3], p[4], "integrate", None, p[5]))
    for name, expr, angle in [("i poly deg 11", "+".join(f"{k + 1}×X^{k}" for k in range(12)), "RAD"),
                              ("i (X+1)^5×(X-1)^6", "(X+1)^5×(X-1)^6", "RAD"),
                              ("i seven forms DEG", "SIN(2×X)+COS(3×X+1)+TAN(X÷2)+EXP(X)+ALOG(2×X)+SINH(X)+1÷(2×X+1)", "DEG"),
                              ("i (2X+1)^(5÷3)", "(2×X+1)^(5÷3)", "RAD")]:
        rows.append((name, expr, "X", angle, "integrate", None, "35"))
    # CAS 007: factor, its visible examples and the heaviest forms (the exhaustive monic degree 12, a
    # twelve-real-root input that spends the whole budget, repeated factors of every degree)
    with open(os.path.join(ROOT, "work", "007-examples.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            p = [x.strip() for x in line.split(" | ")]
            rows.append((p[0], p[2], p[3], "RAD", "factor", None, p[4]))
    for name, expr in [("f prod(X-i)-1, i=-5..6", "×".join(f"(X{-i:+d})" if i else "X" for i in range(-5, 7)) + "-1"),
                       ("f budget spent", "129600×X^12-440640×X^11-696096×X^10+3529728×X^9-155628×X^8-9280476×X^7"
                        "+5112900×X^6+9705420×X^5-7759512×X^4-3970224×X^3+3990816×X^2+456192×X-622083"),
                       ("f window decides", "1152000×X^12+960000×X^11-3160320×X^10-2505600×X^9+3068160×X^8+2248320×X^7"
                        "-1278720×X^6-827520×X^5+234240×X^4+132480×X^3-15360×X^2-7680×X-2"),
                       ("f divisor-rich lead", "8648640×X^12+873×X^10-747×X^4+2"),
                       ("f X^12-1", "X^12-1"), ("f (X-1)^12", "(X-1)^12"), ("f (X^3-2)^4", "(X^3-2)^4"),
                       ("f (X^2+X+6)^6", "(X^2+X+6)^6"), ("f (X^6+X+1)^2", "(X^6+X+1)^2"),
                       ("f 12 linear", "×".join(f"(X{-i:+d})" if i else "X" for i in range(-5, 7)))]:
        rows.append((name, expr, "X", "RAD", "factor", None, "35"))
    # CAS 008: exact values, its visible examples and a mixed constant expression in each mode
    with open(os.path.join(ROOT, "work", "008-examples.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            p = [x.strip() for x in line.split(" | ")]
            rows.append((p[0], p[2], "X", p[3], "simplify", None, p[4]))
    for angle, expr in [("RAD", "SIN(π÷4)×COS(π÷3)+TAN(π÷3)^2+SQRT(0.25)-8^(1÷3)+ATAN(1)"),
                        ("DEG", "SIN(30)+COS(225)×TAN(60)+ASIN(-0.5)+EXP(3×LN(2))"),
                        ("GRAD", "COS(50)^2+SIN(100÷3)+ACOS(-1)+4^(3÷2)")]:
        rows.append((f"x mixed {angle}", expr, "X", angle, "simplify", None, "35"))
    # CAS 009: definite integrals, its visible examples and the heaviest forms (a 12-term polynomial, a long chain
    # of forms with π bounds, TAN near its poles in each mode)
    with open(os.path.join(ROOT, "work", "009-examples.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            p = [x.strip() for x in line.split(" | ")]
            rows.append((p[0], p[2], "X", p[5], "defint", p[3] + "|" + p[4], p[6]))
    for name, expr, lo, hi, angle in [
            ("n poly deg 11", "+".join(f"{k + 1}×X^{k}" for k in range(12)), "-1", "2", "RAD"),
            ("n seven forms", "SIN(2×X)+COS(3×X+1)+EXP(X)+1÷(2×X+1)+SQRT(X+1)+TAN(X÷2)+X^3", "0", "π÷3", "RAD"),
            ("n TAN DEG", "TAN(X)", "-89", "89", "DEG"), ("n TAN GRAD", "TAN(X)", "101", "299", "GRAD"),
            ("n (2X+1)^(5÷3)", "(2×X+1)^(5÷3)", "0", "13÷2", "RAD")]:
        rows.append((name, expr, "X", angle, "defint", lo + "|" + hi, "35"))
    # CAS 010: polynomial division, GCD and cancelling, the visible examples and the heaviest forms
    with open(os.path.join(ROOT, "work", "010-examples.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            p = [x.strip() for x in line.split(" | ")]
            rows.append((p[0], p[2], "X", "RAD", p[1], None if p[3] == "-" else p[3], p[4]))
    for name, op, f_, g_ in [
            ("q deg 12 by 1", "pdiv", "+".join(f"{k + 1}×X^{k}" for k in range(13)), "3×X-2"),
            ("q deg 12 by 6", "pdiv", "+".join(f"{(k % 5) + 1}×X^{k}" for k in range(13)), "X^6+2×X^3-X+5"),
            ("g deg 12 common 6", "pgcd", "(X^6+X+1)×(X^6-2×X^2+3)", "(X^6+X+1)×(2×X^6+X^5-7)"),
            ("g coprime deg 12", "pgcd", "X^12+X+1", "X^12-X^7+3"),
            ("k deg 12 over deg 12", "cancel", "(X^12-1)÷(X^12+X^6-X^4-X^2+1)", None),
            ("k (X-1)..(X-6)", "cancel", "(X-1)×(X-2)×(X-3)×(X-4)×(X-5)×(X-6)÷((X-1)×(X-3)×(X-5)×(X+7))", None)]:
        rows.append((name, f_, "X", "RAD", op, g_, "35"))
    # CAS 012: partial fractions, the visible examples, the heaviest forms, and n÷d nested across the guard in each side
    # (the reading's frame is the one a deep input carries; the arithmetic runs after the parse returns, rule 11)
    with open(os.path.join(ROOT, "work", "012-examples.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            p = [x.strip() for x in line.split(" | ")]
            rows.append((p[0], p[2], "X", "RAD", "partfrac", None, p[4]))
    for name, expr in [
            ("r 12 linear", "1÷(" + "×".join(f"(X{-i:+d})" if i else "X" for i in range(-5, 7)) + ")"),
            ("r 10 linear", "1÷((X^2-1)×(X^2-4)×(X^2-9)×(X^2-16)×(X^2-25))"),
            ("r (X^3-2)^4", "(X^11+1)÷(X^3-2)^4"),
            ("r (X^2+X+1)^6", "(X^5-3)÷(X^2+X+1)^6"),
            ("r mixed deg 12", "(X^12+1)÷((X-1)^3×(X^2+X+1)^2×(X+2)×(X^2+3))"),
            ("r cancel deg 12", "(X^6-1)×(X+5)÷((X^6-1)×(X-2)^3×(X^2+2)×(X+1))"),
            ("r non-monic powers", "(X^4+1)÷((3×X+7)^3×(5×X-11)^3)")]:
        rows.append((name, expr, "X", "RAD", "partfrac", None, "35"))
    # CAS 014: Taylor polynomials, the visible examples and the heaviest forms (the derivatives' swell grows with the
    # order: TAN, ASIN and LN(COS(X)) to 12, a nested composition to the arena's edge, a point with SQRT in it)
    with open(os.path.join(ROOT, "work", "014-examples.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            p = [x.strip() for x in line.split(" | ")]
            rows.append((p[0], p[2], p[3], p[6], "taylor", p[4] + "|" + p[5], p[7]))
    for name, expr, point, n in [
            ("t TAN to 12", "TAN(X)", "0", "12"), ("t ASIN to 12", "ASIN(X)", "0", "12"),
            ("t LN(COS(X)) to 12", "LN(COS(X))", "0", "12"), ("t EXP(SIN(X)) to 7", "EXP(SIN(X))", "0", "7"),
            ("t 1÷(1+X^2) to 12", "1÷(1+X^2)", "0", "12"), ("t COS at π÷3 to 8", "COS(X)", "π÷3", "8"),
            ("t SQRT(X) at SQRT(2)", "SQRT(X)", "SQRT(2)", "3")]:
        rows.append((name, expr, "X", "RAD", "taylor", point + "|" + n, "35"))
    # the value check's walk down the typed tree, with canon below it, at the depth guard's edge (CAS 014's review)
    for name, pre, mid, post, deep in [("t SIN nest", "SIN(", "X", ")", 43), ("t 1÷(2+ nest", "1÷(2+", "X", ")", 21),
                                       ("t LN(2+ nest", "LN(2+", "X", ")", 21), ("t SQRT(1+ nest", "SQRT(1+", "X", ")", 21)]:
        rows.append((f"{name} {deep}", pre * deep + mid + post * deep, "X", "RAD", "taylor", "0|0", "35"))
    # CAS 015: limits, the visible examples and the heaviest walks (L'Hôpital to the step cap, a quotient whose
    # derivatives swell, the rewrites at ∞, a two-sided pole that walks both sides)
    with open(os.path.join(ROOT, "work", "015-examples.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            p = [x.strip() for x in line.split(" | ")]
            rows.append((p[0], p[2], p[3], p[6], "limit", p[4] + "|" + ("" if p[5] == "both" else p[5]), p[7]))
    for name, expr, point, side in [
            ("l X^12÷EXP(X) at ∞", "X^12÷EXP(X)", "∞", ""), ("l X^13÷EXP(X) at ∞", "X^13÷EXP(X)", "∞", ""),
            ("l (TAN(X)-SIN(X))÷X^3", "(TAN(X)-SIN(X))÷X^3", "0", ""),
            ("l SIN(X)-X+X^3÷6 ÷X^5", "(SIN(X)-X+X^3÷6)÷X^5", "0", ""),
            ("l (1-COS(X))^2÷X^4", "(1-COS(X))^2÷X^4", "0", ""),
            ("l EXP(SIN(X))-1-X ÷X^2", "(EXP(SIN(X))-1-X)÷X^2", "0", ""),
            ("l (1+2÷X)^(3×X)", "(1+2÷X)^(3×X)", "∞", ""), ("l SQRT(X^2+5×X)-X", "SQRT(X^2+5×X)-X", "∞", ""),
            ("l 1÷SIN(X)^3 both", "1÷SIN(X)^3", "0", ""), ("l X^X at 0+", "X^X", "0", "+")]:
        rows.append((name, expr, "X", "RAD", "limit", point + "|" + side, "35"))
    # CAS 021: the series route, where the derivatives and the walk refuse: the unit's debt list, the two sides, ±∞, a
    # π-power row that stays NO_ROOM (TAN in DEG), and the series' deepest walks (SQRT(1+ nested 20 reaches 43 levels with
    # the bound's 8 under it)
    for name, expr, angle, op, repl in [
            ("ser EXP(SIN(X)) to 12", "EXP(SIN(X))", "RAD", "taylor", "0|12"),
            ("ser SIN(COS(X)) to 10", "SIN(COS(X))", "RAD", "taylor", "0|10"),
            ("ser TAN(X) to 11 DEG", "TAN(X)", "DEG", "taylor", "0|11"),
            ("ser SQRT(1+X)÷(2+X^2) to 11", "(SQRT(1+X))÷(2+X^2)", "RAD", "taylor", "0|11"),
            ("ser LN(1+X)×TAN(X) to 9", "(LN(1+X))×(TAN(X))", "RAD", "taylor", "0|9"),
            ("ser EXP(K×X)÷(2+X^2) DEG", "(EXP(K×X))÷(2+X^2)", "DEG", "taylor", "0|7"),
            ("ser ABS(X)^3 to 2", "ABS(X)^3", "RAD", "taylor", "0|2"),
            ("ser ABS(ATAN(X))^2 to 2", "ABS(ATAN(X))^2", "RAD", "taylor", "0|2"),
            ("ser SQRT(X^2) to 1", "SQRT(X^2)", "RAD", "taylor", "0|1"),
            ("ser K×ABS(X) to 1", "K×ABS(X)", "RAD", "taylor", "0|1"),
            ("ser ASIN(COS(X)) to 2", "ASIN(COS(X))", "RAD", "taylor", "0|2"),
            ("ser SIN nest 40 to 3", "SIN(" * 40 + "X" + ")" * 40, "RAD", "taylor", "0|3"),
            ("ser ATAN nest 30 to 3", "ATAN(" * 30 + "X" + ")" * 30, "RAD", "taylor", "0|3"),
            ("ser LN(1+ nest 20 to 2", "LN(1+" * 20 + "X" + ")" * 20, "RAD", "taylor", "0|2"),
            ("ser SQRT(1+ nest 20 to 2", "SQRT(1+" * 20 + "X" + ")" * 20, "RAD", "taylor", "0|2"),
            ("ser 1÷X^2-1÷SIN(X)^2", "1÷X^2-1÷SIN(X)^2", "RAD", "limit", "0|"),
            ("ser TAN(X)-1÷COS(X) π÷2-", "TAN(X)-1÷COS(X)", "RAD", "limit", "π÷2|-"),
            ("ser (1÷X-SIN(1÷X))×X -∞", "((1÷X)-SIN((1÷X)))×(X)", "DEG", "limit", "-∞|"),
            ("ser SIN(K×(1÷X))×(2×X+1)", "SIN(K×(1÷X))×(2×X+1)", "RAD", "limit", "-∞|")]:
        rows.append((name, expr, "X", angle, op, repl, "35"))
    # CAS 022: signs decided by value. The deepest constants the bound sizes (21-deep nests at 0: taylor_hole's 8 levels
    # refuse them; a deep constant under limit's walk), and the census rows (work/022-signs.md)
    Z, Z1 = "(10^25×SIN(1)^2+10^25×COS(1)^2-10^25)", "(SIN(1)^2+COS(1)^2-1)"
    for name, expr, angle, op, repl in [
            ("sign LN(2+ nest 21 to 0", "LN(2+" * 21 + "X" + ")" * 21, "RAD", "taylor", "0|0"),
            ("sign SQRT(1+ nest 21 to 0", "SQRT(1+" * 21 + "X" + ")" * 21, "RAD", "taylor", "0|0"),
            ("sign LN(2+ nest 21 to 2", "LN(2+" * 21 + "X" + ")" * 21, "RAD", "taylor", "0|2"),
            ("sign (LN(2+ nest 21+X)÷X", "(" + "LN(2+" * 21 + "1" + ")" * 21 + "+X)÷X", "RAD", "limit", "0|+"),
            ("sign (Z+X)÷X", "(" + Z + "+X)÷X", "RAD", "limit", "0|"),
            ("sign (Z+X^2)÷X^2 0+", "(" + Z + "+X^2)÷X^2", "RAD", "limit", "0|+"),
            ("sign Z×ABS(X)÷X", Z + "×ABS(X)÷X", "RAD", "limit", "0|"),
            ("sign TAN(SIN)-SIN(TAN)+Z1×X^3", "(TAN(SIN(X))-SIN(TAN(X))+" + Z1 + "×X^3)÷X^7", "RAD", "limit", "0|"),
            ("sign 1÷(Z+X) to 1", "1÷(" + Z + "+X)", "RAD", "taylor", "0|1"),
            ("sign ABS(Z+X) to 1", "ABS(" + Z + "+X)", "RAD", "taylor", "0|1"),
            ("sign TAN(Z+π÷2+X) to 1", "TAN(" + Z + "+π÷2+X)", "RAD", "taylor", "0|1"),
            ("sign 1÷(EXP(-20000)+X) to 1", "1÷(EXP(-20000)+X)", "RAD", "taylor", "0|1")]:
        rows.append((name, expr, "X", angle, op, repl, "35"))
    for n in (40, 43):              # the walk's own depth: nests continuity can't settle, up to the guard
        rows.append((f"l EXP nest {n} at ∞", "EXP(" * n + "X" + ")" * n, "X", "RAD", "limit", "∞|", "35"))
        rows.append((f"l LN nest {n} at ∞", "LN(" * n + "X" + ")" * n, "X", "RAD", "limit", "∞|", "35"))
        rows.append((f"l SIN nest {n} 1÷X at 0", "SIN(" * n + "1÷X" + ")" * n, "X", "RAD", "limit", "0|", "35"))
        rows.append((f"l 1÷( nest {n} at 0+", "1÷(" * n + "X" + ")" * n, "X", "RAD", "limit", "0|+", "35"))
    for n in range(20, 58, 4):
        rows.append((f"r nest num {n}", "(" * n + "X" + "+1)" * n + f"÷(X^2-{n * n})", "X", "RAD", "partfrac", None, "35"))
        rows.append((f"r nest den {n}", "1÷(" + "(" * n + "X" + "+1)" * n + f"-{n})", "X", "RAD", "partfrac", None, "35"))
    for n in range(36, 58, 2):      # nests the reading walks down, across the guard (NOT_POLYNOMIAL or NO_ROOM)
        rows.append((f"r sweep X^( {n}", "1÷(" + "X^(" * n + "X" + ")" * n + ")", "X", "RAD", "partfrac", None, "35"))
        rows.append((f"r sweep 1÷(1+ {n}", "1÷(1+" * n + "X" + ")" * n, "X", "RAD", "partfrac", None, "35"))
    # CAS 016 (poles in every op, ABS's corner, ∞-∞ of quotients) and CAS 017 (integrals by substitution and by parts):
    # the visible examples (each `casim ...` row as the bench runs it; a row's --arena goes, the bench bisecting its own,
    # and steps rows are the steps bench's)
    for unit, tag in (("016", "p"), ("017", "17:"), ("018", "18:")):
      with open(os.path.join(ROOT, "work", unit + "-examples.txt"), encoding="utf-8") as f:
        for i, line in enumerate(f):
            if not line.startswith("| `casim "):
                continue
            a = shlex.split(line.split("`")[1])[1:]
            if a[0] == "--arena":
                a = a[2:]
            op, t = a[0], a[1:]
            name = f"{tag} {i} {op}"
            if op == "steps":
                continue
            if op in ("simplify", "expand"):
                rows.append((name, t[0], "X", t[1], op, None, "35"))
            elif op in ("subst", "pdiv", "pgcd"):
                rows.append((name, t[0], t[1], t[3], op, t[2], "35"))
            elif op in ("taylor", "limit", "sum"):
                rows.append((name, t[0], t[1], t[4], op, t[2] + "|" + t[3], "35"))
            else:
                rows.append((name, t[0], t[1], t[2], op, None, "35"))
    # CAS 017's recursion over a deep subtree: parts, the parts chain and a substitution, each beside a constant nest
    # canon walks at every level (the stack gate through the keys: each level counts twice against the depth guard)
    for n in (30, 34, 38, 40):
        for name, pre, fn_ in (("parts", "X×EXP(X)×", "SIN"), ("LN^4", "LN(X)^4×", "COS"), ("sub", "X×EXP(X^2)×", "SIN")):
            rows.append((f"17: {name} beside a {n} nest", pre + (fn_ + "(") * n + "1" + ")" * n, "X", "RAD", "integrate",
                         None, "35"))
    # CAS 013: integrals of rational functions, the visible examples and the heaviest forms
    with open(os.path.join(ROOT, "work", "013-examples.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            p = [x.strip() for x in line.split(" | ")]
            rows.append((p[0], p[2], p[3], p[4], "integrate", None, p[5]))
    for name, expr, angle in [
            ("j mixed deg 11", "(X^11+1)÷((X-2)^3×(X^2+X+1)×(X+3)^2×(X^2+4)×(2×X-5)^2)", "RAD"),
            ("j three quadratics", "(X^5+1)÷((X^2+1)×(X^2+X+1)×(2×X^2-X+3))", "DEG"),
            ("j linear to the 6th", "(X^5+X+1)÷(3×X-2)^6", "RAD"),
            ("j six linear", "1÷((X^2-1)×(X^2-4)×(X^2-9))", "GRAD")]:
        rows.append((name, expr, "X", angle, "integrate", None, "35"))
    # the typed tree's walk at depth (the guard): ((((X+1)+1)+1)...-n, nested groups, swept across the limit
    for n in range(20, 58, 4):
        rows.append((f"v nest {n}", "(" * n + "X" + "+1)" * n + f"-{n}", "X", "RAD", "solve", None, "35"))
    rows.append(("v STU chain X^1^1^...", "X^" + "^".join(["1"] * 40) + "-2", "X", "RAD", "solve", None, "STU"))
    rows = [r if len(r) == 7 else r + ("35",) for r in rows + SHAPES003]
    # CAS 004: the heavy shapes again, read as STU (^ right to left), and STU's right chains across the guard
    for r in rows[:]:
        if r[0].startswith(("192 ", "probe ", "sweep ", "e ", "c ", "u ")):
            rows.append((r[0] + " STU",) + r[1:6] + ("STU",))
    for n in range(36, 58, 2):
        for op in ("simplify", "derive", "expand"):
            rows.append((f"STU chain X^ {n} {op[0]}", "^".join(["X"] * n), "X", "RAD", op, None, "STU"))
    return rows


def main():
    if not os.path.exists(CLI):
        sys.exit("build/casim missing: run make first")
    print(f"{'case':<26} {'op':<3} {'chars':>5} {'bytes':>5} {'arena':>6} {'out':>5} {'ratio':>6}")
    ratios, worst = [], {op: 0 for op in ("derive", "simplify", "expand", "collect", "subst", "solve", "integrate", "factor",
                                          "defint", "pdiv", "pgcd", "cancel", "taylor", "limit", "sum")}
    for name, expr, var, angle, op, repl, model in all_cases():
        arena, out, err = run(expr, var, angle, op, repl, model)
        if err:
            print(f"{name:<26} {op[0]:<3} {len(expr):>5} {len(expr.encode()):>5}  {err}")
            continue
        ratio = len(out) / len(expr)
        if op == "derive":
            ratios.append(ratio)
        if len(expr.encode()) >= 150:
            worst[op] = max(worst[op], arena)
        print(f"{name:<26} {op[0]:<3} {len(expr):>5} {len(expr.encode()):>5} {arena:>6} {len(out):>5} {ratio:>6.2f}")
    q = statistics.quantiles(ratios, n=4)
    print(f"\nderivative length ratio over {len(ratios)} cases: min {min(ratios):.2f}, quartiles "
          f"{q[0]:.2f} / {q[1]:.2f} / {q[2]:.2f}, max {max(ratios):.2f}")
    print(f"largest arena for a 150-191 byte equation: simplify {worst['simplify']}, derive {worst['derive']} "
          f"bytes (budget 8800)")


if __name__ == "__main__":
    main()
