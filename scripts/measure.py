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


def run(expr, var, angle, op="derive"):
    args = ["derive", expr, var, angle] if op == "derive" else ["simplify", expr, angle]
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
    for name, (expr, angle) in SHAPES192.items():
        rows.append((name + " S", expr, "X", angle, "simplify"))
        rows.append((name + " D", expr, "X", angle, "derive"))
    return rows


def main():
    if not os.path.exists(CLI):
        sys.exit("build/casim missing: run make first")
    print(f"{'case':<26} {'op':<3} {'chars':>5} {'bytes':>5} {'arena':>6} {'out':>5} {'ratio':>6}")
    ratios, worst = [], {"derive": 0, "simplify": 0}
    for name, expr, var, angle, op in all_cases():
        arena, out, err = run(expr, var, angle, op)
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
