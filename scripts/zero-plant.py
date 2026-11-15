#!/usr/bin/env python3
"""CAS 040: X's coefficient built from an identity Z that is 0 (Z×atom: zero) or Z+atom (not zero, the
atom not 0 everywhere); expect NEW to decline every zero row and print OLD's line on every other. Usage: scripts/zero-plant.py OLD NEW N SEED"""
import random, subprocess, sys
OLD, NEW, N, SEED = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
rnd = random.Random(SEED)
LET = "ABCD"
def Z():
    l, m = rnd.sample(LET, 2)
    return rnd.choice([f"(SIN({l})^2+COS({l})^2-1)", f"(COSH({l})^2-SINH({l})^2-1)", f"(TAN({l})×COS({l})-SIN({l}))",
                       f"(EXP({l}+{m})-EXP({l})×EXP({m}))", f"(SIN(2×{l})-2×SIN({l})×COS({l}))",
                       f"(COS(2×{l})-2×COS({l})^2+1)", f"(SQRT({l})^2-{l})", f"(LN({l}^2)-2×LN({l}))",
                       f"(SIN({l}+{m})-SIN({l})×COS({m})-COS({l})×SIN({m}))", f"(EXP(2×{l})-EXP({l})^2)"])
def atom():
    l, m = rnd.sample(LET, 2); n = rnd.choice([1, 2, 3, 5, -1, -2])
    return rnd.choice([str(n), l, f"({l}+{n})", f"({l}-{m})", f"SIN({l})", f"EXP({l})", "π", f"{l}^2", f"COS({m})"])
def run(cli, e):
    return subprocess.run([cli, "solve", e, "X", "RAD"], capture_output=True, text=True).stdout.strip()
cnt = {}
for _ in range(N):
    zero = rnd.random() < 0.5
    c = f"{Z()}×{atom()}" if zero else f"({Z()}+{atom()})"
    rhs = rnd.choice(["1", "0", rnd.choice(LET), atom(), f"{atom()}×X"]) if zero else rnd.choice(["1", rnd.choice(LET)])
    e = f"{c}×X={rhs}" if not rhs.endswith("×X") else f"{c}×X+{rhs}={rhs}"
    a, b = run(OLD, e), run(NEW, e)
    if zero: k = "zero: declines" if b == "NOT_NUMERIC" else "zero: ANSWERS"
    else: k = "nonzero: as before" if a == b else "nonzero: MOVED"
    cnt[k] = cnt.get(k, 0) + 1
    if k.endswith(("ANSWERS", "MOVED")): print(f"{k}\t{e}\t{a}\t{b}")
print(cnt)
