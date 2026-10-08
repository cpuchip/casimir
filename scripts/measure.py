#!/usr/bin/env python3
"""What a derivative costs, through the host CLI (build/casim): the smallest arena each case needs
(--min-arena; a node is 24 bytes on the host and on the ESP32-S3, so the figure holds on the chip),
and the derivative's length against the equation's, in characters as the calculator shows them.

The cases: unit CAS 001's visible examples, and equations of about 100 characters (the unit's
budget case) shaped to make the derivative large: long products, quotients, powers and nests of
functions in DEG, where every level carries the angle factor.
"""
import os
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
    if op == "derive" or op == "collect" or op == "solve":
        return m + [op, expr, var, angle]
    if op == "subst":
        return m + [op, expr, var, repl, angle]
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
    ratios, worst = [], {"derive": 0, "simplify": 0, "expand": 0, "collect": 0, "subst": 0}
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
          f"bytes (budget 8192)")


if __name__ == "__main__":
    main()
