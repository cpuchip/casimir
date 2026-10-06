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


def run(expr, var, angle):
    p = subprocess.run([CLI, "--arena", "65536", "--min-arena", "derive", expr, var, angle],
                       stdout=subprocess.PIPE, text=True)
    lines = p.stdout.splitlines()
    if p.returncode != 0:
        return None, None, lines[0] if lines else "?"
    return int(lines[1].split()[1]), lines[0], None


def main():
    if not os.path.exists(CLI):
        sys.exit("build/casim missing: run make first")
    rows = []
    with open(os.path.join(ROOT, "work", "001-examples.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            parts = [p.strip() for p in line.split(" | ")]
            rows.append((parts[0], parts[1], parts[2], parts[3]))
    for name, (expr, angle) in SHAPES.items():
        rows.append((name, expr, "X", angle))
    print(f"{'case':<24} {'chars':>5} {'arena':>6} {'out':>5} {'ratio':>6}")
    ratios = []
    worst = 0
    for name, expr, var, angle in rows:
        arena, out, err = run(expr, var, angle)
        if err:
            print(f"{name:<24} {len(expr):>5}  {err}")
            continue
        ratio = len(out) / len(expr)
        ratios.append(ratio)
        if len(expr) >= 90:
            worst = max(worst, arena)
        print(f"{name:<24} {len(expr):>5} {arena:>6} {len(out):>5} {ratio:>6.2f}")
    q = statistics.quantiles(ratios, n=4)
    print(f"\nlength ratio over {len(ratios)} cases: min {min(ratios):.2f}, quartiles "
          f"{q[0]:.2f} / {q[1]:.2f} / {q[2]:.2f}, max {max(ratios):.2f}")
    print(f"largest arena for an equation of 90-100 characters: {worst} bytes (budget 8192)")


if __name__ == "__main__":
    main()
