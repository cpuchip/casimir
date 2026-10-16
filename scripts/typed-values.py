#!/usr/bin/env python3
"""CAS 019's rewrite judge (in make check): every golden input with a typed decimal, each such literal rewritten as
its exact fraction (0.5 -> (1÷2), 1E3 -> (1000)). Run both ways:
  - the rewritten input has no typed decimal, so its answer prints no decimal point (unless DECIMAL_FORM, rule 5);
  - both answers are one value: each part (split at "; ", a row's sides at = < > ≠) read back by casim with
    CASIM_PRINT_DECIMAL prints alike;
  - a status may differ only where text length or the arena decides it (TOO_LONG, NO_ROOM, DECIMAL_FORM), counted.
Its planted faults first: a decimal left in a rewritten input must be caught by the first test, and an answer with one
number changed by the second."""
import os
import re
import subprocess
import sys
import tempfile
from fractions import Fraction

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN, CLI = os.path.join(ROOT, "build", "golden_check"), os.path.join(ROOT, "build", "casim")
LIT = re.compile(r"(?<![\d.])(\d+\.\d*(?:E-?\d+)?|\.\d+(?:E-?\d+)?|\d+E-?\d+)(?![\d.])")
LENGTH = {"TOO_LONG", "NO_ROOM", "DECIMAL_FORM"}
ANGLES = {"0": "DEG", "1": "RAD", "2": "GRAD"}


def exact(m):
    f = Fraction(m.group(1).replace("E", "e")) if "E" not in m.group(1) else Fraction(m.group(1).replace("E", "e"))
    return "(%d)" % f.numerator if f.denominator == 1 else "(%d÷%d)" % (f.numerator, f.denominator)


def record(lines):
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False, encoding="utf-8") as t:
        t.write("".join(l + "\n" for l in lines))
    try:
        out = subprocess.run([GOLDEN, "--record", t.name], capture_output=True, text=True, check=True).stdout
    finally:
        os.unlink(t.name)
    return [(l.split("\t") + ["", ""])[10:12] for l in out.split("\n") if l and not l.startswith("#")]


canon_memo = {}


def canon(t, angle):
    key = (t, angle)
    if key not in canon_memo:
        canon_memo[key] = subprocess.run([CLI, "--arena", "524280", "--decimal", "simplify", t, angle],
                                         capture_output=True, text=True).stdout.strip()
    return canon_memo[key]


def parts(a):
    out = []
    for p in a.split("; "):
        out += [s for s in re.split(r"[=<>≠]", p.split(": ", 1)[-1]) if s]
    return out


def one_value(a, b, angle):
    pa, pb = parts(a), parts(b)
    return len(pa) == len(pb) and all(x == y or canon(x, angle) == canon(y, angle) for x, y in zip(pa, pb))


def judge(typed, untyped, ins):
    bad, status_moved = [], 0
    for (st1, a1), (st2, a2), line in zip(typed, untyped, ins):
        angle = ANGLES[line.split("\t")[3]]
        if st2 == "OK" and "." in a2:
            bad.append(("a decimal from no typed decimal", line, a2))
        if st1 != st2:
            if {st1, st2} <= LENGTH | {"OK"}:
                status_moved += 1
            else:
                bad.append(("status %s -> %s" % (st1, st2), line, a2))
        elif st1 in ("OK", "DECIMAL_FORM", "PARTIAL") and not one_value(a1, a2, angle):
            bad.append(("another value", line, a1 + "  |  " + a2))
    return bad, status_moved


def main():
    ins = []
    for line in open(os.path.join(ROOT, "tests", "golden", "corpus.txt"), encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        f = line.rstrip("\n").split("\t")[:10]
        if any(LIT.search(x) for x in f[7:10]) and ".." not in "".join(f[7:10]):
            ins.append(f)
    typed = record(["\t".join(f) for f in ins])
    rewritten = [f[:7] + [LIT.sub(exact, x) for x in f[7:10]] for f in ins]
    untyped = record(["\t".join(f) for f in rewritten])
    lines = ["\t".join(f) for f in rewritten]
    # the planted faults: a decimal left in (one line keeps its typed text), an answer changed (2 for 1 in one number)
    p1 = [list(t) for t in untyped]
    k = next(i for i, (s, a) in enumerate(typed) if s == "OK" and "." in a)
    p1[k] = list(typed[k])
    caught1 = any(b[1] == lines[k] for b in judge(typed, p1, lines)[0])
    p2 = [list(t) for t in untyped]
    j = next(i for i, (s, a) in enumerate(untyped) if s == "OK" and re.search(r"(?<![\d.])1(?![\d.])", a))
    p2[j] = ["OK", re.sub(r"(?<![\d.])1(?![\d.])", "2", untyped[j][1], count=1)]
    caught2 = any(b[1] == lines[j] for b in judge(typed, p2, lines)[0])
    bad, moved = judge(typed, untyped, lines)
    print("typed values: %d golden inputs with a typed decimal, each rewritten as fractions; %d statuses moved by "
          "length or arena; the planted faults %s" % (len(ins), moved,
                                                      "refused" if caught1 and caught2 else "NOT refused"))
    for why, line, a in bad[:10]:
        print("FAIL %s: %s\n  %s" % (why, line.replace("\t", " | "), a))
    print("checks: %d failed: %d" % (len(ins), len(bad)))
    sys.exit(1 if bad or not (caught1 and caught2) else 0)


if __name__ == "__main__":
    main()
