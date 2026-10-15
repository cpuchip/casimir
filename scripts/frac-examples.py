#!/usr/bin/env python3
"""frac-print: the visible example rows whose written answer the fraction build no longer prints (units 005-017)."""
import os, sys, subprocess, tempfile
ROOT = sys.argv[1]
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import golden
rows = []
for name in sorted(os.listdir(os.path.join(ROOT, "work"))):
    if len(name) != len("000-examples.txt") or not name.endswith("-examples.txt") or name[:3] in ("001", "002", "003"):
        continue
    unit = name[:3]
    for raw in open(os.path.join(ROOT, "work", name), encoding="utf-8"):
        if raw.startswith("#") or not raw.strip():
            continue
        if unit in ("016", "017"):
            if raw.startswith("| `casim steps") or not raw.startswith("| `casim"):
                continue
            want = raw.split("`")[3]
            ln = golden.cli_line(raw.split("`")[1])
        else:
            f = [x.strip() for x in raw.rstrip("\n").split(" | ")]
            want = f[-1]
            ln = None
            if unit in ("005", "007"): ln = golden.line(f[1], f[2], f[3], model=f[4])
            elif unit in ("006", "013"): ln = golden.line("integrate", f[2], f[3], f[4], f[5])
            elif unit == "008": ln = golden.line("simplify", f[2], angle=f[3], model=f[4])
            elif unit == "009": ln = golden.line("defint", f[2], "X", f[5], f[6], repl=f[3], repl2=f[4])
            elif unit in ("010", "012"): ln = golden.line(f[1], f[2], "X", model=f[4], repl=None if f[3] == "-" else f[3])
            elif unit == "014": ln = golden.line("taylor", f[2], f[3], f[6], f[7], repl=f[4], repl2=f[5])
            elif unit == "015": ln = golden.line("limit", f[2], f[3], f[6], f[7], repl=f[4], repl2="" if f[5] == "both" else f[5])
        rows.append((name, raw.split(" | ")[0][:40], want, ln))
with tempfile.NamedTemporaryFile("w", suffix=".tsv", delete=False, encoding="utf-8") as fh:
    fh.write("".join(r[3] + "\n" for r in rows)); path = fh.name
def rec(gc):
    out = subprocess.run([gc, "--record", path], capture_output=True, text=True, check=True).stdout
    return [l.split("\t") for l in out.split("\n") if l and not l.startswith("#")]
new = rec(os.path.join(ROOT, "build/golden_check"))
old = rec(os.path.join(sys.argv[2], "build/golden_check"))
def shown(fl):
    st = fl[10]; t = fl[11] if len(fl) > 11 else ""
    return t if st == "OK" else st
moved = {}
for r, fl, fo in zip(rows, new, old):
    got = shown(fl)
    if got != shown(fo):
        moved.setdefault(r[0], []).append((r[1], r[2], got))
print(f"example rows: {len(rows)}; answer moved (base build -> fraction build): {sum(len(v) for v in moved.values())}")
for k, v in moved.items():
    print(f"  {k}: {len(v)}")
    for i, w, g in v[:3]:
        print(f"    {i}: {w[:70]}  ->  {g[:70]}")
