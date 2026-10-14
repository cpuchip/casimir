#!/usr/bin/env python3
"""Writes the golden corpus's inputs (CAS 011, D1) to stdout; tests/golden_check --record adds each one's status and
text. Three sources, all casim's own: scripts/measure.py's cases (the S3 bench's), the visible examples in work/, and
a seeded random corpus from the generator below. None of abacus's hidden seeds are used or known here.

    scripts/golden.py > build/golden-inputs.txt
    build/golden_check --record build/golden-inputs.txt > tests/golden/corpus.txt   (on the source being recorded)
"""
import os
import random
import re
import shlex
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import measure  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ANGLE = {"DEG": 0, "RAD": 1, "GRAD": 2}
MODEL = {"STU": 1, "33": 33, "35": 35}
SEED = 20261009
RANDOM_CASES = 6000
PARTFRAC_CASES = 800


def esc(s):
    if s is None:
        return "\\N"
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")


def line(op, eqn, var="X", angle="RAD", model="35", repl=None, repl_model=None, repl2=None, arena=8192, cap=65536):
    v = ord(var) - ord("A")
    m = MODEL[model]
    rm = MODEL[repl_model] if repl_model else m
    return "\t".join([op, str(m), str(v), str(ANGLE[angle]), str(rm), str(arena), str(cap), esc(eqn), esc(repl),
                      esc(repl2)])


def bench():
    for _name, expr, var, angle, op, repl, model in measure.all_cases():
        lo, hi = repl.split("|", 1) if op in ("defint", "taylor", "limit") else (repl, None)
        for arena, cap in ((65536, 65536), (8192, 192)):    # as measured, and the calculator's budget and slot
            yield line(op, expr, var, angle, model, lo, None, hi, arena, cap)


def cli_line(cmd):
    """a `casim ...` command (work/016-examples.txt) as a golden line"""
    a = shlex.split(cmd)[1:]
    arena = 8192
    if a[0] == "--arena":
        arena, a = int(a[1]), a[2:]
    op, t = a[0], a[1:]
    if op in ("simplify", "expand"):
        return line(op, t[0], angle=t[1], arena=arena)
    if op in ("subst", "pdiv", "pgcd"):
        return line(op, t[0], t[1], t[3], repl=t[2], arena=arena)
    if op in ("taylor", "limit"):
        return line(op, t[0], t[1], t[4], repl=t[2], repl2=t[3], arena=arena)
    return line(op, t[0], t[1], t[2], arena=arena)


def examples():
    for name in sorted(os.listdir(os.path.join(ROOT, "work"))):
        if len(name) != len("000-examples.txt") or not name[:3].isdigit() or not name.endswith("-examples.txt"):
            continue                    # a unit's examples only (013-steps-examples.txt is walks: doc-examples.py)
        unit = name[:3]
        for raw in open(os.path.join(ROOT, "work", name), encoding="utf-8"):
            if raw.startswith("#") or not raw.strip():
                continue
            f = [x.strip() for x in raw.rstrip("\n").split(" | ")]
            if unit == "001":
                yield line("derive", f[1], f[2], f[3])
            elif unit == "002":
                yield line("simplify", f[1], angle=f[3])
                yield line("simplify", f[2], angle=f[3])
            elif unit == "003" and f[1] == "expand":
                yield line("expand", f[2], angle=f[4])
            elif unit == "003" and f[1] == "collect":
                yield line("collect", f[2], f[3], f[4])
            elif unit == "003" and f[1] == "subst":
                yield line("subst", f[2], f[3], f[5], repl=f[4])
            elif unit in ("005", "007"):
                yield line(f[1], f[2], f[3], model=f[4])
            elif unit in ("006", "013"):
                yield line("integrate", f[2], f[3], f[4], f[5])
            elif unit == "008":
                yield line("simplify", f[2], angle=f[3], model=f[4])
            elif unit == "009":
                yield line("defint", f[2], "X", f[5], f[6], repl=f[3], repl2=f[4])
            elif unit in ("010", "012"):
                yield line(f[1], f[2], "X", model=f[4], repl=None if f[3] == "-" else f[3])
            elif unit == "014":             # id | taylor | text | var | point | n | angle | model | answer
                yield line("taylor", f[2], f[3], f[6], f[7], repl=f[4], repl2=f[5])
            elif unit == "015":             # id | limit | text | var | point | side | angle | model | answer
                yield line("limit", f[2], f[3], f[6], f[7], repl=f[4], repl2="" if f[5] == "both" else f[5])
            elif unit in ("016", "017"):    # | `casim [--arena N] op args` | `answer` |
                if not raw.startswith("| `casim steps"):          # (a steps walk: doc-examples.py, as 013's)
                    yield cli_line(raw.split("`")[1])
            else:
                raise SystemExit(f"golden.py: {name}: no reader for this unit's columns")


