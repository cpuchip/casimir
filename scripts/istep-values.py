#!/usr/bin/env python3
"""integrate's steps judged by value (make check; work/steps-integrate.md). Seeded random integrable sums go through
`build/casim --tsv steps integrate`, both ways, and each step is evaluated in Python's math (scripts/step-values.py's
evaluator, which shares no code with casim):
  - a result line, ∫(f)dX = F, and a CHECK line, d/dX(F) = f: F's derivative (a central difference) is f;
  - a template line, ∫(f)dX = T: T's derivative is f, taking each ∫(w)dX as w and a substitution
    ∫(H(U))dU÷a, U=L as H(L)×L'÷a (so a wrong a or L is seen);
  - the root's result (the last step) is plain integrate's answer, byte for byte; k runs 0..count-1; a second run is
    identical;
  - every integrate the golden corpus and docs/language.md hold that integrate answers, in its own model, variable and
    angle, pages to the end (a walk that refuses where integrate answers, but for NO_ROOM, fails: abacus #6252), its
    steps judged as above;
  - CAS 013's walks for rational integrands (work/013-rational-integrals.md §Steps), on those inputs and on seeded
    rational ones in every angle mode and both models: partfrac's pages inside the walk byte for byte its own walk's
    (scripts/psteps-values.py judges those), a level down; COMPLETE THE SQUARE as an equation by value; PARTIAL
    FRACTIONS present exactly when partfrac's answer and the integrand have different canonical forms (S2.2, by casim
    simplify, the definition); COMPLETE THE SQUARE absent exactly for X^2+k (S2.3); no page with a chained power
    (S2.4: a number, or a bracketed group, between two ^).
    scripts/istep-values.py [N] [SEED]     (default 600 expressions, seed 20261010)"""
import importlib.util
import os
import random
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "build", "casim")
spec = importlib.util.spec_from_file_location("stepvalues", os.path.join(ROOT, "scripts", "step-values.py"))
sv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sv)
POINTS = [0.37, 0.81, 1.23]
LIN = ["X", "2×X", "2×X+1", "3-X", "X+0.5", "0.5×X-1"]


def term(rnd):
    r, u = rnd.random(), rnd.choice(LIN)
    if r < 0.1:                         # rule 6a's spellings of a power of X (abacus #6252)
        return rnd.choice(["", "2×", "(-3)×", "4×"]) + rnd.choice(["SQRT(X)^3", "INV(X^2)", "SQ(X)", "INV(X)", "X^2÷X^5",
                                                                  "SQRT(X)×X", "X÷SQRT(X)", "INV(SQRT(X))"])
    if r < 0.25:
        return rnd.choice(["", "2×", "3×", "0.5×", "-", "π×"]) + rnd.choice(["X", "X^2", "X^3", "X^(-2)", "SQRT(X)", "1"])
    if r < 0.55:
        return rnd.choice(["", "2×", "-3×", "0.5×"]) + rnd.choice(["SIN", "COS", "EXP", "SINH", "COSH", "ALOG"]) + "(" + u + ")"
    if r < 0.7:
        return rnd.choice(["1÷(" + u + ")", "3÷(" + u + ")", "(" + u + ")^3", "SQRT(" + u + ")", "(" + u + ")^(-2)"])
    if r < 0.85:
        return "(" + rnd.choice(LIN) + ")×(" + rnd.choice(LIN) + ")"
    return rnd.choice(["2×(", "-(", "(", "0.5×("]) + term(rnd) + "+" + term(rnd) + ")"


def deriv_value(expr, x, deg):
    return sv.deriv(expr, x, deg)


