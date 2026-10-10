#!/usr/bin/env python3
"""Writes the golden corpus's inputs (CAS 011, D1) to stdout; tests/golden_check --record adds each one's status and
text. Three sources, all casim's own: scripts/measure.py's cases (the S3 bench's), the visible examples in work/, and
a seeded random corpus from the generator below. None of abacus's hidden seeds are used or known here.

    scripts/golden.py > build/golden-inputs.txt
    build/golden_check --record build/golden-inputs.txt > tests/golden/corpus.txt   (on the source being recorded)
"""
import os
import random
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
        lo, hi = repl.split("|", 1) if op == "defint" else (repl, None)
        for arena, cap in ((65536, 65536), (8192, 192)):    # as measured, and the calculator's budget and slot
            yield line(op, expr, var, angle, model, lo, None, hi, arena, cap)


def examples():
    for name in sorted(os.listdir(os.path.join(ROOT, "work"))):
        if not name.endswith("-examples.txt"):
            continue
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
            elif unit == "006":
                yield line("integrate", f[2], f[3], f[4], f[5])
            elif unit == "008":
                yield line("simplify", f[2], angle=f[3], model=f[4])
            elif unit == "009":
                yield line("defint", f[2], "X", f[5], f[6], repl=f[3], repl2=f[4])
            elif unit in ("010", "012"):
                yield line(f[1], f[2], "X", model=f[4], repl=None if f[3] == "-" else f[3])
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


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    print(f"# Casimir's golden corpus (CAS 011): scripts/golden.py's inputs (S3 bench cases, work/ examples, seed {SEED})")
    for src in (bench, examples, generated, generated_partfrac):
        for row in src():
            print(row)


if __name__ == "__main__":
    main()