def generated():
    rng = random.Random(SEED)
    atoms = ["X", "X", "Y", "2", "3", "0.5", "π", "1.5", "7", "-4", "1E3"]
    fns = ["SIN", "COS", "TAN", "LN", "EXP", "SQRT", "ASIN", "ACOS", "ATAN", "ABS", "LOG", "ALOG", "SINH", "COSH",
           "TANH", "SQ", "INV", "SGN"]

    def expr(d):
        if d == 0 or rng.random() < 0.3:
            return rng.choice(atoms)
        r = rng.random()
        if r < 0.25:
            return rng.choice(fns) + "(" + expr(d - 1) + ")"
        return "(" + expr(d - 1) + ")" + rng.choice("+-×÷^") + "(" + expr(d - 1) + ")"

    def poly(var="X", maxdeg=6):
        return "+".join(f"{rng.randint(-9, 9)}×{var}^{k}" for k in range(rng.randint(1, maxdeg + 1)))

    def rational_roots():
        roots = [f"({rng.randint(1, 4)}×X-{rng.randint(-6, 6)})" for _ in range(rng.randint(1, 4))]
        return "×".join(roots) + rng.choice(["", "×(X^2+1)", "×(X^2-2)", "×(X^3-2)"])

    for _ in range(RANDOM_CASES):
        angle = rng.choice(["RAD", "RAD", "DEG", "GRAD"])
        model = rng.choice(["35", "35", "33", "STU"])
        op = rng.choice(["derive", "simplify", "expand", "collect", "subst", "integrate", "defint", "solve", "factor",
                         "pdiv", "pgcd", "cancel"])
        arena = rng.choice([8192, 8192, 65536])
        if op in ("simplify", "expand"):
            yield line(op, expr(4), angle=angle, model=model, arena=arena)
        elif op in ("derive", "collect", "integrate"):
            yield line(op, expr(4), rng.choice("XXY"), angle, model, arena=arena)
        elif op == "subst":
            yield line(op, expr(3), "X", angle, model, repl=expr(2), arena=arena)
        elif op == "defint":
            yield line(op, expr(3), "X", angle, model, repl=rng.choice(["0", "1", "-1", "π÷4", "0.5"]),
                       repl2=rng.choice(["2", "π", "3", "-2", "1"]), arena=arena)
        elif op in ("solve", "factor"):
            yield line(op, rng.choice([poly(), rational_roots()]), "X", angle, model, arena=arena)
        elif op in ("pdiv", "pgcd"):
            yield line(op, rng.choice([poly(), rational_roots()]), "X", angle, model,
                       repl=rng.choice([poly(maxdeg=3), rational_roots()]), arena=arena)
        else:
            yield line(op, "(" + rational_roots() + ")÷(" + rational_roots() + ")", "X", angle, model, arena=arena)


def generated_partfrac():
    """CAS 012's own seeded fractions, a source of its own so the corpus's earlier lines keep their seed's stream:
    planted linear, quadratic and repeated factors, an improper numerator, a common factor, an unfactorable cubic"""
    rng = random.Random(SEED + 12)

    def factor():
        r = rng.random()
        if r < 0.6:
            return f"({rng.randint(1, 4)}×X{rng.randint(-6, 6):+d})"
        return rng.choice(["(X^2+1)", "(X^2-2)", "(X^2+X+1)", "(2×X^2-X+3)", "(X^3-2)", "X"])

    def num(maxdeg):
        return "+".join(f"{rng.randint(-9, 9)}×X^{k}" for k in range(rng.randint(1, maxdeg + 1)))

    for _ in range(PARTFRAC_CASES):
        den = "×".join(factor() + rng.choice(["", "", "", "^2", "^3"]) for _ in range(rng.randint(1, 4)))
        common = rng.choice(["", "", "", f"(X{rng.randint(-5, 5):+d})×"])
        text = f"{common}({num(rng.randint(0, 7))})÷({common}{den})"
        yield line("partfrac", text, "X", rng.choice(["RAD", "DEG"]), rng.choice(["35", "35", "33", "STU"]),
                   arena=rng.choice([8192, 8192, 65536]))


TAYLOR_CASES = 400


