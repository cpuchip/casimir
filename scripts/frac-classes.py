#!/usr/bin/env python3
"""frac-print (d): readability classes fractions alone don't fix, counted over the golden answers of both builds."""
import re, subprocess, sys, tempfile, collections
NEW, OLD = sys.argv[1] + "/build/golden_check", sys.argv[2] + "/build/golden_check"
inputs = ["\t".join(l.rstrip("\n").split("\t")[:10]) for l in open(sys.argv[1] + "/tests/golden/corpus.txt", encoding="utf-8")
          if l.strip() and not l.startswith("#")]
with tempfile.NamedTemporaryFile("w", delete=False, encoding="utf-8") as f:
    f.write("".join(i + "\n" for i in inputs)); path = f.name
def rec(g):
    out = subprocess.run([g, "--record", path], capture_output=True, text=True, check=True).stdout
    return [(l.split("\t") + ["", ""])[10:12] for l in out.split("\n") if l and not l.startswith("#")]
new, old = rec(NEW), rec(OLD)

def groups_after_div(t):
    """the text of each ( ) group right after a ÷"""
    res, i = [], 0
    while True:
        i = t.find("÷(", i)
        if i < 0: return res
        d, j = 0, i + len("÷")
        for k in range(j, len(t)):
            d += t[k] == "(";  d -= t[k] == ")"
            if d == 0: res.append(t[j + 1:k]); break
        i = j

def top_sum(g):
    d = 0
    for k, ch in enumerate(g):
        d += ch == "("; d -= ch == ")"
        if d == 0 and k > 0 and ch in "+-" and g[k - 1] not in "E^(": return True
    return False

def frac_coef_sum(g, fracs):
    """a sum in a denominator with a coefficient that isn't whole: a decimal (base) or a ÷digits at depth 0 (frac)"""
    if not top_sum(g): return False
    flat, d = [], 0
    for ch in g:
        if ch == "(": d += 1
        if d == 0: flat.append(ch)
        if ch == ")": d -= 1
    flat = "".join(flat)
    return bool(re.search(r"÷\d", flat)) if fracs else bool(re.search(r"\d\.\d", flat))

def quad(g):
    flat = re.sub(r"\([^()]*\)", "", g)
    return bool(re.search(r"\b[A-Z]\^2\b", g)) and not re.search(r"\^([3-9]|\d\d)", flat)

C = collections.Counter(); ex = collections.defaultdict(list)
for (so, to), (sn, tn) in zip(old, new):
    if so != "OK" or sn != "OK": continue
    for name, t, fr in (("base", to, False), ("frac", tn, True)):
        gs = [g for g in groups_after_div(t) if frac_coef_sum(g, fr)]
        if gs:
            C[name + " sum-denominator"] += 1
            if any(quad(g) for g in gs):
                C[name + " quadratic sum-denominator"] += 1; ex[name + " q"].append(t)
    if re.search(r"\d÷\d+÷", tn): C["frac k÷d÷u (coefficient's ÷d before another divisor)"] += 1; ex["kdu"].append(tn)
    if re.search(r"\(1÷\d+\)×\(1÷\d+\)", tn): C["frac numbers kept apart, (1÷d)×(1÷d)"] += 1
    if re.search(r"\d÷\d+×[A-Z∫(]", tn): C["frac k÷d×u (a quotient read left to right before a factor)"] += 1; ex["kdx"].append(tn)
for k, v in sorted(C.items()): print(f"{v:6}  {k}")
for k in ("frac q", "kdu", "kdx"):
    for t in ex[k][:3]: print(f"  [{k}] {t[:150]}")
