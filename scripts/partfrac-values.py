#!/usr/bin/env python3
"""partfrac judged exactly (make check; work/012-partial-fractions.md). Seeded fractions over planted factors, and the
visible examples, go through `build/casim partfrac`; this judge shares no code with casim:
  - n and d are the judge's own (planted), or read from the typed sides by exact evaluation (Lagrange through 13 points);
    the gcd comes out by Euclid over the fractions, as rule 2 says;
  - the factors casim printed must be primitive integer polynomials with a positive lead and no rational root, and the
    reduced d must be their product, each to the highest power printed, times a constant (rule 3: over Q);
  - over those factors the decomposition with deg num_j < deg f is unique, so the judge computes it itself (a linear
    solve, not casim's Euclid), renders it by rule 4 (form) and rule 5 (order), adds CANCEL's rows for the removed
    gcd (`casim cancel` on the same text: rule 2 says the rows are CANCEL's), and compares byte for byte;
  - past 191 bytes the answer must be TOO_LONG, and within them it must not be (rule 8); the answer's value equals the
    typed text's at five points, read by this file's own evaluator;
  - planted faults (a sign, the order, a power order, an unreduced constant, a dropped row, a dropped polynomial part,
    a wrong numerator) must each be refused, and the right answer accepted.
    scripts/partfrac-values.py [N] [SEED]      (default 400 seeded fractions, seed 20261012)"""
import importlib.util
import math
import os
import random
import re
import subprocess
import sys
from fractions import Fraction as F

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "build", "casim")
spec = importlib.util.spec_from_file_location("sstepvalues", os.path.join(ROOT, "scripts", "ssteps-values.py"))
ssv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ssv)
POINTS = [F(3, 7), F(-5, 11), F(13, 9), F(17, 5), F(-29, 13)]


# ---- polynomials over Q, low degree first ----

def trim(a):
    a = [F(x) for x in a]
    while a and a[-1] == 0:
        a.pop()
    return a


def pmul(a, b):
    if not a or not b:
        return []
    r = [F(0)] * (len(a) + len(b) - 1)
    for i, x in enumerate(a):
        for j, y in enumerate(b):
            r[i + j] += x * y
    return trim(r)


def pdivmod(a, b):
    a, b = trim(a), trim(b)
    q = [F(0)] * max(len(a) - len(b) + 1, 1)
    while len(a) >= len(b):
        k = len(a) - len(b)
        c = a[-1] / b[-1]
        q[k] = c
        a = trim([a[i] - (c * b[i - k] if 0 <= i - k < len(b) else 0) for i in range(len(a))])
    return trim(q), a


def ppow(a, k):
    r = [F(1)]
    for _ in range(k):
        r = pmul(r, a)
    return r


def pgcd(a, b):
    a, b = trim(a), trim(b)
    while b:
        a, b = b, pdivmod(a, b)[1]
    return [c / a[-1] for c in a] if a else a


def value(p, x):
    v = F(0)
    for c in reversed(p):
        v = v * x + c
    return v


def has_rational_root(f):
    """f integer, ascending: the rational root test"""
    f = [int(c) for c in f]
    if f[0] == 0:
        return True
    a0, an = abs(f[0]), abs(f[-1])
    for p in range(1, a0 + 1):
        if a0 % p:
            continue
        for q in range(1, an + 1):
            if an % q == 0 and math.gcd(p, q) == 1 and (value(f, F(p, q)) == 0 or value(f, F(-p, q)) == 0):
                return True
    return False


# ---- rule 4's text, written again from the unit ----

def ipoly(a):
    out = []
    for i in range(len(a) - 1, -1, -1):
        c = int(a[i])
        if c == 0:
            continue
        s = "-" if c < 0 else ("+" if out else "")
        m = abs(c)
        x = "" if i == 0 else "X" if i == 1 else f"X^{i}"
        out.append(s + (str(m) if not x else x if m == 1 else f"{m}×{x}"))
    return "".join(out) or "0"