def template_derivative(t, x, deg):
    """T's derivative at x: each ∫(w)dX as w, each ∫(H)dU÷a, U=L as H(L)×L'(x)÷a; None where undefined"""
    m = re.fullmatch(r"(-?)∫\((.*)\)d([UV])(?:÷(.+?))?, ([UV])=(.*)", t)
    if m:
        minus, h, letter, a, _, l = m.groups()
        a = (sv.value(a, x, deg) if a else 1.0) * (-1 if minus else 1)
        dl = sv.deriv(l, x, deg)
        lv = sv.value(l, x, deg)
        if a is None or dl is None or lv is None:
            return None
        hv = sv.value(h.replace(letter, "(" + repr(lv) + ")"), x, deg)
        return None if hv is None else hv * dl / a
    out, i = "", 0
    while i < len(t):
        if t.startswith("∫(", i):
            depth, j = 0, i + 1
            for j in range(i + 1, len(t)):
                depth += {"(": 1, ")": -1}.get(t[j], 0)
                if depth == 0:
                    break
            out += "(" + t[i + 2:j] + ")"
            i = j + 3                   # past ")dX"
        else:
            out += t[i]
            i += 1
    return sv.value(out, x, deg)


def split_int(text):
    """'∫(f)dX = rest' as (f, rest)"""
    depth = 0
    for i in range(1, len(text)):
        depth += {"(": 1, ")": -1}.get(text[i], 0)
        if depth == 0:
            break
    return text[2:i], text[i + 3:].removeprefix(" = ")


def wrong(rule, line, text, deg):
    if rule in PF_RULES or rule == "COMPLETE THE SQUARE":
        return ""                       # not an integral: rational_wrong judges these
    judged = 0
    for x in POINTS:
        if rule == "CHECK":
            f_side = text.split(") = ", 1)
            F, f = f_side[0][len("d/dX("):], f_side[1]
            d, v = sv.deriv(F, x, deg), sv.value(f, x, deg)
        else:
            f, rest = split_int(text)
            v = sv.value(f, x, deg)
            d = sv.deriv(rest, x, deg) if line == "R" else template_derivative(rest, x, deg)
        if d is None or v is None:
            continue
        judged += 1
        if not sv.close(d, v):
            return f"at X={x}: the step's derivative {d!r}, the integrand {v!r}"
    return ""


BIG = ["--arena", "524280"]         # values judged with room to spare; the 8 KB budget's soft NO_ROOM counted apart


ANGLE = {"0": "DEG", "1": "RAD", "2": "GRAD"}
MODEL = {"35": "35", "33": "33", "1": "STU"}


def steps(e, deg, one, arena=BIG, var="X", angle=None, model="35"):
    args = [CLI, "--tsv", "--model", model] + arena + (["--one-line"] if one else []) + [
        "steps", "integrate", e, var, angle or ("DEG" if deg else "RAD")]
    p = subprocess.run(args, stdout=subprocess.PIPE, text=True, encoding="utf-8")
    return p.returncode, p.stdout


# integrate inputs the corpus lacks, kept here: abacus's hidden cases H00075 and H00002 (#6252), where the walk once
# refused what integrate answers, and rule 6a's spellings of a power of X under a constant
AIMED = [("(-3)×SQRT(X)^3", "X", "DEG", "STU"),
         ("2×X×SQRT(X)+(-2)÷((-1÷2)×X-1)+4×INV(X^2)", "X", "DEG", "STU")] + [
    (k + f, "X", "RAD", "35") for k in ("3×", "-2×", "0.5×", "π×") for f in (
        "SQRT(X)^3", "INV(X^2)", "SQ(X)", "INV(X)", "SQRT(X)×X", "X÷SQRT(X)", "INV(SQRT(X))", "X^2÷X^5")]


def known_inputs():
    """(text, var, angle, model) of every integrate in the golden corpus, docs/language.md and AIMED"""
    out = set(AIMED)
    for line in open(os.path.join(ROOT, "tests", "golden", "corpus.txt"), encoding="utf-8"):
        f = line.rstrip("\n").split("\t")
        if f[0] == "integrate" and "\\" not in f[7]:
            out.add((f[7], chr(65 + int(f[2])), ANGLE[f[3]], MODEL.get(f[1], "35")))
    for m in re.finditer(r'`casim (?:steps )?integrate "([^"]+)" ([A-Z]) (RAD|DEG|GRAD)`',
                         open(os.path.join(ROOT, "docs", "language.md"), encoding="utf-8").read()):
        out.add((m.group(1), m.group(2), m.group(3), "35"))
    return sorted(out)


