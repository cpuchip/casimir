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
    m = re.fullmatch(r"(-?)∫\((.*)\)d([A-Z])(?:÷(.+?))?, ([A-Z])=(.*)", t)
    if m:
        minus, h, letter, a, _, l = m.groups()
        a = (sv.value(a, x, deg) if a else 1.0) * (-1 if minus else 1)
        dl = sv.deriv(l, x, deg)
        lv = sv.value(l, x, deg)
        if a is None or dl is None or lv is None:
            return None
        hv = sv.value(re.sub(rf"(?<![A-Z]){letter}(?![A-Z(])", "(" + repr(lv) + ")", h), x, deg)
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


def top_terms(t):
    """t's terms at its top, each with its sign: [("+"/"-", text)]"""
    out, depth, cur, sign = [], 0, "", "+"
    for i, ch in enumerate(t):
        a = t[i - 1] if i else ""
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if depth == 0 and ch in "+-" and i > 0 and a not in "(^×÷" and not (a == "E" and i > 1 and t[i - 2].isdigit()):
            out.append((sign, cur))
            cur, sign = "", ch
            continue
        if i == 0 and ch == "-":
            sign = "-"
            continue
        cur += ch
    out.append((sign, cur))
    return out


def parts_derivative(t, x, deg):
    """a BY PARTS template's right side, P ± c×∫(g)dX (labels removed): P' ± c×g at x; None where undefined"""
    d = 0.0
    for sign, term in top_terms(t):
        m = re.fullmatch(r"(?:(.*)×)?∫\((.*)\)d[A-Z]", term)
        v = (sv.value(m.group(1), x, deg) if m.group(1) else 1.0) if m else None
        part = (v * sv.value(m.group(2), x, deg) if v is not None and sv.value(m.group(2), x, deg) is not None
                else None) if m else sv.deriv(term, x, deg)
        if part is None:
            return None
        d += part if sign == "+" else -part
    return d


def parts_labels(rest):
    """'T, U=u, V=∫(dv)dX' (any two letters) as (T, u, dv), or None"""
    m = list(re.finditer(r", ([A-Z])=", rest))
    if len(m) < 2 or not rest.startswith("∫(", m[-1].end()) or not re.search(r"\)d[A-Z]$", rest):
        return None
    i, j = m[-2], m[-1]
    return rest[:i.start()], rest[i.end():j.start()], rest[j.end() + 2:-3]


def unbracket(s):
    if s.startswith("(") and s.endswith(")"):
        depth = 0
        for i, ch in enumerate(s):
            depth += {"(": 1, ")": -1}.get(ch, 0)
            if depth == 0 and i < len(s) - 1:
                return s
        return s[1:-1]
    return s


def parts_wrong(text, deg):
    """BY PARTS's template: T's derivative is f, and its labels hold: f = U×dv, V' = dv, and P = U×V"""
    f, rest = split_int(text)
    lab = parts_labels(rest)
    if not lab:
        return f"no U and V labels: {rest}"
    t, u, dv = lab
    p = top_terms(t)[0][1]
    if not (p.startswith(u + "×") or p.startswith("(" + u + ")×")):
        return f"P {p} is not U×V with U={u}"
    v = unbracket(p[p.index("×", len(u)) + len("×"):] if p.startswith(u + "×") else p[len(u) + 3:])
    for x in POINTS:
        fv, d = sv.value(f, x, deg), parts_derivative(t, x, deg)
        uv, dvv, vd = sv.value(u, x, deg), sv.value(dv, x, deg), sv.deriv(v, x, deg)
        if None in (fv, d, uv, dvv, vd):
            continue
        if not sv.close(d, fv):
            return f"at X={x}: the step's derivative {d!r}, the integrand {fv!r}"
        if not sv.close(uv * dvv, fv):
            return f"at X={x}: U×dv is {uv * dvv!r}, the integrand {fv!r}"
        if not sv.close(vd, dvv):
            return f"at X={x}: V' is {vd!r}, dv {dvv!r}"
    return ""