def rpoly(a):
    out = []
    for i in range(len(a) - 1, -1, -1):
        c = F(a[i])
        if c == 0:
            continue
        s = "-" if c < 0 else ("+" if out else "")
        m = abs(c)
        x = "" if i == 0 else "X" if i == 1 else f"X^{i}"
        body = str(m.numerator) if not x else (x if m.numerator == 1 else f"{m.numerator}×{x}")
        out.append(s + body + (f"÷{m.denominator}" if m.denominator != 1 else ""))
    return "".join(out)


def term(num, f, j):
    lcd = math.lcm(*[c.denominator for c in num])
    n = [int(c * lcd) for c in num]
    g = math.gcd(*n)
    if n[-1] < 0:
        g = -g
    n = [x // g for x in n]
    k = F(g, lcd)
    sign, k = ("-" if k < 0 else "+"), abs(k)
    if len(n) == 1:
        body = str(k.numerator)
    else:
        nt = ipoly(n)
        nt = f"({nt})" if sum(1 for x in n if x) > 1 else nt
        body = nt if k.numerator == 1 else f"{k.numerator}×{nt}"
    ft = ipoly(f)
    fv = ft if ft == "X" else f"({ft})"
    if j > 1:
        fv += f"^{j}"
    return sign, body + "÷" + (fv if k.denominator == 1 else f"({k.denominator}×{fv})")


def order_key(f):
    f = [int(c) for c in f]
    if len(f) == 2:
        return (1, F(-f[0], f[1]), ())
    return (len(f) - 1, F(0), tuple(f[::-1]))


def solve(cols, rhs):
    n = len(rhs)
    m = [[cols[j][i] for j in range(n)] + [rhs[i]] for i in range(n)]
    for c in range(n):
        p = next(r for r in range(c, n) if m[r][c] != 0)
        m[c], m[p] = m[p], m[c]
        for r in range(n):
            if r != c and m[r][c] != 0:
                t = m[r][c] / m[c][c]
                m[r] = [a - t * b for a, b in zip(m[r], m[c])]
    return [m[i][n] / m[i][i] for i in range(n)]


def decompose(n, d, factors):
    """rule 4's text of n÷d over factors [(f, k)] whose product times a constant is d (the gcd already out)"""
    poly, r = pdivmod(n, d) if n else ([], [])
    order = sorted(factors, key=lambda fk: order_key(fk[0]))
    s = rpoly(poly) if poly else ""
    for f, k in order:
        P = ppow(f, k)
        Q, rem = pdivmod(d, P)
        assert not rem
        deg = len(P) - 1
        cols = []
        for i in range(deg):
            col = pdivmod(pmul([F(0)] * i + [F(1)], Q), P)[1]
            cols.append(col + [F(0)] * (deg - len(col)))
        rhs = pdivmod(r, P)[1] if r else []
        A = trim(solve(cols, rhs + [F(0)] * (deg - len(rhs))))
        digits = []
        for _ in range(k):                      # A's digits in base f: the numerator over f^k first
            A, c = pdivmod(A, f) if A else ([], [])
            digits.append(c)
        for j in range(1, k + 1):
            c = digits[k - j]
            if c:
                sg, t = term(c, f, j)
                s += (sg if (s or sg == "-") else "") + t
    return s or "0"


# ---- reading casim's answer ----

def split_terms(e):
    out, depth, start = [], 0, 0
    for i, ch in enumerate(e):
        depth += (ch == "(") - (ch == ")")
        if depth == 0 and ch in "+-" and i > 0:
            out.append(e[start:i])
            start = i
    out.append(e[start:])
    return out


def printed_factors(e):
    """[(f ascending integer coefficients, highest power)] of the fraction terms' denominators"""
    found = {}
    for t in split_terms(e):
        body = t.lstrip("+-")
        parts = ssv.top_split(body, "÷")
        if len(parts) != 2 or "X" not in parts[1]:
            continue
        den = parts[1]
        m = re.fullmatch(r"\((\d+)×(\(.*\)(?:\^\d+)?|X(?:\^\d+)?)\)", den)
        if m:
            den = m.group(2)
        m = re.fullmatch(r"\((.*)\)\^(\d+)", den) or re.fullmatch(r"\((.*)\)()", den) or \
            re.fullmatch(r"(X)\^(\d+)", den) or re.fullmatch(r"(X)()", den)
        if not m:
            return None
        f = tuple(int(c) for c in ssv.coeffs(m.group(1)))
        found[f] = max(found.get(f, 0), int(m.group(2) or 1))
    return [([F(c) for c in f], k) for f, k in found.items()]


def run(*args):
    p = subprocess.run([CLI, *args], stdout=subprocess.PIPE, text=True, encoding="utf-8")
    return p.stdout.strip()


def judge(text, n, d, model="35"):
    """the reasons casim's answer for text (n÷d, exact) is wrong; [] when right, None when refused by a bound"""
    got = run("--model", model, "partfrac", text, "X", "RAD")
    if got == "NO_ROOM":
        # int64 ran out (rule 3, rule 7): a refusal the unit allows, but not where the answer fits 191 bytes
        g = pgcd(n, d) if n else [c / d[-1] for c in d]
        nr, dr = (pdivmod(n, g)[0] if n else []), pdivmod(d, g)[0]
        fs = factors_by_judge(dr)
        if fs is None:
            return None
        want = decompose(nr, dr, fs)
        return [f"NO_ROOM where the answer fits ({len(want.encode())} bytes): {want}"] if len(want.encode()) <= 191 else None
    return judge_answer(text, n, d, got, model)


def judge_answer(text, n, d, got, model="35"):
    why = []
    partial = got.startswith("PARTIAL ")
    ans = got[8:] if partial else got
    g = pgcd(n, d) if n else [c / d[-1] for c in d]
    nr, dr = (pdivmod(n, g)[0] if n else []), pdivmod(d, g)[0]
    rows = []
    if len(g) > 1:                              # CANCEL's rows for the gcd alone: cancel "(g)÷(g)" is "1; rows"
        gi = [c * math.lcm(*[x.denominator for x in g]) for c in g]
        gt = ipoly([int(c) for c in gi])
        rows = run("cancel", f"({gt})÷({gt})", "X", "RAD").split("; ")[1:]
    if got == "TOO_LONG":
        fs = None
    else:
        expr = ans.split("; ")[0]
        fs = printed_factors(expr)
        if fs is None:
            return [f"a denominator not in rule 4's form: {ans}"]
    if fs is not None:
        prod = [F(1)]
        for f, k in fs:
            if [int(c) for c in f] != list(f) or f[-1] <= 0 or math.gcd(*[int(c) for c in f]) != 1:
                why.append(f"factor {ipoly(f)} not primitive with a positive lead")
            if len(f) > 2 and has_rational_root(f):
                why.append(f"factor {ipoly(f)} has a rational root (not over Q's irreducibles)")
            prod = pmul(prod, ppow(f, k))
        if len(dr) > 1:
            ratio = pdivmod(dr, prod)
            if ratio[1] or len(ratio[0]) != 1:
                why.append(f"the factors printed ({ans}) are not the reduced denominator's")
        elif fs:
            why.append("fractions where the denominator cancels to a constant")
    if why:
        return why
    if fs is None:                              # TOO_LONG: the judge's own rendering must pass 191 bytes
        fs = factors_by_judge(dr)
        if fs is None:
            return []                           # the judge cannot factor it alone: TOO_LONG stands unjudged
    want = decompose(nr, dr, fs) if len(dr) > 1 else (rpoly([c / dr[0] for c in nr]) or "0")
    want = "; ".join([want] + rows)
    if partial:
        want = "PARTIAL " + want
    if len(want.removeprefix("PARTIAL ").encode()) > 191:
        want = "TOO_LONG"
    if got != want:
        why.append(f"got  {got}\n     want {want}")
    elif got != "TOO_LONG":
        expr = ans.split("; ")[0]
        for x in POINTS:
            try:
                tv = ssv.ev(text, x) if model != "1" else None
            except ZeroDivisionError:
                continue
            if tv is not None and ssv.ev(expr, x) != tv:
                why.append(f"{expr} differs from {text} at X={x}")
                break
    return why


def factors_by_judge(d):
    """d's factors over Q when they are the planted shapes (linear, QUADS-like quadratics, CUBICS), else None"""
    d = trim(d)
    out, rest = [], d
    for a in range(1, 13):
        for b in range(-30, 31):
            if math.gcd(a, b) != 1:
                continue
            f, k = [F(b), F(a)], 0
            while len(rest) > 1:
                q, r = pdivmod(rest, f)
                if r:
                    break
                rest, k = q, k + 1
            if k:
                out.append((f, k))
    for a in range(1, 4):
        for b in range(-6, 7):
            for c in range(-6, 7):
                f, k = [F(c), F(b), F(a)], 0
                if math.gcd(math.gcd(a, b), c) != 1 or b * b - 4 * a * c >= 0 and math.isqrt(b * b - 4 * a * c) ** 2 == b * b - 4 * a * c:
                    continue
                while len(rest) > 2:
                    q, r = pdivmod(rest, f)
                    if r:
                        break
                    rest, k = q, k + 1
                if k:
                    out.append((f, k))
    for c in CUBICS:
        f, k = [F(x) for x in c], 0
        while len(rest) > 3:
            q, r = pdivmod(rest, f)
            if r:
                break
            rest, k = q, k + 1
        if k:
            out.append((f, k))
    return out if len(rest) == 1 else None


# ---- inputs ----

QUADS = [[1, 0, 1], [2, 0, 1], [1, 1, 1], [-2, 0, 1], [3, -1, 2], [2, -1, 1], [5, 2, 3]]
CUBICS = [[-2, 0, 0, 1], [1, 1, 0, 1], [1, 1, 0, 2], [-3, 0, 1, 1]]       # irreducible over Q: no rational root


def seeded(rnd):
    """planted n÷d: linear, quadratic and cubic factors to the 4th power, a common factor (linear, quadratic, squared),
    a zero numerator, a constant denominator"""
    if rnd.random() < 0.03:                     # a constant denominator: the polynomial divided
        n = trim([F(rnd.randint(-9, 9)) for _ in range(rnd.randint(1, 6))]) or [F(1)]
        k = rnd.randint(2, 9)
        return f"({ipoly_any(n)})÷{k}", n, [F(k)]
    while True:
        fs, deg = [], 0
        for _ in range(rnd.randint(1, 3)):
            r = rnd.random()
            if r < 0.6:
                a, b = rnd.randint(1, 4), rnd.randint(-6, 6)
                if math.gcd(a, b) != 1:
                    continue
                f = [F(b), F(a)]
            elif r < 0.9:
                f = [F(c) for c in rnd.choice(QUADS)]
            else:
                f = [F(c) for c in rnd.choice(CUBICS)]
            if any(f == g for g, _ in fs):
                continue
            k = rnd.choice([1, 1, 1, 2, 2, 3, 4])
            fs.append((f, k))
            deg += (len(f) - 1) * k
        if fs and deg <= 10:
            break
    n = trim([F(rnd.randint(-9, 9)) for _ in range(rnd.randint(1, deg + 2))]) or [F(1)]
    if rnd.random() < 0.03:
        n = []                                  # 0÷d: all of d cancels
    common = []
    if rnd.random() < 0.25:
        common = rnd.choice([[F(rnd.randint(-5, 5)), F(1)], [F(-3), F(0), F(1)], [F(1), F(1), F(1)],
                             ppow([F(-3), F(0), F(1)], 2), ppow([F(1), F(1)], 2)])
    d = [F(1)]
    for f, k in fs:
        d = pmul(d, ppow(f, k))
    lc = F(rnd.choice([1, 1, 1, 2, 3, -1]))
    d = [c * lc for c in d]
    nt, dt = (ipoly_any(n) if n else "0"), ipoly_any(d)
    if common:
        ct = ipoly_any(common)
        text = f"({ct})×({nt})÷(({ct})×({dt}))"
        n, d = pmul(common, n), pmul(common, d)
        if len(d) > 13:
            return seeded(rnd)
    else:
        text = f"({nt})÷({dt})"
    return text, n, d


def ipoly_any(a):
    return ipoly([int(c) for c in a]) if all(c.denominator == 1 for c in a) else rpoly(a)


def examples():
    for line in open(os.path.join(ROOT, "work", "012-examples.txt"), encoding="utf-8"):
        if line.startswith("#"):
            continue
        f = [x.strip() for x in line.split(" | ")]
        text, model, want = f[2], f[4], f[5]
        if model == "STU" or "=" in text or re.search(r"\^\d+\^", text) or want in ("NOT_NUMERIC", "NOT_POLYNOMIAL", "UNDEFINED", "BAD_ARG", "NO_ROOM"):
            continue
        sides = ssv.top_split(text, "÷") if len(split_terms(text)) == 1 else [text]
        if len(sides) == 1:
            num, den = sides[0], "1"
        elif len(sides) == 2:
            num, den = sides
        else:
            continue
        yield text, [F(c) for c in ssv.coeffs(num)], [F(c) for c in ssv.coeffs(den)]


def planted():
    """each fault, a well-formed answer with exactly one thing wrong, must be refused, and the right answer accepted"""
    text = "(X^3+1)÷((X-1)^2×(X^2+1))"
    n, d = [F(1), F(0), F(0), F(1)], pmul(ppow([F(-1), F(1)], 2), [F(1), F(0), F(1)])
    right = "1÷(2×(X-1))+1÷(X-1)^2+(X+1)÷(2×(X^2+1))"
    faults = {
        "casim's answer moved": None,
        "a sign": "1÷(2×(X-1))-1÷(X-1)^2+(X+1)÷(2×(X^2+1))",
        "the factor order": "(X+1)÷(2×(X^2+1))+1÷(2×(X-1))+1÷(X-1)^2",
        "a power order": "1÷(X-1)^2+1÷(2×(X-1))+(X+1)÷(2×(X^2+1))",
        "a constant unreduced": "2÷(4×(X-1))+1÷(X-1)^2+(X+1)÷(2×(X^2+1))",
        "a numerator not primitive": "1÷(2×(X-1))+1÷(X-1)^2+(2×X+2)÷(4×(X^2+1))",
        "a numerator's value": "1÷(2×(X-1))+2÷(X-1)^2+(X+1)÷(2×(X^2+1))",
        "two powers left combined (the same value)": "(X+1)÷(2×(X-1)^2)+(X+1)÷(2×(X^2+1))",
    }
    caught, bad = 0, []
    if run("partfrac", text, "X", "RAD") != right:
        bad.append(f"planted: casim's answer for {text} is no longer {right}")
    ok = judge_answer(text, n, d, right)
    if ok:
        bad.append("the right answer was refused: " + "; ".join(ok))
    t2 = "(X^2-1)÷((X-1)×(X+2))"
    n2, d2 = [F(-1), F(0), F(1)], [F(-2), F(1), F(1)]
    cases = [(text, n, d, f) for k, f in faults.items() if f] + \
            [(t2, n2, d2, "1-1÷(X+2)"), (t2, n2, d2, "-1÷(X+2); X=1")]       # a row dropped; the polynomial part dropped
    for t, nn, dd, fault in cases:
        if judge_answer(t, nn, dd, fault):
            caught += 1
        else:
            bad.append(f"planted fault accepted: {fault}")
    return caught, bad


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    rnd = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 20261012)
    caught, bad = planted()
    fails, judged, too_long, refused = [], 0, 0, 0
    cases = list(examples()) + [seeded(rnd) for _ in range(count)]
    for text, n, d in cases:
        why = judge(text, n, d)
        if why is None:
            refused += 1
            continue
        judged += 1
        too_long += run("partfrac", text, "X", "RAD") == "TOO_LONG"
        if why:
            fails.append(f"{text}\n     " + "\n     ".join(why))
    for f in fails[:10]:
        print("FAIL partfrac " + f)
    for b in bad:
        print("FAIL scripts/partfrac-values.py: " + b)
    if fails or bad or judged < count // 2:
        sys.exit(1)
    print(f"partfrac-values: {judged} answers rendered again by the judge and equal byte for byte ({too_long} TOO_LONG), "
          f"{refused} NO_ROOM each past 191 bytes anyway; the judge refused its {caught} planted faults")


if __name__ == "__main__":
    main()
