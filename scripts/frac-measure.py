#!/usr/bin/env python3
"""frac-print (prototype, abacus #8392): what printing every rational as a quotient moves in the golden corpus.

    scripts/frac-measure.py BASE_DIR [CORPUS]

BASE_DIR holds a build of the same tree with CASIM_FRAC_PRINT 0 (build/golden_check). This tree's build/golden_check
prints fractions. For every corpus line: the two builds' answers, then
  - the moved lines, by op and by status (a status that moves is listed);
  - the read-back by value: each part of a moved answer (split at "; ", a "WORD: " prefix and a ≠ row kept as they
    are) is canonicalised by the BASE build, old text and new text, at a 512 KB arena; the two must print the same;
  - the fixed point: each new part canonicalised by this build prints itself again (a fraction reads back whole);
  - the byte growth (UTF-8) of each moved OK answer: worst, median, and the lines that newly hit TOO_LONG at their cap.
"""
import os
import statistics
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def esc(s):
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")


def unesc(s):
    out, i = [], 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            out.append({"t": "\t", "n": "\n"}.get(s[i + 1], s[i + 1]))
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def record(golden, lines):
    """golden_check --record on input lines (ten fields each); returns [(status, text)]."""
    with tempfile.NamedTemporaryFile("w", suffix=".tsv", delete=False, encoding="utf-8") as f:
        f.write("".join(l + "\n" for l in lines))
        path = f.name
    try:
        out = subprocess.run([golden, "--record", path], capture_output=True, text=True, check=True).stdout
    finally:
        os.unlink(path)
    rows = []
    for l in out.split("\n"):
        if not l or l.startswith("#"):
            continue
        fl = l.split("\t")
        rows.append((fl[10], unesc(fl[11]) if len(fl) > 11 else ""))
    assert len(rows) == len(lines), (len(rows), len(lines))
    return rows


def parts(text):
    """an answer's parts, each (prefix, expression): "SOLVED: X=1; X=2" is ("SOLVED: ", "X=1"), ("", "X=2")"""
    res = []
    for p in text.split("; "):
        pre = ""
        if ": " in p and p.split(": ", 1)[0].isupper():
            pre, p = p.split(": ", 1)
            pre += ": "
        res.append((pre, p))
    return res


def main():
    base = sys.argv[1]
    corpus = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "tests/golden/corpus.txt")
    inputs = []
    for l in open(corpus, encoding="utf-8"):
        l = l.rstrip("\n")
        if l and not l.startswith("#"):
            inputs.append("\t".join(l.split("\t")[:10]))
    old = record(os.path.join(base, "build/golden_check"), inputs)
    new = record(os.path.join(HERE, "build/golden_check"), inputs)

    moved, by_op, status_moves, growth, too_long = [], {}, [], [], []
    for inp, (so, to), (sn, tn) in zip(inputs, old, new):
        if (so, to) == (sn, tn):
            continue
        op = inp.split("\t")[0]
        moved.append((inp, so, to, sn, tn))
        by_op[op] = by_op.get(op, 0) + 1
        if so != sn:
            status_moves.append((inp, so, to, sn, tn))
            if sn == "TOO_LONG":
                too_long.append((inp, so, to))
        elif so == "OK":
            growth.append((len(tn.encode()) - len(to.encode()), len(to.encode()), len(tn.encode()), inp, to, tn))

    # the read-back: every differing part, both texts through the base build's simplify
    checks, keys = [], []
    for inp, so, to, sn, tn in moved:
        if so != "OK" or sn != "OK":
            continue
        po, pn = parts(to), parts(tn)
        if len(po) != len(pn) or any(a[0] != b[0] for a, b in zip(po, pn)):
            keys.append(("shape", inp, to, tn))
            continue
        f = inp.split("\t")
        for (pre, a), (_, b) in zip(po, pn):
            if a == b:
                continue
            for t in (a, b):
                checks.append("\t".join(["simplify", f[1], f[2], f[3], f[4], "524288", "65536",
                                         esc(t.replace("≠", "=")), "\\N", "\\N"]))
            keys.append(("part", inp, a, b))
    rb = record(os.path.join(base, "build/golden_check"), checks) if checks else []
    fp_in = [c for i, c in enumerate(checks) if i % 2 == 1]
    fp = record(os.path.join(HERE, "build/golden_check"), fp_in) if fp_in else []

    value_bad, fixed_bad, shape_bad, i, fixed_base = [], [], [], 0, 0
    for k in keys:
        if k[0] == "shape":
            shape_bad.append(k)
            continue
        (sa, ta), (sb, tb) = rb[2 * i], rb[2 * i + 1]
        if sa != "OK" or sb != "OK" or ta != tb:
            value_bad.append((k, (sa, ta), (sb, tb)))
        sf, tf = fp[i]
        # counted only where the base build's own text is a fixed point (collect's form, say, is not canonical)
        if (sf != "OK" or tf != k[3].replace("≠", "=")) and ta == k[2].replace("≠", "="):
            fixed_bad.append((k, (sf, tf)))
        if ta == k[2].replace("≠", "="):
            fixed_base += 1
        i += 1

    print(f"corpus lines: {len(inputs)}; moved: {len(moved)}")
    print("moved by op: " + ", ".join(f"{op} {n}" for op, n in sorted(by_op.items())))
    print(f"status moved: {len(status_moves)} (newly TOO_LONG: {len(too_long)})")
    for inp, so, to, sn, tn in status_moves:
        f = inp.split("\t")
        print(f"  {f[0]} arena {f[5]} cap {f[6]} {f[7][:70]}: {so} {to[:60]} -> {sn}")
    print(f"read-back by value: {i} parts, {len(value_bad)} differ, {len(shape_bad)} answers changed shape")
    for k, a, b in value_bad[:20]:
        print(f"  VALUE {k[1].split(chr(9))[0]}: {k[2][:90]}\n     -> {k[3][:90]}\n     base canon {a[0]} {a[1][:90]}\n"
              f"                {b[0]} {b[1][:90]}")
    for k in shape_bad[:10]:
        print(f"  SHAPE {k[1][:80]}: {k[2][:80]} -> {k[3][:80]}")
    print(f"fixed point: of {fixed_base} parts whose old text reads back as itself in the base build, "
          f"{fixed_base - len(fixed_bad)} new texts read back as themselves")
    for k, b in fixed_bad[:40]:
        print(f"  FIXED {k[1].split(chr(9))[0]}: {k[3][:100]}\n     reads back {b[0]} {b[1][:100]}")
    if growth:
        d = sorted(g[0] for g in growth)
        r = sorted(g[2] / g[1] for g in growth if g[1])
        print(f"byte growth over {len(growth)} moved OK answers: median {statistics.median(d):+} B, "
              f"worst {d[-1]:+} B, best {d[0]:+} B; ratio median {statistics.median(r):.3f}, worst {r[-1]:.3f}; "
              f"shorter {sum(1 for x in d if x < 0)}, same {sum(1 for x in d if x == 0)}, longer {sum(1 for x in d if x > 0)}")
        for g in sorted(growth, reverse=True)[:5]:
            print(f"  {g[0]:+} B ({g[1]} -> {g[2]}): {g[4][:80]}\n      -> {g[5][:80]}")
        for g in sorted(growth)[:3]:
            print(f"  {g[0]:+} B ({g[1]} -> {g[2]}): {g[4][:80]}\n      -> {g[5][:80]}")


if __name__ == "__main__":
    main()