def generated_taylor():
    """CAS 014's own seeded inputs, a source of its own: elementary functions and their sums, products, quotients and
    compositions, at 0 and at points with exact values (1, -2, 1÷2, π÷3, π÷6, SQRT(2)), orders 0 to 12, every angle
    and model, at both budgets"""
    rng = random.Random(SEED + 14)
    atoms = ["SIN(X)", "COS(X)", "EXP(X)", "LN(1+X)", "ATAN(X)", "SQRT(1+X)", "1÷(1-X)", "X^3-2×X+1", "TAN(X)",
             "EXP(2×X)", "SIN(3×X)", "X×EXP(X)", "1÷(2+X)", "(1+X)^(1÷3)", "EXP(K×X)", "COS(X)^2"]
    for _ in range(TAYLOR_CASES):
        a = rng.choice(atoms)
        r = rng.random()
        if r < 0.3:
            a = f"{a}+{rng.choice(atoms)}"
        elif r < 0.5:
            a = f"({a})×({rng.choice(atoms)})"
        elif r < 0.6:
            a = f"({a})÷(2+X^2)"
        point = rng.choice(["0", "0", "0", "1", "-2", "1÷2", "π÷3", "π÷6", "SQRT(2)"])
        yield line("taylor", a, "X", rng.choice(["RAD", "RAD", "DEG", "GRAD"]), rng.choice(["35", "35", "33", "STU"]),
                   repl=point, repl2=str(rng.randint(0, 12)), arena=rng.choice([8192, 8192, 65536]))


LIMIT_CASES = 400


def generated_limit():
    """CAS 015's own seeded inputs: quotients that are 0÷0 or ∞÷∞ at a point, sums and products of functions at
    points where they're continuous, one-sided poles, rationals and growth at ±∞, and the rewriting forms (0×∞, ∞-∞,
    1^∞), every side, angle and model, at both budgets"""
    rng = random.Random(SEED + 15)
    zero = ["SIN(X)", "X", "EXP(X)-1", "LN(1+X)", "1-COS(X)", "TAN(X)", "ATAN(X)", "X^2", "SQRT(1+X)-1", "X-SIN(X)",
            "SIN(K×X)", "X^3+X"]
    fin = ["COS(X)", "EXP(X)", "1+X^2", "2-X", "SQRT(4+X)", "ATAN(X)+1", "3"]
    grow = ["X", "X^2", "EXP(X)", "LN(X)", "SQRT(X)", "X^3-X", "2×X+1", "X×LN(X)"]
    for _ in range(LIMIT_CASES):
        r = rng.random()
        side = rng.choice(["", "", "+", "-"])
        if r < 0.35:
            text, point = f"({rng.choice(zero)})÷({rng.choice(zero)})", "0"
        elif r < 0.5:
            text = rng.choice([f"({rng.choice(fin)})×({rng.choice(zero)})", f"{rng.choice(fin)}+{rng.choice(zero)}",
                               f"({rng.choice(fin)})÷({rng.choice(fin)})"])
            point = rng.choice(["0", "1", "-2", "π÷3", "1÷2"])
        elif r < 0.65:
            text, point = f"({rng.choice(fin)})÷({rng.choice(zero)})", "0"
        elif r < 0.85:
            text = rng.choice([f"({rng.choice(grow)})÷({rng.choice(grow)})", f"{rng.choice(grow)}-{rng.choice(grow)}",
                               f"({re.sub(r'(?<![A-Z])X(?![A-Z])', '(1÷X)', rng.choice(zero))})×({rng.choice(grow)})",
                               f"(1+{rng.randint(1, 5)}÷X)^({rng.randint(1, 3)}×X)",
                               f"SQRT(X^2+{rng.randint(1, 5)}×X)-X"])
            point, side = rng.choice(["∞", "∞", "-∞"]), ""
        else:
            text, point, side = rng.choice(["SIN(X)", "COS(X)+1", "SIN(X)÷X", "ATAN(X)", "TANH(X)", "EXP(-X)"]), \
                rng.choice(["∞", "-∞"]), ""
        if "LN" in text or "SQRT(X)" in text:
            side = "+" if point == "0" else side
        yield line("limit", text, "X", rng.choice(["RAD", "RAD", "DEG", "GRAD"]), rng.choice(["35", "35", "33", "STU"]),
                   repl=point, repl2=side, arena=rng.choice([8192, 8192, 65536]))


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    print(f"# Casimir's golden corpus (CAS 011): scripts/golden.py's inputs (S3 bench cases, work/ examples, seed {SEED})")
    for src in (bench, examples, generated, generated_partfrac, generated_taylor, generated_limit):
        for row in src():
            print(row)


if __name__ == "__main__":
    main()
