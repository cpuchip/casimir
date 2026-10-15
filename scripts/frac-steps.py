#!/usr/bin/env python3
"""frac-print: steps walks over every golden integrate/solve/partfrac input, base build against the fraction build, at
caps 192 and 150: the walks whose text moved, and those that newly end TOO_LONG (or newly page)."""
import subprocess, sys
from concurrent.futures import ThreadPoolExecutor
NEW, OLD, CORPUS = sys.argv[1] + "/build/casim", sys.argv[2] + "/build/casim", sys.argv[1] + "/tests/golden/corpus.txt"
def unesc(s): return None if s == "\\N" else s.replace("\\t", "\t").replace("\\n", "\n").replace("\\\\", "\\")
cases = set()
for l in open(CORPUS, encoding="utf-8"):
    if l.startswith("#") or not l.strip(): continue
    f = l.rstrip("\n").split("\t")
    if f[0] in ("integrate", "solve", "partfrac") and f[10] == "OK":
        cases.add((f[0], unesc(f[7]), chr(ord("A") + int(f[2]) - 0) if False else f[2], f[3], f[5]))
VARS = {}
def run(cli, op, eqn, var, angle, arena, cap):
    v = chr(ord("A") + int(var)); a = ["RAD", "DEG", "GRAD"][int(angle)]
    p = subprocess.run([cli, "--arena", arena, "--cap", str(cap), "steps", op, eqn, v, a], capture_output=True, text=True)
    return p.stdout
jobs = [(c, cap) for c in sorted(cases) for cap in (192, 150)]
def one(j):
    (op, eqn, var, angle, arena), cap = j
    return j, run(OLD, op, eqn, var, angle, arena, cap), run(NEW, op, eqn, var, angle, arena, cap)
with ThreadPoolExecutor(16) as ex:
    res = list(ex.map(one, jobs))
moved = newly_long = newly_fit = 0; ex_long = []; worst = []
for j, o, n in res:
    if o == n: continue
    moved += 1
    ol, nl = o.strip().endswith("TOO_LONG"), n.strip().endswith("TOO_LONG")
    if nl and not ol: newly_long += 1; ex_long.append((j, o, n))
    if ol and not nl: newly_fit += 1
    if not ol and not nl:
        lo = max(len(x.encode()) for x in o.split("\n")); ln = max(len(x.encode()) for x in n.split("\n"))
        worst.append((ln - lo, j))
print(f"walks: {len(cases)} inputs x 2 caps = {len(jobs)}; text moved {moved}; newly TOO_LONG {newly_long}; newly fit {newly_fit}")
if worst:
    worst.sort()
    print(f"  longest page growth: worst {worst[-1][0]:+} B, best {worst[0][0]:+} B")
for j, o, n in ex_long[:8]:
    print(f"  NEWLY TOO_LONG cap {j[1]} arena {j[0][4]}: steps {j[0][0]} {j[0][1][:80]}")
    ol = [x for x in o.split("\n") if x]; nl = [x for x in n.split("\n") if x]
    longest = max(ol, key=lambda x: len(x.encode()))
    print(f"     base longest page {len(longest.encode())} B: {longest[:110]}")
