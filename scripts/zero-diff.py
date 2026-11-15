#!/usr/bin/env python3
"""CAS 040: random letter equations through two CLIs; print every row whose solve output differs. Usage: scripts/zero-diff.py OLD NEW N SEED"""
import random, subprocess, sys
OLD, NEW, N, SEED = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
rnd = random.Random(SEED)
LET = "ABCD"
def atom():
    l, m = rnd.sample(LET, 2); n = rnd.choice([1, 2, 3, 5, -1, -2])
    return rnd.choice([str(n), l, f"({l}+{n})", f"({l}-{m})", f"{n}×{l}", f"SQRT({l})", f"LN({l})", f"(1÷{abs(n)+1})",
                       f"({l}+{m})", f"SIN({l})", f"COS({l})", f"EXP({l})", f"TAN({l})", f"ABS({l})", "π", f"{l}^2",
                       f"SIN({l})^2", f"COS({l})^2", f"(1÷{l})", f"SINH({l})", f"COSH({l})"])
def coef():
    k = rnd.randrange(3)
    return atom() if k == 0 else f"{atom()}×{atom()}" if k == 1 else f"({atom()}+{atom()})"
def side():
    t = []
    for _ in range(rnd.randrange(1, 4)):
        c = coef()
        t.append(rnd.choice([f"{c}×X", f"X×{c}", c, f"{c}×(X+{rnd.choice(LET)})"]))
    return "+".join(t).replace("+-", "-")
def run(cli, e):
    return subprocess.run([cli, "solve", e, "X", "RAD"], capture_output=True, text=True).stdout.strip()
moved = solved = 0
for _ in range(N):
    e = f"{side()}={side()}"
    a, b = run(OLD, e), run(NEW, e)
    solved += a.startswith("SOLVED")
    if a != b:
        moved += 1
        print(f"MOVED\t{e}\t{a}\t{b}")
print(f"rows {N}, old SOLVED {solved}, moved {moved}")