PF_RULES = {"COMMON FACTOR", "POLYNOMIAL PART", "FACTOR", "FORM", "CLEARED", "COVER-UP", "COEFFICIENTS", "ANSWER"}
CHAIN = re.compile(r"\^(\d+(\.\d+)?|\([^()]*\))\^")      # a number, or a bracket, between two ^


def cli(*args):
    p = subprocess.run([CLI, *args], stdout=subprocess.PIPE, text=True, encoding="utf-8")
    return p.returncode, p.stdout.rstrip("\n")


def equation_wrong(text, deg):
    """lhs=rhs by value at the points ("" when right or never defined)"""
    lhs, rhs = text.split("=", 1)
    for x in POINTS:
        a, b = sv.value(lhs, x, deg), sv.value(rhs, x, deg)
        if a is not None and b is not None and not sv.close(a, b):
            return f"at X={x}: {a!r} and {b!r}"
    return ""


def rational_wrong(e, var, angle, model, rows, one):
    """CAS 013's walk (rows of the TSV, already split): what is wrong with it, beyond each line's value"""
    why = []
    m = ["--model", model]
    rc, pf = cli(*m, "partfrac", e, var, angle)
    _, a = cli(*m, "simplify", e, angle)
    _, b = cli(*m, "simplify", pf, angle)
    first = rows[0][3] == "PARTIAL FRACTIONS"
    changed = a != b
    if not one and first != changed:
        why.append(f"PARTIAL FRACTIONS {'present' if first else 'absent'}, but partfrac's answer {pf} "
                   f"{'is' if not changed else 'is not'} the integrand's canonical form (S2.2)")
    if changed:
        # partfrac's own pages, a level down, right after the root's template (or first, in one line)
        rc, ps = cli("--tsv", *m, *(["--one-line"] if one else []), "steps", "partfrac", e, var, angle)
        want = [(str(int(r.split("\t")[1]) + 1), r.split("\t")[2], r.split("\t")[3], r.split("\t")[4])
                for r in ps.split("\n")]
        got = [tuple(r[1:5]) for r in rows[0 if one else 1:][:len(want)]]
        if got != want:
            why.append("partfrac's pages are not its own walk's, a level down")
    for r in rows:
        if CHAIN.search(r[4]):
            why.append(f"step {r[0]}: a chained power: {r[4]}")
        if r[3] == "LOG DERIVATIVE" and "ABS(" in r[4]:
            why.append(f"step {r[0]}: LOG DERIVATIVE with an ABS (f has no real root: rule 4): {r[4]}")
        if r[3] == "COMPLETE THE SQUARE":
            f = r[4].split("=", 1)[0]
            if re.fullmatch(rf"{var}\^2\+[\d÷]+", f):
                why.append(f"step {r[0]}: COMPLETE THE SQUARE on {f} (S2.3 skips it)")
            w = equation_wrong(r[4], angle) if var == "X" else ""
            if w:
                why.append(f"step {r[0]} COMPLETE THE SQUARE: {r[4]}: {w}")
        if r[3] == "ARCTANGENT" and r[2] == "R" and not one:
            fm = re.fullmatch(r"1÷(\(.*\))", split_int(r[4])[0])
            if not fm:
                continue                # the root's line (S2.2): the integrand as typed
            f = fm.group(1)
            plain_square = re.fullmatch(rf"\({var}\^2\+[\d÷]+\)", f)
            has = any(q[3] == "COMPLETE THE SQUARE" and "(" + q[4].split("=", 1)[0] + ")" == f for q in rows)
            if not plain_square and not has:
                why.append(f"step {r[0]}: no COMPLETE THE SQUARE for {f}")
    return why