def in_x(text):
    """a line in another letter (a substitution's piece, ∫(G)dU or d/dU(...)) read in X, which the evaluator varies:
    the letter renamed where X isn't in the line; the line as it is otherwise"""
    m = re.search(r"\)d([A-Z])(?:$|[ ,÷])", text) or re.match(r"d/d([A-Z])\(", text)
    if not m or m.group(1) == "X" or re.search(r"(?<![A-Z])X(?![A-Z(])", text):
        return text
    return re.sub(rf"(?<![A-Z]){m.group(1)}(?![A-Z(])", "X", text)


JUXTA = re.compile(r"\)(?:d[A-Z])?\s*[∫(]")   # casim writes every product with ×: ")(" or ")dX∫(" reads as nothing


def wrong(rule, line, text, deg):
    if JUXTA.search(text):              # the cold review: ∫(a)dX∫(-b)dX, which the evaluator abstains on
        return f"two factors side by side, with no operator: {text}"
    if rule in PF_RULES or rule == "COMPLETE THE SQUARE":
        return ""                       # not an integral: rational_wrong judges these
    text = in_x(text)
    if rule == "BY PARTS" and line == "T":
        return parts_wrong(text, deg)
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


def steps(e, deg, one, arena=BIG, var="X", angle=None, model="35", cap=None):
    args = [CLI, "--tsv", "--model", model] + arena + (["--one-line"] if one else []) + (
        ["--cap", str(cap)] if cap else []) + [
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
    """(text, var, angle, model) of every integrate in the golden corpus, docs/language.md, work/017-steps-examples.txt
    and AIMED"""
    out = set(AIMED)
    for line in open(os.path.join(ROOT, "tests", "golden", "corpus.txt"), encoding="utf-8"):
        f = line.rstrip("\n").split("\t")
        if f[0] == "integrate" and "\\" not in f[7]:
            out.add((f[7], chr(65 + int(f[2])), ANGLE[f[3]], MODEL.get(f[1], "35")))
    for doc in ("docs/language.md", "work/017-steps-examples.txt"):
        for m in re.finditer(r'`casim ((?:--\S+ \S+ )*)(?:steps )?integrate "([^"]+)" ([A-Z]) (RAD|DEG|GRAD)`',
                             open(os.path.join(ROOT, doc), encoding="utf-8").read()):
            mm = re.search(r"--model (\S+)", m.group(1))
            out.add((m.group(2), m.group(3), m.group(4), mm.group(1) if mm else "35"))
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
ib_n = [0]


def rational(e, var, model):
    """whether integrate answered e through CAS 013's path: the table's own walk refuses it (the walk's first page
    is one of 013's, or its last is integrate's answer after a 013 rule)"""
    rc, out = cli("--tsv", "--model", model, "steps", "integrate", e, var, "RAD")
    return rc == 0 and not ib_walk(out) and any(r.split("\t")[3] in ("PARTIAL FRACTIONS", "SPLIT", "LOG DERIVATIVE",
                                                                        "ARCTANGENT", "COMPLETE THE SQUARE")
                                                for r in out.split("\n"))


IB_RULES = {"SUBSTITUTION", "BY PARTS", "CYCLE"}


def ib_walk(out):
    """whether a walk (TSV) is CAS 017's: one of its rules anywhere, or its root a constant factor that isn't a number
    (work/017 §Steps S1.5) or a sum's (S1.1, a sum only that path answers)"""
    rows = [r.split("\t") for r in out.rstrip("\n").split("\n") if r.count("\t") >= 4]
    if any(r[3] in IB_RULES for r in rows):
        return True
    if rows and rows[0][1] == "0" and rows[0][3] == "CONSTANT MULTIPLE" and rows[0][2] == "T":
        k = split_int(rows[0][4])[1].split("×∫(")[0]
        return bool(re.search(r"[A-Zπ]", k))
    return False


def ln_args(text):
    """the arguments of each LN in text, with whether it is under ABS"""
    out = []
    for m in re.finditer(r"LN\(", text):
        i, depth = m.end(), 1
        for j in range(i, len(text)):
            depth += {"(": 1, ")": -1}.get(text[j], 0)
            if depth == 0:
                break
        a = text[i:j]
        out.append((a[4:-1], True) if a.startswith("ABS(") and a.endswith(")") else (a, False))
    return out


def quad_disc(a):
    """b^2-4ac of a, when a is a quadratic in X (by its values at -1, 0, 1, 2), else None"""
    if not re.fullmatch(r"[-+0-9X^×÷.()]+", a):
        return None
    v = [sv.value(a, x, False) for x in (-1.0, 0.0, 1.0, 2.0)]
    if None in v:
        return None
    c, b, q = v[1], (v[2] - v[0]) / 2, (v[2] + v[0]) / 2 - v[1]
    if abs(q) < 1e-12 or abs(v[3] - (q * 4 + b * 2 + c)) > 1e-9:
        return None
    return b * b - 4 * q * c


def ib_wrong(rows):
    """CAS 017's walk, beyond each line's value: no chained power (013's S2.4), and S2.6's LN forms on its results"""
    why = []
    for r in rows:
        if CHAIN.search(r[4]):
            why.append(f"step {r[0]}: a chained power: {r[4]}")
        if r[2] != "R" or r[3] not in IB_RULES:
            continue
        for a, absd in ln_args(split_int(r[4])[1]):
            disc = quad_disc(a)
            if absd and disc is not None and disc < 0:
                why.append(f"step {r[0]}: LN(ABS({a})), whose argument has no real root (S2.6): {r[4]}")
            if re.fullmatch(r"[-+0-9X^×]+(\.[0-9]+[-+0-9X^×]*)+", a):
                why.append(f"step {r[0]}: LN of {a}, not in whole coefficients (S2.6): {r[4]}")
    return why


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


iv_spec = importlib.util.spec_from_file_location("integratevalues", os.path.join(ROOT, "scripts", "integrate-values.py"))
iv = importlib.util.module_from_spec(iv_spec)
iv_spec.loader.exec_module(iv)


def check_ib(n, rnd):
    """CAS 017's walks (work/017 §Steps) on integrate-values's seeded families, both ways, in each family's angle mode
    and both models: every line by value, ib_wrong, the last page integrate's answer; and at the 8 KB budget the walk
    still pages to integrate's answer (S2.4: never a refusal where integrate answers). Returns (walks, failures)"""
    judged = failed = 0
    for _ in range(n):
        kind, e, angle = iv.gen_case(rnd)
        model = rnd.choice(["35", "35", "STU"])
        rc, plain = cli("--model", model, *BIG, "integrate", e, "X", angle)
        if rc != 0:                     # a refusal: the steps refuse with integrate's own status (the letter rule's)
            src, sout = steps(e, False, False, var="X", angle=angle, model=model)
            if src == 0 or sout.strip().split("\n")[-1] != plain:
                failed += 1
                print(f"FAIL scripts/istep-values.py: {e} ({kind}, {angle}): integrate {plain}, its steps end "
                      f"{sout.strip().split(chr(10))[-1][:80]}")
            continue
        for arena in (BIG, []):
            rc_a, plain_a = cli("--model", model, *arena, "integrate", e, "X", angle)
            if rc_a != 0:
                continue
            for one in (False, True):
                rc, out = steps(e, False, one, arena=arena, var="X", angle=angle, model=model)
                why = []
                if rc != 0:
                    why.append(f"steps exit {rc} where integrate answers: {out.strip()[:80]}")
                else:
                    rows = [r.split("\t") for r in out.rstrip("\n").split("\n")]
                    if [int(r[0]) for r in rows] != list(range(len(rows))):
                        why.append("k is not 0..count-1")
                    if rows[-1][1] != "0" or rows[-1][2] != "R" or split_int(rows[-1][4])[1] != plain_a:
                        why.append(f"the last step {rows[-1][4]} is not integrate's {plain_a}")
                    if arena == BIG and not one:
                        judged += 1
                        if steps(e, False, one, arena=arena, var="X", angle=angle, model=model)[1] != out:
                            why.append("a second run differs")
                        for r in rows:
                            w = wrong(r[3], r[2], r[4], angle)
                            if w:
                                why.append(f"step {r[0]} {r[2]} {r[3]}: {r[4]}: {w}")
                        if ib_walk(out):
                            why += ib_wrong(rows)
                    why += cap_fit(e, plain_a, one, arena, angle, model)
                if why:
                    failed += 1
                    print(f"FAIL scripts/istep-values.py: {e} ({kind}, {angle}, model {model}, "
                          f"{'8 KB' if not arena else 'big arena'}, {'one line' if one else 'two lines'})\n  "
                          + "\n  ".join(why))
    return judged, failed


CAP_FIT = (150, 192)       # abacus #8227 judged at 192, as for integrate


def cap_fit(e, answer, one, arena, angle, model, var="X"):
    """S2.4 at a page cap (abacus #8227): where the one page, ∫(f)dX = F, fits the cap, the steps page to the end at
    that cap, never TOO_LONG on a walk's line (a line past cap makes the walk its one page); where it doesn't fit, the
    one page is TOO_LONG, never half a walk. Returns the failures' reasons"""
    why = []
    page = len(f"∫({e})d{var} = {answer}".encode()) + 1
    for cap in CAP_FIT:
        rc, out = steps(e, False, one, arena=arena, var=var, angle=angle, model=model, cap=cap)
        rows = out.rstrip("\n").split("\n") if out.strip() else []
        if page <= cap and rc != 0:
            why.append(f"--cap {cap}: the one page fits ({page} bytes) but the steps end {rows[-1][:60] if rows else ''}")
        if page > cap and len(rows) > 1:
            why.append(f"--cap {cap}: the one page doesn't fit ({page} bytes), yet {len(rows) - 1} pages showed first")
    return why


def planted_ib():
    """the judges of CAS 017's walk must refuse these"""
    bad = []
    rc, out = steps("SIN(A×X)×COS(X)", False, False)
    if rc == 0:
        bad.append("(the control: the steps of SIN(A×X)×COS(X), which integrate refuses, paged)")
    good = "∫(LN(X))dX = LN(X)×X-∫(1)dX, U=LN(X), V=∫(1)dX"
    if wrong("BY PARTS", "T", good, "RAD"):
        bad.append(f"(the control: {good} was refused)")
    for name, t in [("BY PARTS with w lacking u'", "∫(LN(X))dX = LN(X)×X-∫(X)dX, U=LN(X), V=∫(1)dX"),
                    ("BY PARTS with a wrong V", "∫(X×EXP(X))dX = X×EXP(X)-∫(EXP(X))dX, U=X, V=∫(X×EXP(X))dX"),
                    ("BY PARTS with U×dv not the integrand", "∫(X×EXP(X))dX = X×EXP(X)-∫(EXP(X))dX, U=X^2, V=∫(EXP(X))dX"),
                    ("the cycle's second part with -k's sign turned",
                     "∫(EXP(X)×COS(X))dX = COS(X)×EXP(X)-∫(EXP(X)×SIN(X))dX, U=COS(X), V=∫(EXP(X))dX"),
                    ("BY PARTS without labels", "∫(LN(X))dX = LN(X)×X-∫(1)dX")]:
        if not wrong("BY PARTS", "T", t, "RAD"):
            bad.append(name)
    if not wrong("SUBSTITUTION", "T", "∫(X×EXP(X^2))dX = ∫(EXP(W))dW, W=X^2", "RAD"):
        bad.append("a substitution in W without its 0.5")
    if not wrong("CYCLE", "R", "∫(EXP(X)×SIN(X))dX = (SIN(X)×EXP(X)-COS(X)×EXP(X))÷(-2)", "RAD"):
        bad.append("the cycle's 1-k with its sign turned")
    if not wrong("SUM", "T", "∫(LN(X)-X×EXP(X))dX = ∫(LN(X))dX∫(-X×EXP(X))dX", "RAD"):
        bad.append("a SUM with no operator between two integrals")
    rows = [["0", "0", "R", "SUBSTITUTION", "∫(2×X÷(X^2+1))dX = LN(ABS(X^2+1))"]]
    if not ib_wrong(rows):
        bad.append("LN(ABS(X^2+1)), an ABS on a quadratic with no real root")
    rows = [["0", "0", "R", "SUBSTITUTION", "∫(15×X^2÷(5×X^3+2))dX = LN(ABS(X^3+0.4))"]]
    if not ib_wrong(rows):
        bad.append("LN(ABS(X^3+0.4)), not in whole coefficients")
    return bad


def check_caps():
    """casim_step's contract on CAS 017's walks: a page past cap is TOO_LONG, whatever page and however deep its piece
    (the cold review: BAD_ARG for a k below the count). The CLI stops at the first page that isn't OK"""
    failed = n = 0
    for line in open(os.path.join(ROOT, "work", "017-steps-examples.txt"), encoding="utf-8"):
        m = re.match(r'\| `casim ((?:--\S+ \S+ )*)steps integrate "([^"]+)" ([A-Z]) (RAD|DEG|GRAD)`', line)
        if not m:
            continue
        frc, full = cli(*m.group(1).split(), "steps", "integrate", m.group(2), m.group(3), m.group(4))
        refusal = full.split("\n")[-1] if frc != 0 else None        # a walk integrate refuses ends so at any cap
        for cap in (24, 48, 90, 150, 260):
            for one in ([], ["--one-line"]):
                n += 1
                args = m.group(1).split() + ["--cap", str(cap), "--tsv"] + one + ["steps", "integrate", m.group(2),
                                                                                m.group(3), m.group(4)]
                rc, out = cli(*args)
                last = out.split("\n")[-1] if out else ""
                if rc != 0 and last.split("\t")[-1] != "TOO_LONG" and last != "TOO_LONG" and last != refusal:
                    failed += 1
                    print(f"FAIL scripts/istep-values.py: {m.group(2)} with --cap {cap} {' '.join(one)}: ends {last[:80]}")
        if refusal is None:
            prc, plain = cli(*m.group(1).split(), "integrate", m.group(2), m.group(3), m.group(4))
            arena = [a for a in m.group(1).split()]
            for one in (False, True):
                n += 1
                why = cap_fit(m.group(2), plain, one, arena, m.group(4), "35", var=m.group(3))
                if why:
                    failed += 1
                    print(f"FAIL scripts/istep-values.py: {m.group(2)}: " + "; ".join(why))
    print(f"CAS 017 walks paged at 5 caps and fitted at {len(CAP_FIT)}: {n} runs, {failed} failed")
    return failed


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
            if ib_walk(out):
                ib_n[0] += 1
                why += ib_wrong(rows)
            elif rational(e, var, model):
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
          f" ({rational_n[0]} rational ones through CAS 013's walk, {ib_n[0]} through CAS 017's)")
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
    planted17 = planted_ib()
    for b in planted17:
        print(f"FAIL scripts/istep-values.py: its CAS 017 judge accepts {b}")
    ibn, ibfailed = check_ib(120, random.Random(20261014))
    ibfailed += check_caps()
    print(f"integrate step values: {ok} of {n} expressions integrable, both ways, {lines} steps; {soft} fit 8 KB plain "
          f"but not with steps; the judge refused its three planted steps\nCAS 013 walks: {rn} seeded rational "
          f"integrands, both ways, in RAD, DEG and GRAD, 35 and STU; the judge refused its {8 - len(planted)} of 8 "
          f"planted faults\nCAS 017 walks: {ibn} of integrate-values's seeded integrands, both ways, at both arenas; "
          f"the judge refused its {10 - len(planted17)} of 10 planted faults\n"
          f"checks: {2 * ok + 2 * rn + 2 * ibn} failed: {failed + known + rfailed + len(planted) + ibfailed + len(planted17)}")
    sys.exit(1 if failed or known or rfailed or planted or ibfailed or planted17 else 0)


if __name__ == "__main__":
    main()