rational_n = [0]


def rational(e, var, model):
    """whether integrate answered e through CAS 013's path: the table's own walk refuses it (the walk's first page
    is one of 013's, or its last is integrate's answer after a 013 rule)"""
    rc, out = cli("--tsv", "--model", model, "steps", "integrate", e, var, "RAD")
    return rc == 0 and any(r.split("\t")[3] in ("PARTIAL FRACTIONS", "SPLIT", "LOG DERIVATIVE", "ARCTANGENT",
                                                 "COMPLETE THE SQUARE") for r in out.split("\n"))


def poly_text(p):
    s = ""
    for i in range(len(p) - 1, -1, -1):
        a = p[i]
        if a == 0:
            continue
        s += ("-" if a < 0 else "+" if s else "") + ("" if abs(a) == 1 and i else str(abs(a))) + \
             ("×" if abs(a) != 1 and i else "") + ("X" if i else "") + (f"^{i}" if i > 1 else "")
    return s or "0"


def rational_integrand(rnd):
    """n÷d: d a product of linear factors to powers up to 3 and quadratics with no real root, degree <= 9"""
    def mul(p, q):
        r = [0] * (len(p) + len(q) - 1)
        for i, a in enumerate(p):
            for j, b in enumerate(q):
                r[i + j] += a * b
        return r
    while True:
        d = [1]
        for _ in range(rnd.randint(1, 3)):
            if rnd.random() < 0.6:
                f = [rnd.randint(-5, 5), rnd.choice([1, 1, 2, 3])]
                for _ in range(rnd.choice([1, 1, 2, 3])):
                    d = mul(d, f)
            else:
                a, b = rnd.choice([1, 1, 2]), rnd.randint(-4, 4)
                d = mul(d, [b * b // (4 * a) + rnd.randint(1, 5), b, a])
        if len(d) - 1 <= 9:
            break
    n = [rnd.randint(-6, 6) for _ in range(rnd.randint(0, len(d) - 1 if rnd.random() < 0.8 else len(d)) + 1)]
    if not any(n):
        n[0] = 1
    return f"({poly_text(n)})÷({poly_text(d)})"


def check_rational(n, rnd):
    """seeded rational integrands, through CAS 013's walk; returns (walks judged, failures)"""
    judged = failed = 0
    for _ in range(n):
        e = rational_integrand(rnd)
        angle, model = rnd.choice(["RAD", "DEG", "GRAD"]), rnd.choice(["35", "35", "STU"])
        rc, plain = cli("--model", model, *BIG, "integrate", e, "X", angle)
        if rc != 0 or not rational(e, "X", model):
            continue                    # refused (rule 5), too long, or the table's own
        judged += 1
        for one in (False, True):
            rc, out = steps(e, False, one, var="X", angle=angle, model=model)
            why = []
            if rc != 0:
                why.append(f"steps exit {rc}: {out.strip()[:80]}")
            else:
                rows = [r.split("\t") for r in out.rstrip("\n").split("\n")]
                if [int(r[0]) for r in rows] != list(range(len(rows))):
                    why.append("k is not 0..count-1")
                if rows[-1][1] != "0" or rows[-1][2] != "R" or split_int(rows[-1][4])[1] != plain:
                    why.append(f"the last step {rows[-1][4]} is not the root's result {plain}")
                if steps(e, False, one, var="X", angle=angle, model=model)[1] != out:
                    why.append("a second run differs")
                for r in rows:
                    w = wrong(r[3], r[2], r[4], angle)
                    if w:
                        why.append(f"step {r[0]} {r[2]} {r[3]}: {r[4]}: {w}")
                why += rational_wrong(e, "X", angle, model, rows, one)
            if why:
                failed += 1
                print(f"FAIL scripts/istep-values.py: {e} ({angle}, model {model}, {'one line' if one else 'two lines'})\n  "
                      + "\n  ".join(why))
    return judged, failed


def planted_rational():
    """the judges of CAS 013's walk must refuse these"""
    bad = []
    if not wrong("SPLIT", "T", "∫((X+3)÷(X^2+2×X+5))dX = 1×∫((2×X+2)÷(X^2+2×X+5))dX+2×∫(1÷(X^2+2×X+5))dX", "RAD"):
        bad.append("SPLIT with k1 off")
    if not wrong("ARCTANGENT", "T", "∫(1÷(4×X^2+9))dX = ∫(1÷(U^2+9÷4))dU, U=X", "RAD"):
        bad.append("ARCTANGENT without its ÷a")
    if not wrong("LOG DERIVATIVE", "R", "∫((2×X+2)÷(X^2+2×X+5))dX = LN(ABS(X^2+2×X+5))+X", "RAD"):
        bad.append("a LOG DERIVATIVE result off by X")
    if not equation_wrong("X^2+2×X+5=(X+1)^2+5", "RAD"):
        bad.append("a COMPLETE THE SQUARE off by 1")
    if not CHAIN.search("X^2^3") or not CHAIN.search("(X+1)^2^3"):
        bad.append("a chained power")
    rc, out = steps("(5×X+1)÷(X^2+X-2)", False, False)
    rows = [r.split("\t") for r in out.rstrip("\n").split("\n")]
    if rational_wrong("(5×X+1)÷(X^2+X-2)", "X", "RAD", "35", rows, False):
        bad.append("(the control: the real walk of (5×X+1)÷(X^2+X-2) was refused)")
    spoiled = [r[:] for r in rows]
    spoiled[2][4] = spoiled[2][4].replace("A", "B", 1)
    if not rational_wrong("(5×X+1)÷(X^2+X-2)", "X", "RAD", "35", spoiled, False):
        bad.append("a partfrac page that is not partfrac's")
    if not rational_wrong("(5×X+1)÷(X^2+X-2)", "X", "RAD", "35", rows[1:], False):
        bad.append("PARTIAL FRACTIONS skipped where partfrac changed the integrand")
    rc, out = steps("(X+3)÷(X^2+2×X+5)", False, False)
    rows = [r.split("\t") for r in out.rstrip("\n").split("\n")]
    spoiled = [r[:] for r in rows]
    for r in spoiled:
        if r[3] == "LOG DERIVATIVE" and r[2] == "R":
            r[4] = r[4].replace("= LN(", "= LN(ABS(") + ")"
    if not rational_wrong("(X+3)÷(X^2+2×X+5)", "X", "RAD", "35", spoiled, False):
        bad.append("ABS on LOG DERIVATIVE's LN")
    return bad


def check_known():
    """every known integrate input integrate answers must page to the end, right by value; returns the failures"""
    failed = n = 0
    for e, var, angle, model in known_inputs():
        p = subprocess.run([CLI, "--model", model] + BIG + ["integrate", e, var, angle], stdout=subprocess.PIPE,
                           text=True, encoding="utf-8")
        if p.returncode != 0:
            continue
        n += 1
        rc, out = steps(e, False, False, var=var, angle=angle, model=model)
        last = out.rstrip("\n").split("\n")[-1]
        why = []
        if rc != 0 and last != "NO_ROOM":
            why.append(f"integrate answers, its steps end {last}")
        elif rc == 0:
            rows = [r.split("\t") for r in out.rstrip("\n").split("\n")]
            if split_int(rows[-1][4])[1] != p.stdout.strip():
                why.append(f"the last step {rows[-1][4]} is not integrate's {p.stdout.strip()}")
            if rational(e, var, model):
                rational_n[0] += 1
                why += rational_wrong(e, var, angle, model, rows, False)
            if var == "X":              # the evaluator knows X (and Y as a constant)
                for r in rows:
                    w = wrong(r[3], r[2], r[4], angle)
                    if w:
                        why.append(f"step {r[0]} {r[3]}: {r[4]}: {w}")
        if why:
            failed += 1
            print(f"FAIL scripts/istep-values.py: {e} ({var}, {angle}, model {model})\n  " + "\n  ".join(why))
    print(f"integrate steps on the corpus, the docs and {len(AIMED)} aimed inputs: {n} integrate answers, {failed} failed"
          f" ({rational_n[0]} rational ones through CAS 013's walk)")
    return failed


def main():
    for rule, line, text in [("SIN", "R", "∫(SIN(2×X))dX = -COS(2×X)"),
                             ("SIN", "T", "∫(SIN(2×X+1))dX = ∫(SIN(U))dU÷3, U=2×X+1"),
                             ("CHECK", "R", "d/dX(X^3÷3) = X^3")]:
        if not wrong(rule, line, text, False):
            print(f"FAIL scripts/istep-values.py: its judge accepts {text}")
            sys.exit(1)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    rnd = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 20261010)
    failed = lines = ok = soft = 0
    for _ in range(n):
        e = "+".join(term(rnd) for _ in range(rnd.randint(1, 3))).replace("+-", "-")
        deg = rnd.random() < 0.2
        p = subprocess.run([CLI] + BIG + ["integrate", e, "X", "DEG" if deg else "RAD"], stdout=subprocess.PIPE,
                           text=True, encoding="utf-8")
        if p.returncode != 0:
            continue                    # only what integrate answers has steps to judge
        ok += 1
        plain = p.stdout.strip()
        small = subprocess.run([CLI, "integrate", e, "X", "DEG" if deg else "RAD"], stdout=subprocess.PIPE).returncode
        if small == 0 and steps(e, deg, False, [])[0] != 0:
            soft += 1                   # fits the 8 KB budget plain, not with its steps: NO_ROOM, softly
        for one in (False, True):
            rc, out = steps(e, deg, one)
            why = []
            if rc != 0:
                why.append(f"steps exit {rc}: {out.strip()[:80]}")
            else:
                rows = [r.split("\t") for r in out.rstrip("\n").split("\n")]
                if [int(r[0]) for r in rows] != list(range(len(rows))):
                    why.append("k is not 0..count-1")
                if rows[-1][1] != "0" or rows[-1][2] != "R" or rows[-1][3] == "CHECK" or split_int(rows[-1][4])[1] != plain:
                    why.append(f"the last step {rows[-1][4]} is not the root's result {plain}")
                if steps(e, deg, one)[1] != out:
                    why.append("a second run differs")
                for r in rows:
                    lines += 1
                    w = wrong(r[3], r[2], r[4], deg)
                    if w:
                        why.append(f"step {r[0]} {r[2]} {r[3]}: {r[4]}: {w}")
            if why:
                failed += 1
                print(f"FAIL scripts/istep-values.py: {e} ({'DEG' if deg else 'RAD'}, "
                      f"{'one line' if one else 'two lines'})\n  " + "\n  ".join(why))
    known = check_known()
    planted = planted_rational()
    for b in planted:
        print(f"FAIL scripts/istep-values.py: its CAS 013 judge accepts {b}")
    rn, rfailed = check_rational(150, random.Random(20261011))
    print(f"integrate step values: {ok} of {n} expressions integrable, both ways, {lines} steps; {soft} fit 8 KB plain "
          f"but not with steps; the judge refused its three planted steps\nCAS 013 walks: {rn} seeded rational "
          f"integrands, both ways, in RAD, DEG and GRAD, 35 and STU; the judge refused its {8 - len(planted)} of 8 "
          f"planted faults\nchecks: {2 * ok + 2 * rn} failed: {failed + known + rfailed + len(planted)}")
    sys.exit(1 if failed or known or rfailed or planted else 0)


if __name__ == "__main__":
    main()
