#!/usr/bin/env python3
"""CAS 018's sums judged by a different method from casim's (make check; work/018-sums.md). casim finds an
antidifference and telescoping classes; this judge adds the summand itself, term by term: the summand's text is read
by scripts/limit-values.py's Decimal reader (60 digits) at each k, and casim's answer in N is read at N = lo-1, lo, ...,
lo+25 (lo-1 is the empty sum) and must equal the running sum there to 45 digits. A sum to ∞ is judged against a sum
run until its terms are below 10^-65 (the geometric families), or against the construction itself (a telescoping
family is built as g(k) - g(k+d), whose sum from lo to ∞ is g(lo) + ... + g(lo+d-1), since g goes to 0). The seeded
families: polynomials to degree 12; q(k) × B^k for rational B and EXP(±1); telescoping sums written as partial
fractions and multiplied out; sums that can't close (NO_CLOSED_FORM); integer bounds at most 64 apart (rule 7, in each
angle mode); poles in the range (UNDEFINED); divergent sums (∞, -∞, UNDEFINED). Every work/018-examples.txt row with a
symbolic or whole upper bound is judged the same way. The judge refuses its planted answers first.
    scripts/sum-values.py [N] [SEED]     (default 300 cases, seed 20261015)"""
import decimal
import importlib.util
import os
import random
import re
import subprocess
import sys
import threading
from decimal import Decimal as D
from fractions import Fraction as F

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "build", "casim")
ARENA = "65536"
INF, NINF = "∞", "-∞"
STATUS = ("UNDEFINED", "UNDECIDED", "NO_CLOSED_FORM", "NO_ROOM", "TOO_LONG", "BAD_ARG")

_spec = importlib.util.spec_from_file_location("lv", os.path.join(ROOT, "scripts", "limit-values.py"))
lv = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(lv)
decimal.getcontext().prec = 60
TOL = D(10) ** -45


def sub(text, letter, by):
    """letter written as (by), not inside a name (the N of SIN, the K of nothing yet)"""
    return re.sub(rf"(?<![A-Z]){letter}(?![A-Z])", lambda m: f"({by})", text)


def dec(f):
    return D(f.numerator) / D(f.denominator)


def ft(c):
    """a Fraction as casim reads it inside a product: 3, (1÷2), (-3)"""
    if c.denominator == 1:
        return str(c.numerator) if c >= 0 else f"({c.numerator})"
    return f"({c.numerator}÷{c.denominator})"


def dvalue(text, fns, strict=False):
    """limit-values.py's reader, with 0^0 = 1 as casim and the calculator have it (Decimal raises there): numbers, π,
    the functions in fns, + - × ÷ ^. strict (CAS 035): a divisor, or a base under a negative power, that is 0 to 40
    digits is 0 (the 90-digit π gives COS(π÷2) 2.3E-59, and a line's huge terms there cancelled to a "value")"""
    toks = lv.TOK.findall(text)
    if "".join(toks) != text:
        raise ValueError(f"not readable: {text}")
    i = 0

    def peek():
        return toks[i] if i < len(toks) else None

    def take():
        nonlocal i
        i += 1
        return toks[i - 1]

    def atom():
        t = take()
        if t == "(":
            v = expr()
            take()
            return v
        if t == "π":
            return lv.PI
        if t == "-":
            return -factor()
        if t in fns:
            take()
            v = expr()
            take()
            return fns[t](v)
        if t[0].isdigit():
            return D(t)
        raise ValueError(f"not readable: {text} at {t}")

    def factor():
        v = atom()
        if peek() == "^":
            take()
            e = factor()
            if v == 0 and e == 0:
                return D(1)
            if v == 0 and e < 0:                # Decimal gives Infinity here; it has no value, as 1÷0 has none
                raise ZeroDivisionError("0 to a negative power")
            if strict and e < 0 and abs(v) < ZERO40:
                raise ZeroDivisionError("a base 0 to 40 digits under a negative power")
            v = v ** int(e) if e == int(e) else (e * v.ln()).exp()
        return v

    def term():
        v = factor()
        while peek() in ("×", "÷"):
            if take() == "×":
                v = v * factor()
            else:
                d = factor()
                if strict and abs(d) < ZERO40:
                    raise ZeroDivisionError("a divisor 0 to 40 digits")
                v = v / d
        return v

    def expr():
        v = term() if peek() != "-" else (take(), -term())[1]
        while peek() in ("+", "-"):
            v = v + term() if take() == "+" else v - term()
        return v

    return expr()


def d_asin(x):
    if abs(x) > 1:
        raise decimal.InvalidOperation(f"ASIN({x})")
    return lv.PI / 2 * (1 if x > 0 else -1) if abs(x) == 1 else lv.d_atan(x / (1 - x * x).sqrt())


def d_ln(x):
    """LN where Decimal's gives -Infinity at 0: no value"""
    if x <= 0:
        raise decimal.InvalidOperation(f"LN({x})")
    return x.ln()


ZERO40 = D(10) ** -40
CONTEXT = decimal.getcontext().copy()         # the 60 digits set above: Decimal's context is per thread (CAS 035)


def value(text, angle="RAD", strict=False):
    """text with no letter in Decimal, SIN, COS and TAN (and ASIN's, ACOS's and ATAN's answers) in the angle mode.
    strict (CAS 034's judge): TAN has no value where its cosine is 0 to 40 digits, as at π÷2, which the 90-digit π
    otherwise gives a value near 1E90; and (CAS 035's) a divisor 0 to 40 digits is 0, and LN(0) has no value (Decimal's
    LN gives -Infinity there). Valued in the module's own context, in any thread"""
    with decimal.localcontext(CONTEXT):
        return value_(text, angle, strict)


def value_(text, angle, strict):
    to_rad = {"RAD": D(1), "DEG": lv.PI / 180, "GRAD": lv.PI / 200}[angle]
    fns = dict(lv.FN)
    fns["SIN"] = lambda x: lv.d_sin(x * to_rad)
    fns["COS"] = lambda x: lv.d_cos(x * to_rad)
    fns["ABS"] = abs                                            # CAS 026's factors
    fns["COSH"] = lambda x: (x.exp() + (-x).exp()) / 2
    fns["TAN"] = lambda x: lv.d_sin(x * to_rad) / lv.d_cos(x * to_rad)      # CAS 030's
    fns["ATAN"] = lambda x: lv.d_atan(x) / to_rad
    fns["ASIN"] = lambda x: d_asin(x) / to_rad
    fns["ACOS"] = lambda x: (lv.PI / 2 - d_asin(x)) / to_rad
    fns["LOG"] = lambda x: d_ln(x) / D(10).ln()
    fns["SINH"] = lambda x: (x.exp() - (-x).exp()) / 2
    fns["TANH"] = lambda x: (x.exp() - (-x).exp()) / (x.exp() + (-x).exp())
    fns["ASINH"] = lambda x: d_ln(x + (x * x + 1).sqrt())
    fns["ACOSH"] = lambda x: d_ln(x + (x * x - 1).sqrt())       # Decimal's sqrt of a negative: no value
    fns["ATANH"] = lambda x: d_ln((1 + x) / (1 - x)) / 2
    fns["ALOG"] = lambda x: D(10) ** x                         # CAS 034's
    if strict:
        def tan(x):
            c = lv.d_cos(x * to_rad)
            if abs(c) < D(10) ** -40:
                raise ZeroDivisionError(f"TAN({x})")
            return lv.d_sin(x * to_rad) / c
        fns["TAN"] = tan
        fns["LN"] = d_ln                                        # CAS 035: LN(0) has no value
    return dvalue(text, fns, strict)


def term(summand, k, angle):
    """the summand at k, or None where it has no value"""
    try:
        return value(sub(summand, "K", k), angle)
    except (ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero):
        return None


def close(got, want):
    return abs(got - want) <= TOL * max(D(1), abs(want))


def judge_n(summand, lo, out, angle="RAD", points=26, first=None):
    """'' when out, an answer in N, is the running sum from lo at N = lo-1 (or first) .. lo+points-2"""
    if out in STATUS or out in (INF, NINF) or not out:
        return f"{out} for a closed form"
    run = D(0)
    for n in range(lo - 1, lo - 1 + points):
        if n >= lo:
            t = term(summand, n, angle)
            if t is None:
                return f"the truth has a pole at {n}"
            run += t
        if first is not None and n < first:
            continue
        try:
            got = value(sub(out, "N", n), angle)
        except (ValueError, ZeroDivisionError, decimal.InvalidOperation) as e:
            return f"{out} at N={n}: {e}"
        if not close(got, run):
            return f"{out} at N={n} is {got:.25} for {run:.25}"
    return ""


def judge_value(out, want, angle="RAD"):
    """'' when out is want (a Decimal, or a status or ∞ text)"""
    if isinstance(want, str):
        return "" if out == want else f"{out} for {want}"
    if out in STATUS or out in (INF, NINF) or not out:
        return f"{out} for {want:.25}"
    try:
        got = value(out, angle)
    except (ValueError, ZeroDivisionError, decimal.InvalidOperation) as e:
        return f"{out}: {e}"
    return "" if close(got, want) else f"{out} = {got:.25} for {want:.25}"


def run_sum(summand, lo, hi, angle="RAD", arena=ARENA):
    p = subprocess.run([CLI, "--arena", arena, "sum", summand, "K", str(lo), str(hi), angle], stdout=subprocess.PIPE,
                       text=True, encoding="utf-8")
    return p.stdout.strip()


def tail_sum(summand, lo, angle="RAD"):
    """Σ from lo to ∞ of a geometrically shrinking summand, until 8 terms in a row are below 10^-65 (one alone may be 0:
    (1÷3)^K+(-1÷3)^K at an odd K)"""
    s, k, small = D(0), lo, 0
    while True:
        t = term(summand, k, angle)
        s += t
        small = small + 1 if abs(t) < D(10) ** -65 else 0
        if k > lo + 20 and small >= 8:
            return s
        k += 1


# ---- the families: (summand, lo, hi, angle, truth) with truth "N" (judge_n), a Decimal, or a status ----

def poly_text(co):
    parts = [f"{ft(c)}×K^{j}" if j > 1 else f"{ft(c)}×K" if j == 1 else ft(c) for j, c in enumerate(co) if c]
    return "+".join(parts) if parts else "0"


def rand_frac(rnd, small=False):
    c = F(rnd.randint(-5, 5), rnd.choice((1, 1, 1, 2, 3)))
    return c if c or small else F(1)


def gen_poly(rnd):
    d = rnd.choice((0, 1, 1, 2, 2, 3, 3, 4, 5, 6, 8, 12))
    co = [rand_frac(rnd, True) for _ in range(d)] + [rand_frac(rnd)]
    lo = rnd.randint(-2, 3)
    if rnd.random() < 0.15:
        lead = next(c for c in reversed(co) if c)
        return poly_text(co), lo, INF, "RAD", INF if lead > 0 else NINF
    if rnd.random() < 0.1:
        hi = lo + rnd.randint(70, 150)                  # past rule 7: a closed form at whole bounds
        return poly_text(co), lo, hi, "RAD", sum((term(poly_text(co), k, "RAD") for k in range(lo, hi + 1)), D(0))
    return poly_text(co), lo, "N", "RAD", "N"


GEOM = [F(2), F(3), F(1, 2), F(1, 3), F(-1, 2), F(-1, 3), F(3, 2), F(-2), F(-1), F(2, 3)]


def gen_geom(rnd):
    d = rnd.choice((0, 0, 1, 1, 2, 3, 4))
    co = [rand_frac(rnd, True) for _ in range(d)] + [rand_frac(rnd)]
    lo = rnd.randint(0, 2)
    if rnd.random() < 0.2:
        b, ratio = rnd.choice(("EXP(K)", "EXP(-K)")), None
    else:
        ratio = rnd.choice(GEOM)
        b = f"{ft(ratio)}^K" if rnd.random() < 0.7 else f"{ft(ratio)}^(K+1)"
    q = poly_text(co)
    summand = f"({q})×{b}" if d or co[0] != 1 else b
    if rnd.random() < 0.4:
        small = (ratio is not None and abs(ratio) < 1) or b == "EXP(-K)"
        if small:
            return summand, lo, INF, "RAD", tail_sum(summand, lo)
        lead = co[-1] * (ratio if ratio is not None and b.endswith("(K+1)") else 1)
        if ratio is not None and ratio < 0:
            return summand, lo, INF, "RAD", "UNDEFINED"
        return summand, lo, INF, "RAD", INF if lead > 0 else NINF
    return summand, lo, "N", "RAD", "N"


def gen_geom2(rnd):
    """two groups to ∞: the larger |B| decides (a polynomial is |B| = 1), then its sign; both small converge"""
    ratios = rnd.sample([F(1), F(2), F(3), F(1, 2), F(-1, 3), F(3, 2), F(-2), F(1, 3)], 2)
    parts, lead = [], {}
    for r in ratios:
        c, j = F(rnd.choice((1, 2, 3, -1, -2))), rnd.randint(0, 2)
        k = f"×K^{j}" if j > 1 else "×K" if j == 1 else ""
        parts.append(f"{ft(c)}{k}" + (f"×{ft(r)}^K" if r != 1 else ""))
        lead[r] = (j, c)
    summand, lo = "+".join(parts), rnd.randint(0, 1)
    big = [r for r in ratios if abs(r) >= 1]
    if not big:
        return summand, lo, INF, "RAD", tail_sum(summand, lo)
    top = max(big, key=lambda r: (abs(r), lead[r][0]))
    if any(r != top and abs(r) == abs(top) and lead[r][0] == lead[top][0] for r in big):
        return summand, lo, INF, "RAD", "UNDECIDED"             # B and -B at one degree: a tie
    if top < 0:
        return summand, lo, INF, "RAD", "UNDEFINED"
    return summand, lo, INF, "RAD", INF if lead[top][1] > 0 else NINF


def frac_text(a, s, m, rnd):
    """a÷(K+s)^m, written one of several ways"""
    if s.denominator == 2 and rnd.random() < 0.5:                  # a÷(K+s)^m = a×2^m÷(2×K+2s)^m
        den = f"(2×K+{2 * s})" if s >= 0 else f"(2×K-{-2 * s})"
        a = a * 2 ** m
    else:
        den = f"(K+{ft(s)})" if s else "K"
    return f"{ft(a)}÷{den}" + (f"^{m}" if m > 1 else "")


def gen_tele(rnd):
    """f(k) = g(k) - g(k+d), g one or two fractions of one power"""
    m = rnd.choice((1, 1, 1, 2))
    shifts = rnd.sample([F(0), F(1), F(2), F(1, 2), F(-1, 2), F(3)], rnd.choice((1, 1, 2)))
    g = [(F(rnd.choice((1, 2, 3, -1, -2))), s) for s in shifts]
    d = rnd.choice((1, 1, 2, 3))
    while len({x for _, s in g for x in (s, s + d)}) < 2 * len(g):      # no shift twice: the typed text would cancel
        d += 1
    lo = max(int(-s) for _, s in g) + rnd.randint(1, 2)          # every pole below lo
    parts = []
    for a, s in g:
        parts += [frac_text(a, s, m, rnd), frac_text(-a, s + d, m, rnd)]
    if m == 1 and len(g) == 1 and rnd.random() < 0.5:              # multiplied out: a×d÷((K+s)(K+s+d))
        (a, s), = g
        parts = [f"{ft(a * d)}÷((K+{ft(s)})×(K+{ft(s + d)}))"]
    summand = "+".join(parts).replace("+(-", "+(-")
    if rnd.random() < 0.35:
        truth = sum((a / (k + s) ** m for a, s in g for k in range(lo, lo + d)), F(0))
        return summand, lo, INF, "RAD", D(truth.numerator) / D(truth.denominator)
    return summand, lo, "N", "RAD", "N"


def gen_tele_typed(rnd):
    """a telescoping sum typed as one n÷d, multiplied out (abacus #8460: canon splits it into one term per power of K
    in the numerator, over one divisor); integer shifts, two to four fractions of power 1 or 2"""
    m = rnd.choice((1, 2, 2))
    shifts = rnd.sample([0, 1, 2, 3], rnd.choice((1, 2)))
    g = [(F(rnd.choice((1, 2, 3, -1, -2, 4))), s) for s in shifts]
    d = rnd.choice((1, 1, 2))
    while len({x for _, s in g for x in (s, s + d)}) < 2 * len(g):
        d += 1
    fr = [(a, s) for a, s in g] + [(-a, s + d) for a, s in g]            # f(k) = Σ a÷(k+s)^m
    lo = max(-s for _, s in fr) + rnd.randint(1, 2)
    def pmul(p, q):
        r = [F(0)] * (len(p) + len(q) - 1)
        for i, x in enumerate(p):
            for j, y in enumerate(q):
                r[i + j] += x * y
        return r
    def ppow(s):                                                    # (k+s)^m
        r = [F(1)]
        for _ in range(m):
            r = pmul(r, [F(s), F(1)])
        return r
    den = [F(1)]
    for _, s in fr:
        den = pmul(den, ppow(s))
    num = [F(0)] * len(den)
    for i, (a, s) in enumerate(fr):
        t = [a]
        for j, (_, s2) in enumerate(fr):
            if j != i:
                t = pmul(t, ppow(s2))
        for k, x in enumerate(t):
            num[k] += x
    def text(co):
        terms = [(j, c) for j, c in enumerate(co) if c]
        rnd.shuffle(terms)                                          # the order typed is canon's to fix
        return "+".join(f"{ft(c)}×K^{j}" if j > 1 else f"{ft(c)}×K" if j == 1 else ft(c) for j, c in terms) or "0"
    summand = f"({text(num)})÷({text(den)})".replace("+-", "-")
    if rnd.random() < 0.35:
        truth = sum((a / (k + s) ** m for a, s in g for k in range(lo, lo + d)), F(0))
        return summand, lo, INF, "RAD", D(truth.numerator) / D(truth.denominator)
    return summand, lo, "N", "RAD", "N"


def gen_open(rnd):
    """sums that don't close: a class that doesn't telescope, a function of K (symbolic bounds)"""
    summand = rnd.choice(("1÷K", "1÷K^2", "1÷(K×(K+2))+1÷K", "3÷(K+1)^2-1÷K", "SIN(K)", "LN(K)", "SQRT(K)",
                          "K!", "2^K÷K", "1÷(K^2+1)"))
    return summand, rnd.randint(1, 3), rnd.choice(("N", INF)), "RAD", "NO_CLOSED_FORM"


def gen_direct(rnd):
    """rule 7: whole bounds at most 64 apart, term by term, in each angle mode"""
    angle = rnd.choice(("RAD", "DEG", "GRAD"))
    summand = rnd.choice(("1÷K", "SIN(K)", "COS(K)", "K^2÷(K+1)", "SQRT(K)", "1÷K^2", "2^K÷K", "K×SIN(K)"))
    lo = rnd.randint(1, 4)
    hi = lo + rnd.randint(-1, 63)
    if hi < lo:
        return summand, lo, hi, angle, D(0)
    return summand, lo, hi, angle, sum((term(summand, k, angle) for k in range(lo, hi + 1)), D(0))


def gen_pole(rnd):
    """a pole in the range: UNDEFINED, at symbolic, ∞ and whole bounds"""
    s = rnd.randint(-4, 0)
    # the third: (K+s)÷((K+s)(K+s+1)(K+s+2)) with the divisor multiplied out, so canon keeps it and partfrac
    # cancels K+s, a hole at -s (rule 5's removed values)
    cubic = f"(K+{s})÷(K^3+{3 * s + 3}×K^2+{3 * s * s + 6 * s + 2}×K+{s * s * s + 3 * s * s + 2 * s})".replace("+-", "-")
    summand = rnd.choice((f"1÷(K+{s})" if s else "1÷K", f"1÷((K+{s})×(K+{s + 1}))" if s else "1÷(K×(K+1))", cubic))
    lo = rnd.randint(-s - 3, -s)
    return summand, lo, rnd.choice(("N", INF, lo + 5, lo + 80)), "RAD", "UNDEFINED"


DOMAIN036 = {"LN": "{}>0", "LOG": "{}>0", "ACOSH": "{}≥1", "ASIN": "ABS({})≤1", "ACOS": "ABS({})≤1", "TAN": "COS({})≠0"}


def typed_row(summand, r):
    """CAS 035: a row for the typed summand's root or fractional power of a letter (Z≥0 for SQRT(Z)^(2×K-2), which canon
    writes Z^(K-1)), which judge_typed judges; the older judges read the rows beside it. CAS 036: to N, the domain row
    of the summand's LN, LOG, ACOSH, ASIN, ACOS or TAN of a letter too, which its answer shows the hole of"""
    m = re.fullmatch(r"([A-Z])(≥|≤|>)(.*)", r)
    if m and (f"SQRT({m.group(1)})" in summand or re.search(rf"{m.group(1)}\^\(-?\d+÷\d+\)", summand) is not None):
        return True
    return r in {DOMAIN036[f].format(L) for f, L in re.findall(r"(?<![A-Z])(LN|LOG|ACOSH|ASIN|ACOS|TAN)\(([A-Z])\)", summand)}


def gen_letter(rnd):
    """rule 4b: q(K) × (c×X^m)^K, answered with its rows: judged at seeded X (off the excluded values; at ∞, inside the
    region), and the rows against the true excluded set: the real roots of c×X^m = 1, or |X| < or > |c|^(-1/m)"""
    c = rnd.choice((F(1), F(1), F(-1), F(2), F(1, 2), F(-1, 3)))
    m = rnd.choice((1, 1, 2, -1, -2, 3))
    co = [rand_frac(rnd, True) for _ in range(rnd.choice((0, 0, 1, 2)))] + [rand_frac(rnd)]   # to degree 2: the shift's
                                                                                                  # binomials (#8349)
    xm = "X" if m == 1 else f"X^{m}" if m > 0 else f"X^({m})"
    b = f"({xm})" if c == 1 else f"({ft(c)}×{xm})"         # the judge's reader takes ^ right to left
    q = poly_text(co)
    e = rnd.choice(("K", "K", "K-1", "K+1", "K-2"))          # a constant in the exponent: (c×X^m)^(K-1) at X = 0,
                                                             # and K-2 for rule 4b's shift by 2 (abacus #8349)
    pw = f"{b}^K" if e == "K" else f"{b}^({e})"
    summand = f"({q})×{pw}" if len(co) > 1 or co[0] != 1 else pw
    lo = rnd.randint(0, 2)
    return summand, lo, INF if rnd.random() < 0.35 else "N", "RAD", ("LETTER", c, m)


def judge_letter(summand, lo, hi, out, c, m, rnd):
    """'' when out ('answer; rows') is right at three seeded X and its rows are the truth's"""
    if out in STATUS or not out:
        return f"{out} for an answer with rows" if out != "TOO_LONG" else ""
    parts = out.split("; ")
    answer, rows = parts[0], parts[1:]
    # a row X≠0 is right only where the answer itself has no value at 0 (rule 4b's hole row)
    if "X≠0" in rows:
        try:
            value(sub(answer, "X", "0").replace("N", "3") if hi != INF else sub(answer, "X", "0"))
            return f"row X≠0, but {answer} has a value at X=0"
        except (ValueError, ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero):
            rows = [r for r in rows if r != "X≠0"]
    if hi == INF:
        want_op = "<" if m > 0 else ">"
        rv = dec(abs(c)) ** (D(-1) / D(m))                     # |X| op |c|^(-1/m)
        if len(rows) != 1 or not rows[0].startswith("ABS(X)" + want_op):
            return f"rows {rows} for ABS(X){want_op}{rv:.20}"
        got = value(rows[0][len("ABS(X)") + 1:])
        if not close(got, rv):
            return f"row {rows[0]}: {got:.25} for {rv:.25}"
        inside = [x for x in (F(0), F(1, 3), F(-1, 5), F(2, 7), F(3), F(-4), F(5, 2))
                  if (x != 0 or (m > 0 and "X≠0" not in parts)) and abs(c) * abs(x) ** m < F(3, 4)]
        for x in inside[:3]:
            why = judge_value(sub(answer, "X", ft(x)), tail_sum(sub(summand, "X", ft(x)), lo))
            if why:
                return f"at X={x}: {why}"
        return ""
    # finite: rows X≠v, the real roots of c×x^m = 1, ascending
    t = 1 / c
    roots = []
    if m % 2:
        roots = [(-1 if t < 0 else 1) * dec(abs(t)) ** (D(1) / D(m))]
    elif t > 0:
        u = dec(t) ** (D(1) / D(m))
        roots = [-abs(u), abs(u)]
    if len(rows) != len(roots) or any(not row.startswith("X≠") for row in rows):
        return f"rows {rows} for X≠ {[f'{v:.10}' for v in roots]}"
    for row, v in zip(rows, sorted(roots)):
        got = value(row[2:])
        if not close(got, v):
            return f"row {row}: {got:.25} for {v:.25}"
    for x in (F(0), F(1, 3), F(-2, 5), F(3), F(-7, 4), F(5, 2)):
        if any(abs(dec(x) - v) < D(10) ** -9 for v in roots) or (x == 0 and (m < 0 or "X≠0" in parts)):
            continue
        why = judge_n(sub(summand, "X", ft(x)), lo, sub(answer, "X", ft(x)), points=12)
        if why:
            return f"at X={x}: {why}"
    return ""



def gen_letter2(rnd):
    """CAS 024 (abacus #9560): q(K) × c^K × X^(a×K+e) × Y^(b×K+f), with W^(K+g) a quarter of the time: a ratio
    c×X^a×Y^b (×W) in two or three letters, lo from -2 to 3, to N and to ∞. Each letter is written once, at its merged
    power, so the text is canon's reading (abacus #9563)"""
    c = rnd.choice((F(1), F(1), F(-1), F(2), F(1, 2)))
    letters, ms = ["X", "Y"], [rnd.choice((1, 1, 2)), rnd.choice((1, 1, 2))]
    es = [rnd.choice((0, -1, -1, -2, -3, 1, 2)), rnd.choice((0, 0, -1, -2, 1))]
    if rnd.random() < 0.25:
        letters.append("W"), ms.append(1), es.append(rnd.choice((0, -1, -2)))
    co = [rand_frac(rnd, True) for _ in range(rnd.choice((0, 0, 1, 2)))] + [rand_frac(rnd)]
    lo = rnd.choice((-2, -1, 0, 1, 1, 2, 3))
    if rnd.random() < 0.2:          # q × (K - lo): a zero coefficient at lo cancels no power (abacus #9563, #9594)
        co = [(co[i - 1] if i else 0) - lo * (co[i] if i < len(co) else 0) for i in range(len(co) + 1)]

    def pw(L, m, e):
        ex = ("K" if m == 1 else f"{m}×K") + (f"+{e}" if e > 0 else f"{e}" if e < 0 else "")
        return f"{L}^K" if ex == "K" else f"{L}^({ex})"
    parts = ([f"({poly_text(co)})"] if len(co) > 1 or co[0] != 1 else []) + ([f"{ft(c)}^K"] if c != 1 else [])
    summand = "×".join(parts + [pw(L, m, e) for L, m, e in zip(letters, ms, es)])
    return summand, lo, INF if rnd.random() < 0.3 else "N", "RAD", \
        ("LETTER2", c, letters, ms)


def judge_letter2(summand, lo, hi, out, c, letters, ms):
    """'' when out ('answer; rows') is right: L≠0 stands exactly where a term from lo divides by L (canon's reading
    with K put in, nothing cancelled after: abacus #9560 (2), #9563); one row for the ratio, E≠v with E÷v the ratio, or
    ABS(E)<v to ∞; and the answer is the sum at letter values with each letter 0 in turn and at three without a 0, from
    N = lo-1 (to ∞ inside ABS(E) < 3÷4 of v)"""
    if out in STATUS or not out:
        return f"{out} for an answer with rows"
    parts = out.split("; ")
    answer, rows = parts[0], parts[1:]

    def at(text, pt):
        for L, v in pt.items():
            text = sub(text, L, ft(v))
        return text

    def pole(pt):
        return any(term(at(summand, pt), k, "RAD") is None for k in range(lo, lo + 13))

    def ratio(pt):
        b = c
        for L, m in zip(letters, ms):
            b *= pt[L] ** m
        return b
    off = {L: F(1, 3) for L in letters}
    for L in letters:
        want = pole({**off, L: F(0)})
        if want != (f"{L}≠0" in rows):
            return (f"{L}≠0 {'missing' if want else 'for nothing'}: a term from {lo} "
                    f"{'divides' if want else 'never divides'} by {L}")
    rest = [r for r in rows if r not in [f"{L}≠0" for L in letters]]
    rest = [r for r in rest if not typed_row(summand, r)]     # CAS 035's, judged by judge_typed
    m = re.match(r"^ABS\((.*)\)<(.*)$", rest[0]) if hi == INF and len(rest) == 1 else \
        re.match(r"^(.*)≠(.*)$", rest[0]) if len(rest) == 1 else None
    if not m:
        return f"rows {rest} for the ratio's one row"
    E, v = m.group(1), m.group(2)
    for pt in ({L: F(2, 3) for L in letters}, {L: F(-5, 4) + i for i, L in enumerate(letters)}):
        if not close(value(at(E, pt)) / value(v), dec(ratio(pt))):
            return f"row {rest[0]}: not the ratio {ft(c)}×{'×'.join(f'{L}^{m}' for L, m in zip(letters, ms))} at {pt}"
    grid = (F(1, 2), F(-1, 3), F(2), F(-3, 2))
    pts = [{**{M: grid[(i + j) % 4] for j, M in enumerate(letters)}, L: F(0)} for i, L in enumerate(letters)]
    pts += [{M: grid[(i + 2 * j) % 4] for j, M in enumerate(letters)} for i in range(3)]
    for pt in pts:
        if any(pt[L] == 0 and f"{L}≠0" in rows for L in letters) or pole(pt):
            continue
        e = value(at(E, pt)) / value(v)
        if (hi == INF and abs(e) >= D(3) / 4) or (hi != INF and close(e, D(1))):
            continue
        why = judge_value(at(answer, pt), tail_sum(at(summand, pt), lo)) if hi == INF else \
            judge_n(at(summand, pt), lo, at(answer, pt), points=12)
        if why:
            return f"at {', '.join(f'{L}={x}' for L, x in pt.items())}: {why}"
    return ""

C025 = ("SQRT(2)", "π", "EXP(1)", "SQRT(5)÷3", "LN(3)", "π÷4", "EXP(-1)", "-SQRT(3)", "3^(1÷4)", "2×π")


def gen_letter3(rnd):
    """CAS 025 (abacus #9687): CAS 024's ratios whose B holds a factor the shift doesn't read: an irrational constant c,
    a letter at a negative power (Y^(-K)), or SIN(Z) or SQRT(Z). q(K) × c^K × X^(a×K+e) × Y^(b×K+f) (× W^(K+g)), lo
    from -2 to 3, to N and to ∞, written as canon reads it (abacus #9563)"""
    kind = rnd.choice(("c", "c", "c", "neg", "neg", "fn"))
    c = rnd.choice(C025) if kind == "c" else ft(rnd.choice((F(1), F(1), F(-1), F(2), F(1, 2))))
    letters, ms = ["X", "Y"], [rnd.choice((1, 1, 2)), rnd.choice((1, 1, 2))]
    if kind == "neg" or (kind == "c" and rnd.random() < 0.2):
        ms[1] = -ms[1]
    es = [rnd.choice((0, -1, -1, -2, -3, 1, 2)), rnd.choice((0, 0, -1, -2, 1))]
    if rnd.random() < 0.25:
        letters.append("W"), ms.append(1), es.append(rnd.choice((0, -1, -2)))
    fn = rnd.choice(("SIN(Z)", "SQRT(Z)")) if kind == "fn" else None
    co = [rand_frac(rnd, True) for _ in range(rnd.choice((0, 0, 1, 2)))] + [rand_frac(rnd)]
    lo = rnd.choice((-2, -1, 0, 1, 1, 2, 3))
    if rnd.random() < 0.2:          # q × (K - lo): a zero coefficient at lo cancels no power (abacus #9563, #9594)
        co = [(co[i - 1] if i else 0) - lo * (co[i] if i < len(co) else 0) for i in range(len(co) + 1)]

    def pw(L, m, e):
        ex = ("K" if m == 1 else "-K" if m == -1 else f"{m}×K") + (f"+{e}" if e > 0 else f"{e}" if e < 0 else "")
        return f"{L}^K" if ex == "K" else f"{L}^({ex})"
    cp = c if c.startswith("(") else f"({c})"
    parts = ([f"({poly_text(co)})"] if len(co) > 1 or co[0] != 1 else []) + ([f"{cp}^K"] if c != "1" else []) + \
        ([f"{fn}^K"] if fn else [])
    summand = "×".join(parts + [pw(L, m, e) for L, m, e in zip(letters, ms, es)])
    B = "×".join([cp] + [f"{L}^({m})" for L, m in zip(letters, ms)] + ([fn] if fn else []))
    return summand, lo, INF if rnd.random() < 0.3 else "N", "RAD", \
        ("LETTER3", B, letters + (["Z"] if fn else []), dict(zip(letters, ms)), dict(zip(letters, es)))


def judge_letter3(summand, lo, hi, out, B, letters, ms, es):
    """('' when out ('answer; rows') is right, and what it names). CAS 025, abacus #9687: L≠0 stands wherever a term from
    lo divides by L (canon's reading, nothing cancelled: #9563), past any factor of B; a row with no value at L = 0
    (ABS(X÷Y)<1) excludes nothing. One row for the ratio, ABS(E)<v or E≠v with E÷v = B; the answer is the sum at each
    letter 0 in turn and at three points with none 0 (to ∞ inside ABS(B) < 3÷4). Named, each as at rc6 (e551d3e), and
    counted, not failed:
      - a row for nothing on a letter B holds at a positive power, where q divides by it or lo < 0: the shift that
        would clear it is refused past another factor (the follow-up list's class, p2's fix; to ∞ from lo < 0 it is the
        follow-up item for a rational ratio too; abacus #9699 (b));
      - to N from lo < 0, no value at a letter's 0 for N from lo-1 to -2 (B^(N+1) at a negative power), which CAS
        024's shift by lo gives one (#9572);
      - a letter inside a function with no row where a term from lo divides by its factor (K×(SIN(Z)×Y)^K÷SIN(Z) from
        0): a row on the factor, abacus #9698, #9699.
    DECIMAL_FORM (3^(1÷4)'s ratio, as at rc6) is the caller's soft count"""
    if out in STATUS or not out:
        return f"{out} for an answer with rows", []
    parts = out.split("; ")
    answer, rows = parts[0], parts[1:]

    def at(text, pt):
        for L, v in pt.items():
            text = sub(text, L, ft(v))
        return text

    def pole(pt):
        return any(term(at(summand, pt), k, "RAD") is None for k in range(lo, lo + 13))
    named = []
    off = {L: F(1, 3) for L in letters}
    for L in letters:
        want = pole({**off, L: F(0)})
        if want and f"{L}≠0" not in rows:
            if L not in ms:
                named.append(f"{L} in a function")
                continue
            return f"{L}≠0 missing: a term from {lo} divides by {L}", named
        if not want and f"{L}≠0" in rows:
            if ms.get(L, 0) > 0 and (es[L] < 0 or lo < 0):
                named.append(L)
            else:
                return f"{L}≠0 for nothing: no term from {lo} divides by {L}", named
    rest = [r for r in rows if r not in [f"{L}≠0" for L in letters]]
    rest = [r for r in rest if not typed_row(summand, r)]     # CAS 035's, judged by judge_typed
    m = re.match(r"^ABS\((.*)\)<(.*)$", rest[0]) if hi == INF and len(rest) == 1 else \
        re.match(r"^(.*)≠(.*)$", rest[0]) if len(rest) == 1 else None
    if not m:
        return f"rows {rest} for the ratio's one row", named
    E, v = m.group(1), m.group(2)
    for pt in ({L: F(2, 3) for L in letters}, {L: F(-5, 4) + i for i, L in enumerate(letters)}):
        if not close(value(at(E, pt)) / value(v), value(at(B, pt))):
            return f"row {rest[0]}: not the ratio {B} at {pt}", named
    # to ∞ a letter B holds at a positive power is small and one at a negative power large, so B is inside 3÷4
    small, large, grid = (F(1, 5), F(-1, 4), F(1, 3)), (F(4), F(-3), F(5, 2)), (F(1, 2), F(-1, 3), F(2), F(-3, 2))

    def pick(L, i):
        return (large if ms.get(L, 1) < 0 else small)[i % 3] if hi == INF else grid[i % 4]
    pts = [{**{M: pick(M, i + j) for j, M in enumerate(letters)}, L: F(0)} for i, L in enumerate(letters)]
    pts += [{M: pick(M, i + 2 * j) for j, M in enumerate(letters)} for i in range(3)]
    judged = 0
    for pt in pts:
        if any(pt[L] == 0 and f"{L}≠0" in rows for L in letters) or pole(pt):
            continue
        e = value(at(B, pt))
        if (hi == INF and abs(e) >= D(3) / 4) or (hi != INF and close(e, D(1))):
            continue
        judged += 1
        first = None
        for n in range(lo - 1, -1) if hi != INF and 0 in pt.values() else ():
            try:
                value(sub(at(answer, pt), "N", n))
                break
            except (ValueError, ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero):
                first = n + 1
        if first is not None:
            named.append(f"N<{first}")
        why = judge_value(at(answer, pt), tail_sum(at(summand, pt), lo)) if hi == INF else \
            judge_n(at(summand, pt), lo, at(answer, pt), points=12, first=first)
        if why:
            return f"at {', '.join(f'{L}={x}' for L, x in pt.items())}: {why}", named
    return ("" if judged else "no point judged"), named


# CAS 026 (decision 90, abacus #9719/#9720): a factor F of the ratio that holds Z and isn't a letter at a whole power. Its
# zeros (F put to 0 exactly in the text, since the reader's SIN(π) isn't 0, Z at that value elsewhere), and Z where the
# answer is valued off them. EXP, COSH and 2^Z have none, so any row on them is for nothing
F026 = {"SIN(Z)": (["0", "π"], ["(7÷10)", "(-2÷5)", "(13÷10)"]), "COS(Z)": (["(π÷2)"], ["(7÷10)", "(-2÷5)", "(13÷10)"]),
        "TAN(Z)": (["0", "π"], ["(7÷10)", "(-2÷5)", "(1÷3)"]), "SQRT(Z)": (["0"], ["(7÷10)", "(8÷5)", "(12÷5)"]),
        "Z^(1÷3)": (["0"], ["(7÷10)", "(8÷5)", "(12÷5)"]), "ABS(Z)": (["0"], ["(7÷10)", "(-2÷5)", "(8÷5)"]),
        "LN(Z)": (["1"], ["(7÷10)", "(8÷5)", "(12÷5)"]), "EXP(Z)": ([], ["(7÷10)", "(-2÷5)", "(-13÷10)"]),
        "COSH(Z)": ([], ["(7÷10)", "(-2÷5)", "(1÷3)"]), "2^Z": ([], ["(7÷10)", "(-2÷5)", "(-13÷10)"])}


# CAS 030 (work/030-factor-row-to-n.md): the factors whose row stands to N where the answer has no value at their zero
# (the 24's shape), SIN, COS and TAN of a letter, LN, LOG, ACOS and ACOSH; those 0 only at their letter's 0, left to its
# row; and a function of something else, named. SIN's, COS's and TAN's zeros are the angle mode's
ROW030 = ("SIN(Z)", "COS(Z)", "TAN(Z)", "LN(Z)", "LOG(Z)", "ACOS(Z)", "ACOSH(Z)")
F030 = {"LOG(Z)": (["1"], ["(7÷10)", "(8÷5)", "(12÷5)"]), "ACOS(Z)": (["1"], ["(7÷10)", "(-2÷5)", "(1÷3)"]),
        "ACOSH(Z)": (["1"], ["(8÷5)", "(12÷5)", "(3÷2)"]), "ASIN(Z)": (["0"], ["(7÷10)", "(-2÷5)", "(1÷3)"]),
        "ATAN(Z)": (["0"], ["(7÷10)", "(-2÷5)", "(13÷10)"]), "SINH(Z)": (["0"], ["(7÷10)", "(-2÷5)", "(13÷10)"]),
        "TANH(Z)": (["0"], ["(7÷10)", "(-2÷5)", "(13÷10)"]), "SIN(2×Z)": (["0", "(π÷2)"], ["(7÷10)", "(-2÷5)", "(13÷10)"])}
ZEROS_IN = {("SIN(Z)", "DEG"): ["0", "180"], ("SIN(Z)", "GRAD"): ["0", "200"], ("COS(Z)", "DEG"): ["90"],
            ("COS(Z)", "GRAD"): ["100"], ("TAN(Z)", "DEG"): ["0", "180"], ("TAN(Z)", "GRAD"): ["0", "200"],
            ("SIN(2×Z)", "DEG"): ["0", "90"], ("SIN(2×Z)", "GRAD"): ["0", "100"]}
# and the points off them at the angles RAD's stand for (7÷10 is about 40° and 45 grads), so a ratio to ∞ converges
# there as in RAD
OFFS_IN = {"DEG": ["40", "-23", "74"], "GRAD": ["45", "-25", "83"]}


def fzeros(fac, angle):
    zeros, offs = F026[fac] if fac in F026 else F030[fac]
    if angle in OFFS_IN and fac in ("SIN(Z)", "COS(Z)", "TAN(Z)", "SIN(2×Z)"):
        offs = OFFS_IN[angle][:2] + (["19"] if fac == "TAN(Z)" and angle == "DEG" else
                                     ["21"] if fac == "TAN(Z)" else OFFS_IN[angle][2:])
    return ZEROS_IN.get((fac, angle), zeros), offs


def letter_sum(rnd, fac, los, p_inf, zero_at_lo=False):
    """q(K) × F^(m×K+g) × Y^(b×K+f) for gen_letter4, gen_letter5 and gen_letter6, in gen_letter4's order of draws
    (zero_at_lo: q is always 0 at lo, its draw still made)"""
    m, b = rnd.choice((1, 1, 2, 3, -1)), rnd.choice((1, 1, 2))
    g, f = rnd.choice((0, -1, -1, -2, 1)), rnd.choice((0, 0, -1, 1))
    co = [rand_frac(rnd, True) for _ in range(rnd.choice((0, 0, 1)))] + [rand_frac(rnd)]
    lo = rnd.choice(los)
    if rnd.random() < 0.3 or zero_at_lo:    # q × (K - lo): a zero coefficient where the term divides (#9563)
        co = [(co[i - 1] if i else 0) - lo * (co[i] if i < len(co) else 0) for i in range(len(co) + 1)]

    def pw(base, m, e):
        ex = ("K" if m == 1 else "-K" if m == -1 else f"{m}×K") + (f"+{e}" if e > 0 else f"{e}" if e < 0 else "")
        base = f"({base})" if base == "2^Z" or base == "Z^(1÷3)" else base
        return f"{base}^K" if ex == "K" else f"{base}^({ex})"
    parts = [f"({poly_text(co)})"] if len(co) > 1 or co[0] != 1 else []
    summand = "×".join(parts + [pw(fac, m, g), pw("Y", b, f)])
    fb = f"({fac})" if fac in ("2^Z", "Z^(1÷3)") else fac
    return summand, lo, INF if rnd.random() < p_inf else "N", ("LETTER4", fac, f"{fb}^({m})×Y^({b})", b, f)


def gen_letter4(rnd):
    """CAS 026: q(K) × F^(m×K+g) × Y^(b×K+f), F one of F026 at power m = 1, 2, 3 (CAS 027) or -1, Y at 1 or 2, lo 0..3
    (and -1), to N and to ∞, written as canon reads it (#9563)"""
    fac = rnd.choice(sorted(F026))
    summand, lo, hi, truth = letter_sum(rnd, fac, (-1, 0, 0, 1, 1, 2, 3), 0.35)
    return summand, lo, hi, "RAD", truth


def gen_letter5(rnd):
    """CAS 030: gen_letter4's sums for ROW030's factors (twice as often), the four 0 only at their letter's 0 and
    SIN(2×Z), lo -2..2, mostly to N, in each angle mode"""
    fac = rnd.choice(ROW030 * 2 + ("ASIN(Z)", "ATAN(Z)", "SINH(Z)", "TANH(Z)", "SIN(2×Z)"))
    summand, lo, hi, truth = letter_sum(rnd, fac, (-2, -1, 0, 0, 1, 1, 2), 0.2)
    angle = rnd.choice(("RAD", "DEG", "GRAD"))
    # ACOS's, ASIN's and ATAN's values are angles (ATAN(7÷10) is 35 in DEG), and to ∞ their ratio is past the judge's
    # 3÷4 at every point, so to ∞ they are RAD's
    return summand, lo, hi, "RAD" if hi == INF and fac in ("ACOS(Z)", "ASIN(Z)", "ATAN(Z)") else angle, truth


def gen_said(rnd):
    """CAS 032: a sum whose answer has no value at Z = 0, and whether a kept row says Z≠0 by the rule (work/032-
    redundant-row.md, h032's reading #10639): a ratio that is 1 at Z = 0 (its ≠1 and ABS rows are false there), Y÷F
    with F(Z)≠0 kept, and 1+Z×G with G Y's polynomial or a function with a value everywhere of it (says it; CAS 034:
    to ∞ too, where the row is ABS(Z×G+1)<1) or Y under a division, a root or another function (doesn't: no value at
    Z = 0 for some Y)"""
    kind = rnd.choice(("ratio", "over", "poly", "nonpoly"))
    q = rnd.choice(("", "K×", "(K+1)×", "2×"))
    if kind == "ratio":
        f = rnd.choice(("COS(Z)", "EXP(Z)", "COSH(Z)", "(Z^2+1)", "(SIN(Z)+1)", "COS(2×Z)", "(1÷COSH(Z))"))
        return f"{q}{f}^K", rnd.choice((0, 1)), rnd.choice(("N", INF)), True
    if kind == "over":
        return f"(Y÷{rnd.choice(('SIN(Z)', 'TAN(Z)'))})^K", 1, "N", True
    g = rnd.choice(("Y", "Y^2", "(Y-1)^2", "(Y+2)", "3×Y") if kind == "poly" else
                   ("Y^(-1)", "(Y-1)^(-1)", "SQRT(Y)", "TAN(Y)", "LN(Y)", "ABS(Y)", "EXP(Y)", "SIN(Y)"))
    hi = rnd.choice(("N", INF))
    return f"(1+Z×{g})^K", 0, hi, kind == "poly" or g in ("ABS(Y)", "EXP(Y)", "SIN(Y)")


def judge_said(summand, lo, hi, out, said, angle="RAD"):
    """'' when out keeps Z≠0 just where no kept row says it (NOT_SAID: a row the rule drops, kept; a drop beyond the rule:
    one it doesn't allow), and to N, the first part is the running sum at generic Y and Z"""
    rows = out.split("; ")[1:]
    if said and "Z≠0" in rows:
        return f"{out}: NOT_SAID, Z≠0 kept beside a row that says it (CAS 032)"
    if not said and "Z≠0" not in rows:
        return f"{out}: Z≠0 dropped though no row says it by the rule (CAS 032)"
    if hi == "N":
        pt = {"Y": "1÷5", "Z": "7÷10"}
        s, o = summand, out.split("; ")[0]
        for L, v in pt.items():
            s, o = sub(s, L, v), sub(o, L, v)
        return judge_n(s, lo, o, angle, points=8)
    return ""


DIV033 = {"Y": [("Y", "0")], "Y^2": [("Y", "0")], "(2×Y)": [("Y", "0")], "(Y-1)": [("Y", "1")], "(Y+2)": [("Y", "-2")],
          "(Y^2-1)": [("Y", "1"), ("Y", "-1")], "SIN(Y)": [("SIN(Y)", "π")], "COS(Y)": [("COS(Y)", "π÷2")],
          "LN(Y)": [("LN(Y)", "1")], "(X×Y)": [("Y", "0")], "(Y^2+1)": [], "EXP(Y)": []}


TOTAL034 = ("ABS", "SIN", "COS", "EXP", "ALOG", "SINH", "COSH", "TANH", "ATAN", "ASINH")
PART034 = {"TAN": ("Y", "2×Y", "Y+1"), "SQRT": ("Y", "2×Y", "Y+1"), "LN": ("Y", "Y+1"), "LOG": ("Y", "2×Y"),
           "ASIN": ("Y", "2×Y"), "ACOS": ("Y", "2×Y"), "ACOSH": ("Y+1",), "ATANH": ("Y", "2×Y")}
SPECIAL034 = ("-2", "-1", "-1÷2", "0", "1÷2", "1", "2", "π÷2", "π", "-π÷2", "100", "90")
ROW034 = re.compile(r"^(.*?)(≠|<|>|≥|≤)(.*)$")              # ≥ and ≤: CAS 035's rows


def gen_total(rnd):
    """CAS 034 (work/034-total-functions.md): (1+Z×F(P))^K with P Y's polynomial, from 0 or 1, to N or ∞, in an
    angle mode. Its ratio is 1 at Z = 0, so the line has Z≠0, and the row beside it is 1 at Z = 0 whatever Y is wherever
    F has a value: said where F is one of the ten with a value at every real input, not where F is TAN, SQRT, LN, LOG,
    ASIN, ACOS, ACOSH or ATANH (each without one somewhere: P's from their arguments with a value at generic Y)"""
    if rnd.random() < 0.5:
        f, said = rnd.choice(TOTAL034), True
        q = rnd.choice(("Y", "2×Y", "Y+1", "Y^2", "Y-1"))
    else:
        f, said = rnd.choice(sorted(PART034)), False
        q = rnd.choice(PART034[f])
    return (f"{rnd.choice(('', 'K×', '2×'))}(1+Z×{f}({q}))^K", rnd.choice((0, 1)), rnd.choice(("N", INF)),
            rnd.choice(("RAD", "DEG", "GRAD")), said)


def says034(rows, angle):
    """whether a row of rows is false at Z = 0 with a value, at every special Y (the card's sentence: false at L = 0 for
    every value of the others), TAN with no value at its poles"""
    for r in rows:
        m = ROW034.match(r)
        if not m:
            continue
        l, op, rr = m.groups()
        ok = True
        for y in SPECIAL034:
            try:
                a = value(sub(sub(l, "Z", "0"), "Y", y), angle, strict=True)
                b = value(sub(sub(rr, "Z", "0"), "Y", y), angle, strict=True)
            except (ValueError, ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero, OverflowError):
                ok = False
                break
            if not (abs(a - b) < TOL if op == "≠" else a >= b - TOL if op == "<" else a <= b + TOL if op == ">" else
                    a < b - TOL if op == "≥" else a > b + TOL):
                ok = False
                break
        if ok:
            return True
    return False


def judge_total(summand, lo, hi, out, said, angle):
    """'' when out keeps Z≠0 just where no kept row says it, by the rule and by the sentence at special values of Y;
    to N, the first part is the running sum at generic Y and Z"""
    rows = out.split("; ")[1:]
    if said and "Z≠0" in rows:
        return f"{out}: NOT_SAID, Z≠0 kept beside a row that says it (CAS 034)"
    if not said and "Z≠0" not in rows:
        return f"{out}: Z≠0 dropped though no row says it by the rule (CAS 034)"
    if "Z≠0" not in rows and not says034(rows, angle):
        return f"{out}: Z≠0 dropped, no kept row false at Z = 0 with a value at every special Y (CAS 034)"
    if hi == "N":
        pt = {"Y": "1÷5", "Z": "7÷10"}
        s, o = summand, out.split("; ")[0]
        for L, v in pt.items():
            s, o = sub(s, L, v), sub(o, L, v)
        return judge_n(s, lo, o, angle, points=8)
    return ""


def gen_div(rnd):
    """CAS 033: B holding Y under a division inside a sum, to ∞: (summand, lo, the divisor's zeros). A zero is the
    letter's value, or for a function of Y the point where it is 0 (π for SIN, π÷2 for COS, 1 for LN)"""
    d = rnd.choice(sorted(DIV033))
    sh = rnd.choice(("(Z÷D+1)^K", "(Z÷D+2)^K", "(2×Z÷D-1)^K", "(X+Z÷D)^K", "K×(Z÷D+1)^K", "(Z÷D+1)^(K+1)",
                     "(K+1)×(Z÷D+1)^K"))
    return sh.replace("D", d), rnd.choice((-1, 0, 1)), d


def judge_div(summand, lo, out, d):
    """'' when out has no value at the divisor's zeros, or a row with a value there excludes them: there every term from
    K = 1 has no value, so the sum has none. A function's zero is put in exactly (SIN(π) is 1E-60 in Decimal)"""
    if out in STATUS or out in (INF, NINF) or not out:
        return ""
    parts = out.split("; ")
    for at, v in DIV033[d]:
        pt = {"X": "1÷3", "Z": "7÷10", "Y": v}

        def val(t):
            if at != "Y":
                t = t.replace(at, "(0)")
            for L, x in pt.items():
                t = sub(t, L, x)
            y = value(t)
            if not y.is_finite() or abs(y) > D(10) ** 25:
                raise ZeroDivisionError(t)
            return y
        try:
            val(parts[0])
        except (ValueError, ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero, OverflowError):
            continue
        excluded = False
        for r in parts[1:]:
            m = re.match(r"^(.*?)(≠|<|>|≥|≤)(.*)$", r)
            try:
                a, b = val(m.group(1)), val(m.group(3))
            except (ValueError, ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero, OverflowError):
                continue
            op = m.group(2)
            if (abs(a - b) <= TOL) if op == "≠" else (a >= b - TOL if op == "<" else a <= b + TOL if op == ">" else
                                                        a < b - TOL if op == "≥" else a > b + TOL):
                excluded = True
                break
        if not excluded:
            return f"{out}: a value where {at} is 0 (Y = {v}), where the sum has none (CAS 033)"
    return ""


def gen_letter6(rnd):
    """CAS 031: the 12's shape, gen_letter4's sums from lo -3..-1 to ∞ with q 0 at lo (the term at lo is 0), F any of
    F026's and F030's, SIN's, COS's, TAN's and SIN(2×Z)'s in each angle mode"""
    fac = rnd.choice(sorted(F026) + sorted(F030))
    summand, lo, hi, truth = letter_sum(rnd, fac, (-3, -2, -1, -1), 1.0, zero_at_lo=True)
    angle = rnd.choice(("RAD", "DEG", "GRAD"))
    return summand, lo, hi, angle if fac in ("SIN(Z)", "COS(Z)", "TAN(Z)", "SIN(2×Z)") else "RAD", truth


def judge_letter4(summand, lo, hi, out, fac, B, b, f, angle="RAD"):
    """('' when out ('answer; rows') is right, what it names). At each zero of F (F put to 0, Z there elsewhere, Y at
    1÷3 and -1÷4): where a term from lo has no value, the answer must not claim one (a value, and no row with a value
    false there: abacus #9687 (2)); F≠0 where the sum has a value at every zero is for nothing, and F with no zero gets
    none. Y≠0 stands where a term divides by Y; a Z≠0 the sum doesn't need is named (the probe's, from the answer's
    own hole at Z = 0, as at 0c84d44). One row for the ratio, its value B; the answer is the sum off F's zeros (to ∞
    inside ABS(B) < 3÷4). CAS 027 (abacus #10004, #10005): at each zero of F where the sum has a value and no row with a
    value is false, the answer has one, and it is the sum's; to N, a point where the sum has none for a larger N is the
    24's shape (decision 90 (3)), named "missing N", and to ∞ from lo < 0 with q 0 at lo (its term at lo is 0 off F's
    zeros) the rational twin's row for nothing (#10068), named "missing ∞" (abacus's judge also asks that 1.0.0 missed
    each). A row whose zero set is F's reads as F's row (Z^(2÷3)≠0 as Z^(1÷3)≠0). CAS 030: for ROW030's factors the
    24's shape is a missing row, but where the line with F≠0 would pass 191 B, which gives way ("missing cap"); F's
    zeros and the values are the angle mode's. CAS 031: #10068's shape (the 12's) is refused, no longer named: the
    closed form's common factor is cancelled, so the answer has the sum's value there"""
    if out in STATUS or not out or out.startswith("DECIMAL_FORM"):
        return f"{out} for an answer with rows", []
    parts = out.split("; ")
    answer, rows = parts[0], parts[1:]
    zeros, offs = fzeros(fac, angle)
    named = []

    def put(text, z, y, n=None):
        t = text.replace(fac, "(0)") if z in zeros else text
        t = sub(sub(t, "Z", z), "Y", y)
        return sub(t, "N", str(n)) if n is not None else t

    def has(z, y, last):
        return all(term(put(summand, z, y), k, angle) is not None for k in range(lo, last + 1))

    def same(r):
        """r is p≠0, p a power of F or of Z, 0 at each of F's zeros and at none of the points off them"""
        m = re.fullmatch(rf"((\(?{re.escape(fac)}\)?|Z)\^.+)≠0", r)
        try:
            return bool(m) and all(value(put(m.group(1), z, "0"), angle) == 0 for z in zeros) and \
                all(value(sub(m.group(1), "Z", z), angle) != 0 for z in offs)
        except (ValueError, ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero):
            return False
    rows = [f"{fac}≠0" if same(r) else r for r in rows]

    def valued(z, y, n):
        try:
            value(put(answer, z, y, n), angle)
        except (ValueError, ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero):
            return False
        return True

    def excluded(z, y, n):
        """a row with a value there is false"""
        for r in rows:
            m = re.fullmatch(r"(.+?)(≠|<|>|≥|≤)(.+)", r)
            try:
                a, c = value(put(m.group(1), z, y, n), angle), value(put(m.group(3), z, y, n), angle)
            except (ValueError, ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero):
                continue                                # no value there: it excludes nothing
            if not {"≠": a != c, "<": a < c, ">": a > c, "≥": a >= c, "≤": a <= c}[m.group(2)]:
                return True
        return False
    ns = range(lo - 1, lo + 6) if hi != INF else [None]
    need = False
    for z in zeros:
        for y in ("(1÷3)", "(-1÷4)"):
            for n in ns:
                h = has(z, y, n if n is not None else lo + 12)
                need = need or not h
                if not h and valued(z, y, n) and not excluded(z, y, n):
                    return f"a value where {fac} = 0 (Z = {z}, Y = {y}, N = {n}) and the sum has none", named
                if not h or excluded(z, y, n):
                    continue
                if not valued(z, y, n):
                    if n is not None and not has(z, y, lo + 12):
                        kind = "missing N"              # the 24's shape: to N, none for a larger N
                        if fac in ROW030 and hi == "N":
                            if len(f"{out}; {fac}≠0".encode()) <= 191:
                                return (f"no value where {fac} = 0 (Z = {z}, Y = {y}, N = {n}) and the sum has one: "
                                        f"the 24's shape, with no {fac}≠0 (CAS 030)"), named
                            kind = "missing cap"        # the row gives way at the cap
                    elif n is None and lo < 0 and term(sub(sub(summand, "Z", offs[0]), "Y", "(1÷3)"), lo, angle) == 0:
                        return (f"no value where {fac} = 0 (Z = {z}, Y = {y}, N = {n}) and the sum has one: the 12's "
                                f"shape, to ∞ from lo < 0 with its term at lo 0 (CAS 031)"), named
                    else:
                        return f"no value where {fac} = 0 (Z = {z}, Y = {y}, N = {n}) and the sum has one", named
                    if kind not in named:
                        named.append(kind)
                    continue
                pt = put(summand, z, y)
                want = tail_sum(pt, lo, angle) if n is None else sum((term(pt, k, angle) for k in range(lo, n + 1)), D(0))
                why = judge_value(put(answer, z, y, n), want, angle)
                if why:
                    return f"at {fac} = 0 (Z = {z}, Y = {y}, N = {n}): {why}", named
    if f"{fac}≠0" in rows and not need:
        return f"{fac}≠0 for nothing", named
    want = not all(term(sub(sub(summand, "Z", offs[0]), "Y", "0"), k, angle) is not None for k in range(lo, lo + 13))
    if want != ("Y≠0" in rows):
        if not want and (f < 0 or lo < 0):
            named.append("Y")                       # the shift refused past F, or lo < 0 (025's named classes)
        else:
            return f"Y≠0 {'missing' if want else 'for nothing'}", named
    if "Z≠0" in rows:
        try:
            ok = all(term(sub(sub(summand, "Z", "0"), "Y", "(1÷3)"), k, angle) is not None for k in range(lo, lo + 13))
        except ValueError:
            ok = False
        if ok:
            named.append("Z")
    rest = [r for r in rows if r not in ("Y≠0", "Z≠0", f"{fac}≠0")]
    rest = [r for r in rest if not typed_row(summand, r)]     # CAS 035's, judged by judge_typed
    mt = re.match(r"^ABS\((.*)\)<(.*)$", rest[0]) if hi == INF and len(rest) == 1 else \
        re.match(r"^(.*)≠(.*)$", rest[0]) if len(rest) == 1 else None
    if not mt:
        return f"rows {rest} for the ratio's one row", named
    E, v = mt.group(1), mt.group(2)
    for z, y in ((offs[0], "(2÷3)"), (offs[1], "(-5÷4)")):
        if not close(value(sub(sub(E, "Z", z), "Y", y), angle) / value(v, angle),
                     value(sub(sub(B, "Z", z), "Y", y), angle)):
            return f"row {rest[0]}: not the ratio {B} at Z = {z}, Y = {y}", named
    judged = 0
    for z in offs:
        for y in ("(1÷5)", "(-1÷7)", "(1÷9)", "(3÷2)"):
            pt_s = sub(sub(summand, "Z", z), "Y", y)
            if any(term(pt_s, k, angle) is None for k in range(lo, lo + 13)):
                continue
            e = value(sub(sub(B, "Z", z), "Y", y), angle)
            if (hi == INF and abs(e) >= D(3) / 4) or (hi != INF and close(e, D(1))):
                continue
            judged += 1
            a = sub(sub(answer, "Z", z), "Y", y)
            why = judge_value(a, tail_sum(pt_s, lo, angle), angle) if hi == INF else judge_n(pt_s, lo, a, angle, points=12)
            if why:
                return f"at Z = {z}, Y = {y}: {why}", named
    return ("" if judged else "no point judged"), named


def gen_quot(rnd):
    """CAS 029 (abacus #10313): a quotient of powers in K, n÷d, typed as a user would. n = q(K) × c^K × X^(a×K+e);
    d = C^K × Y^(b×K+f), with X^(a2×K) in d a quarter of the time (a2 > a, so X's power still falls in canon) and W^K a
    fifth of the time; lo from -2 to 2, to N and to ∞. One in eight is a shape the rule must refuse: canon cancelling to
    a power rising in K (X^(2×K)÷X^K), or a divisor falling in K (X^K÷Y^(-K))"""
    lo = rnd.choice((-2, -1, 0, 0, 1, 1, 2))
    hi = INF if rnd.random() < 0.3 else "N"
    if rnd.random() < 0.125:
        a = rnd.choice((1, 2))
        s = rnd.choice((f"X^({a + 1}×K)÷X^({a}×K)" if a > 1 else "X^(2×K)÷X^K", "X^K÷Y^(-K)", f"X^({a}×K)÷Y^(-{a}×K)"))
        return s, lo, hi, "RAD", ("QUOT", True)

    def pw(L, m, e=0):
        ex = ("K" if m == 1 else f"{m}×K") + (f"+{e}" if e > 0 else f"{e}" if e < 0 else "")
        return f"{L}^K" if ex == "K" else f"{L}^({ex})"
    a, e = rnd.choice((1, 1, 2, -1)), rnd.choice((0, 0, 1, -1))
    b, f = rnd.choice((1, 1, 2)), rnd.choice((0, 0, 1, -1, 2))
    c, C = rnd.choice((F(1), F(1), F(2), F(1, 2), F(-1))), rnd.choice((F(1), F(1), F(3), F(-2)))
    co = [rand_frac(rnd, True) for _ in range(rnd.choice((0, 0, 0, 1)))] + [rand_frac(rnd)]
    num = ([f"({poly_text(co)})"] if len(co) > 1 or co[0] != 1 else []) + ([f"{ft(c)}^K"] if c != 1 else []) + \
        [pw("X", a, e)]
    den = ([f"{ft(C)}^K"] if C != 1 else []) + [pw("Y", b, f)]
    if rnd.random() < 0.25:
        den.append(pw("X", max(a, 0) + rnd.choice((1, 2))))
    if rnd.random() < 0.2:
        den.append("W^K")
    d = den[0] if len(den) == 1 else f"({'×'.join(den)})"
    return f"{'×'.join(num)}÷{d}", lo, hi, "RAD", ("QUOT", False)


def quot_claim(answer, rows, n=None):
    """the answer's value at N = n, or None where it has none, or where a row with a value is false (#9727)"""
    def v(t):
        return value(sub(t, "N", n) if n is not None else t)
    for r in rows:
        m = re.fullmatch(r"(.+?)(≠|<|>|≥|≤)(.+)", r)
        try:
            a, b = v(m.group(1)), v(m.group(3))
        except (ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero):
            continue
        if not {"≠": a != b, "<": a < b, ">": a > b, "≥": a >= b, "≤": a <= b}[m.group(2)]:
            return None
    try:
        return v(answer)
    except (ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero):
        return None


def judge_quot_values(summand, lo, hi, out):
    """'' when out has no wrong value for the typed quotient: wherever out has a value (each letter 0 in turn, and three
    points without), the typed terms from lo have values (0^0 = 1) and sum to it: to N from N = lo-1, to ∞ until 8
    terms in a row are below 10^-65 (terms that don't shrink in 3,000 are no value)"""
    parts = out.split("; ")
    answer, rows = parts[0], parts[1:]
    if answer in (INF, NINF):
        return ""
    letters = [L for L in "XYWZ" if re.search(rf"(?<![A-Z]){L}(?![A-Z])", summand)]
    grid = (F(1, 2), F(-1, 3), F(2), F(-3, 2))
    pts = [{**{M: grid[(i + j) % 4] for j, M in enumerate(letters)}, L: F(0)} for i, L in enumerate(letters)]
    pts += [{M: grid[(i + 2 * j) % 4] for j, M in enumerate(letters)} for i in range(3)]
    for pt in pts:
        def at(text):
            for L, x in pt.items():
                text = sub(text, L, ft(x))
            return text
        A, R, S = at(answer), [at(r) for r in rows], at(summand)
        where = ", ".join(f"{L}={ft(x)}" for L, x in pt.items())
        if hi == INF:
            got = quot_claim(A, R)
            if got is None:
                continue
            run, k, small = D(0), lo, 0
            while k < lo + 3000 and not (k > lo + 20 and small >= 8):
                t = term(S, k, "RAD")
                if t is None:
                    return f"{out} at {where}: a value where term {k} has none"
                run += t
                small = small + 1 if abs(t) < D(10) ** -65 else 0
                k += 1
            if small < 8:
                return f"{out} at {where}: a value where the terms don't shrink"
            if not close(got, run):
                return f"{out} at {where} is {got:.25} for {run:.25}"
            continue
        run, none = D(0), None
        for n in range(lo - 1, lo + 12):
            if n >= lo and none is None:
                t = term(S, n, "RAD")
                if t is None:
                    none = n
                else:
                    run += t
            got = quot_claim(A, R, n)
            if got is None:
                continue
            if none is not None:
                return f"{out} at {where}, N={n}: a value where term {none} has none"
            if not close(got, run):
                return f"{out} at {where}, N={n} is {got:.25} for {run:.25}"
    return ""


def judge_quot(summand, lo, hi, out, refuse):
    """'' when out is right for a typed quotient (CAS 029, abacus #10313): a shape the rule refuses is NO_CLOSED_FORM;
    any other is, byte for byte, the answer for canon's own spelling (simplify's text), with no wrong value for the typed
    quotient (judge_quot_values)"""
    if refuse:
        return "" if out == "NO_CLOSED_FORM" else f"{out} for NO_CLOSED_FORM: the rule must refuse it"
    canon = subprocess.run([CLI, "simplify", summand, "RAD"], stdout=subprocess.PIPE, text=True,
                           encoding="utf-8").stdout.strip()
    want = run_sum(canon, lo, hi)
    if out != want:
        return f"{out} is not {want}, the answer for canon's spelling {canon}"
    if out in STATUS or not out:
        return "" if out in ("TOO_LONG", "NO_ROOM") else f"{out}, as canon's spelling {canon}: the rule takes it"
    return judge_quot_values(summand, lo, hi, out)

FAMILIES = [(gen_poly, 3), (gen_geom, 3), (gen_geom2, 1), (gen_tele, 3), (gen_tele_typed, 2), (gen_open, 1),
            (gen_direct, 2), (gen_pole, 1),
            (gen_letter, 2)]


TYPED035 = ("EXP(LN(u))", "ALOG(LOG(u))", "SQRT(u)^2", "SQRT(u)×SQRT(u)", "(u^(1÷2))^2", "(u^(1÷4))^4", "u÷u",
            "LN(u)÷LN(u)", "SQRT(u)÷SQRT(u)", "0×LN(u)", "LN(u)^0")
PTS035 = ("0", "1", "-1", "2", "-2", "1÷2", "-1÷2", "3", "-3")
NOVAL035 = (ValueError, ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero, OverflowError)


def gen_typed(rnd):
    """CAS 035 (work/035-typed-domain.md): a summand whose typed text has no value where canon's has one, by the
    domain rule's cases (canon cancels the part with the hole), of a polynomial u of Y, in CAS 034's shapes, from 0 or 1,
    to N or ∞"""
    u = rnd.choice(("Y", "Y+1", "2×Y", "Y-2", "Y^2-1", "-Y"))
    f = rnd.choice(TYPED035).replace("u", u if u == "Y" else f"({u})")
    shape = rnd.choice(("(1+Z×F)^K", "(Z×F)^K", "Z^K×F", "K×(Z×F)^K"))
    return shape.replace("F", f"({f})"), rnd.choice((0, 1)), rnd.choice(("N", INF))


def judge_typed(summand, lo, hi, out):
    """'' when the line has no value, or a row with a value is false, at each point where the typed term at K = lo has
    none (so the sum has none: to ∞, and to N at N = lo), Y at PTS035 and Z at 1÷2; the values strict"""
    if out in STATUS or not out or out.startswith(("DECIMAL", "NO_", "NOT_", "(rc")):
        return ""
    parts = out.split("; ")
    for y in PTS035:
        pt = {"Y": y, "Z": "1÷2", "N": str(lo)}

        def val(t):
            for L, x in pt.items():
                t = sub(t, L, x)
            v = value(t, "RAD", strict=True)
            if not v.is_finite():
                raise ZeroDivisionError(t)
            return v
        try:
            val(sub(summand, "K", str(lo)))
            continue                                                # the term at lo has a value
        except NOVAL035:
            pass
        try:
            val(parts[0])
        except NOVAL035:
            continue                                                # the line has none either
        excluded = False
        for r in parts[1:]:
            m = ROW034.match(r)
            try:
                a, b = val(m.group(1)), val(m.group(3))
            except NOVAL035:
                continue                                            # a row with no value excludes nothing
            op = m.group(2)
            if not {"≠": abs(a - b) > TOL, "<": a < b - TOL, ">": a > b + TOL, "≥": a >= b - TOL, "≤": a <= b + TOL}[op]:
                excluded = True
                break
        if not excluded:
            return f"{out}: a value at Y = {y}, where the typed summand has none from K = {lo} (CAS 035)"
    return ""


def check_examples():
    """work/018-examples.txt's rows with a whole lo, a symbolic (N) or whole hi and a summand in K alone, by value.
    Returns (rows, failures)"""
    rows = failed = 0
    path = os.path.join(ROOT, "work", "018-examples.txt")
    row = re.compile(r"^\| `casim (?:--arena \d+ )?sum (\S+|'[^']*'|\"[^\"]*\") K (\S+) (\S+) (RAD|DEG|GRAD)` \| `([^`]*)` \|$")
    for line in open(path, encoding="utf-8"):
        m = row.match(line.rstrip("\n"))
        if not m:
            continue
        summand, lo, hi, angle, out = m.group(1).strip("'\""), m.group(2), m.group(3), m.group(4), m.group(5)
        if not re.fullmatch(r"-?\d+", lo) or out in STATUS or out in (INF, NINF):
            continue
        if re.search(r"(?<![A-Z])[A-JL-Z](?![A-Z(])", summand):       # another letter: doc-examples pins its text
            continue
        rows += 1
        if hi == "N":
            why = judge_n(summand, int(lo), out, angle, points=12)
        elif re.fullmatch(r"-?\d+", hi):
            ks = range(int(lo), int(hi) + 1)
            vals = [term(summand, k, angle) for k in ks]
            why = "" if None in vals else judge_value(out, sum(vals, D(0)), angle)
        else:
            rows -= 1
            continue
        if why:
            failed += 1
            print(f"FAIL work/018-examples.txt: sum {summand} K {lo} {hi} {angle}: {why}")
    return rows, failed


def main():
    # the judge refuses its planted answers first, and accepts the right ones
    planted = [("K", 1, "0.5×N+0.5×N^2+1"), ("K", 1, "0.5×N^2+N"), ("1÷(K×(K+1))", 1, "1÷(N+1)+1"),
               ("1÷(K×(K+2))", 1, "-0.5÷(N+1)+0.75"), ("2^K", 0, "2^N-1"), ("K", 1, "NO_CLOSED_FORM")]
    for summand, lo, out in planted:
        if not judge_n(summand, lo, out):
            print(f"FAIL scripts/sum-values.py: its judge accepts {out} for Σ {summand} from {lo}")
            sys.exit(1)
    for summand, lo, out in [("K", 1, "0.5×N+0.5×N^2"), ("1÷(K×(K+1))", 1, "-1÷(N+1)+1"), ("2^K", 0, "2^(N+1)-1")]:
        if judge_n(summand, lo, out):
            print(f"FAIL scripts/sum-values.py: its judge refuses {out} for Σ {summand} from {lo}")
            sys.exit(1)
    if not judge_value("0.76", D(3) / 4) or judge_value("0.75", D(3) / 4) or not judge_value(INF, NINF):
        print("FAIL scripts/sum-values.py: its value judge is wrong on a planted pair")
        sys.exit(1)
    n_cases = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    rnd = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 20261015)
    gens = [g for g, w in FAMILIES for _ in range(w)]
    failed = soft = 0
    kinds = {}
    for _ in range(n_cases):
        gen = rnd.choice(gens)
        summand, lo, hi, angle, truth = gen(rnd)
        out = run_sum(summand, lo, hi, angle)
        kinds[gen.__name__] = kinds.get(gen.__name__, 0) + 1
        if out in ("NO_ROOM", "TOO_LONG") and (isinstance(truth, tuple) or truth not in ("NO_CLOSED_FORM", "UNDEFINED")):
            soft += 1
            continue
        if isinstance(truth, tuple):
            why = judge_letter(summand, lo, hi, out, truth[1], truth[2], rnd)
        else:
            why = judge_n(summand, lo, out, angle) if truth == "N" else judge_value(out, truth, angle)
        if why:
            failed += 1
            print(f"FAIL sum {summand} K {lo} {hi} {angle}: {why}")
    # CAS 024: ratios in two and three letters, their own seeded batch (the 300 above draw as before), and the judge's
    # planted answers first: a row for nothing, a row missing and a value moved must each be refused
    good = ("Y^K×X^(K-1)", 1, "N", "Y×(X×Y)^N÷(X×Y-1)-Y÷(X×Y-1); X×Y≠1", F(1), ["X", "Y"], [1, 1])
    plants = [("Y^K×X^(K-1)", 1, "N", "Y×(X×Y)^N÷(X×Y-1)-Y÷(X×Y-1); X≠0; X×Y≠1"),
              ("X^(K-2)×Y^K", 1, "N", "Y÷(X-X^2×Y)-(X×Y)^(N+1)÷(X^2-X^3×Y); X×Y≠1"),
              ("Y^K×X^(K-1)", 1, "N", "Y×(X×Y)^N÷(X×Y-1)-2×Y÷(X×Y-1); X×Y≠1")]
    if judge_letter2(*good[:2], good[2], good[3], *good[4:]) or \
            any(not judge_letter2(s, lo, hi, out, F(1), ["X", "Y"], [1, 1]) for s, lo, hi, out in plants):
        print("FAIL scripts/sum-values.py: its two-letter judge accepts a planted answer or refuses the right one")
        sys.exit(1)
    rnd2, n2 = random.Random(20261022), 60
    # and held rows of one class the draws rarely reach (h024's R083, abacus #9594): q is 0 at lo while a letter's power
    # there is negative, so the term has no value at that letter's 0 and its row stands, to ∞ as to N
    held2 = [("K×X^K×Y^(K-1)", 0, INF, F(1), ["X", "Y"], [1, 1]), ("K×X^(K-1)×Y^K", 0, INF, F(1), ["X", "Y"], [1, 1]),
             ("(K-1)×X^(K-2)×Y^K", 1, INF, F(1), ["X", "Y"], [1, 1]),
             ("(K-2)×X^(K-3)×Y^(K-1)", 2, INF, F(1), ["X", "Y"], [1, 1]),
             ("K×2^K×X^K×Y^(K-1)", 0, INF, F(2), ["X", "Y"], [1, 1]),
             ("K×X^K×Y^(K-1)×W^K", 0, INF, F(1), ["X", "Y", "W"], [1, 1, 1]),
             ("K×X^K×Y^(K-1)", 0, "N", F(1), ["X", "Y"], [1, 1])]
    cases2 = [gen_letter2(rnd2) for _ in range(n2)] + [(s, lo, hi, "RAD", ("LETTER2", c, ls, ms))
                                                       for s, lo, hi, c, ls, ms in held2]
    for summand, lo, hi, angle, truth in cases2:
        out = run_sum(summand, lo, hi, angle)
        if out in ("NO_ROOM", "TOO_LONG"):
            soft += 1
            continue
        why = judge_letter2(summand, lo, hi, out, *truth[1:])
        if why:
            failed += 1
            print(f"FAIL sum {summand} K {lo} {hi} {angle}: {why}")
    # CAS 025 (abacus #9687): ratios whose B holds an irrational constant, a letter at a negative power or a letter in a
    # function, their own seeded batch, and the judge's planted answers first: rc6's two (R083's irrational twin with
    # no Y≠0, and ruling 2's (X÷Y)^K÷Y, whose ABS row has no value at Y = 0), a row for nothing outside the named class
    # and a value moved must each be refused, and the right answers (one with a named row) taken
    r083 = ("K×(SQRT(2))^K×X^K×Y^(K-1)", 0, INF, "(SQRT(2))×X^(1)×Y^(1)", ["X", "Y"], {"X": 1, "Y": 1},
            {"X": 0, "Y": -1})
    ans = "-X×SQRT(2)÷2÷(X×Y×SQRT(2)-X^2×Y^2-1÷2)"
    neg = ("X^K×Y^(-K-1)", 0, INF, "X^(1)×Y^(-1)", ["X", "Y"], {"X": 1, "Y": -1}, {"X": 0, "Y": -1})
    named = ("(K-1)×(SQRT(2))^K×X^(K-1)×Y^K", 1, INF, "(SQRT(2))×X^(1)×Y^(1)", ["X", "Y"], {"X": 1, "Y": 1},
             {"X": -1, "Y": 0})
    named_out = ("X×Y×SQRT(2)÷(X-X^2×Y×SQRT(2))-X×Y^2÷(X×Y×SQRT(2)-X^2×Y^2-1÷2)+Y×SQRT(2)÷(X×Y×SQRT(2)-1); X≠0; "
                 "ABS(X×Y×SQRT(2))<1")
    takes = [(r083, f"{ans}; Y≠0; ABS(X×Y×SQRT(2))<1"), (neg, "-1÷(X-Y); Y≠0; ABS(X÷Y)<1"), (named, named_out)]
    refuses = [(r083, f"{ans}; ABS(X×Y×SQRT(2))<1"), (neg, "-1÷(X-Y); ABS(X÷Y)<1"),
               (r083, f"{ans}; X≠0; Y≠0; ABS(X×Y×SQRT(2))<1"), (r083, f"{ans}+1÷1000; Y≠0; ABS(X×Y×SQRT(2))<1")]
    if any(judge_letter3(*t[:3], out, *t[3:])[0] for t, out in takes) or \
            judge_letter3(*named[:3], named_out, *named[3:])[1] != ["X"] or \
            any(not judge_letter3(*t[:3], out, *t[3:])[0] for t, out in refuses):
        print("FAIL scripts/sum-values.py: its CAS 025 judge accepts a planted answer or refuses a right one")
        sys.exit(1)
    rnd3, n3 = random.Random(20261025), 60
    held3 = [r083, neg, named, ("K×(π)^K×Y^(K-1)", 0, INF, "(π)×Y^(1)", ["Y"], {"Y": 1}, {"Y": -1}),
             ("K×(SQRT(2))^K×X^(K-1)×Y^(K-1)", 0, INF, "(SQRT(2))×X^(1)×Y^(1)", ["X", "Y"], {"X": 1, "Y": 1},
              {"X": -1, "Y": -1}),
             ("K×SIN(Z)^K×Y^(K-1)", 0, INF, "SIN(Z)×Y^(1)", ["Y", "Z"], {"Y": 1}, {"Y": -1}),
             ("K×X^(K-1)×Y^(-K)", 0, INF, "X^(1)×Y^(-1)", ["X", "Y"], {"X": 1, "Y": -1}, {"X": -1, "Y": 0}),
             ("X^K×Y^(-K)", 1, INF, "X^(1)×Y^(-1)", ["X", "Y"], {"X": 1, "Y": -1}, {"X": 0, "Y": 0}),
             ("(SQRT(2))^K×X^(-K)", 1, INF, "(SQRT(2))×X^(-1)", ["X"], {"X": -1}, {"X": 0}),
             ("K×(SQRT(2))^K×Y^(K-1)", 0, "N", "(SQRT(2))×Y^(1)", ["Y"], {"Y": 1}, {"Y": -1})]
    cases3 = [gen_letter3(rnd3) for _ in range(n3)] + [(s, lo, hi, "RAD", ("LETTER3", *t)) for s, lo, hi, *t in held3]
    n_named = n_empty = n_fn = 0
    for summand, lo, hi, angle, truth in cases3:
        out = run_sum(summand, lo, hi, angle)
        if out in ("NO_ROOM", "TOO_LONG") or out.startswith("DECIMAL_FORM"):
            soft += 1
            continue
        why, named_rows = judge_letter3(summand, lo, hi, out, *truth[1:])
        n_named += any(len(r) == 1 for r in named_rows)
        n_empty += any(r.startswith("N<") for r in named_rows)
        n_fn += any(r.endswith("function") for r in named_rows)
        if why:
            failed += 1
            print(f"FAIL sum {summand} K {lo} {hi} {angle}: {why}")
    # CAS 026 (decision 90, abacus #9720/#9726): a function factor of the ratio, judged at its own zeros. Planted first:
    # 0c84d44's answer (no SIN(Z)≠0), a row on EXP(Z), and a value moved, refused; the right ones taken
    s4 = ("K×SIN(Z)^(K-1)×Y^K", 0, INF, "SIN(Z)", "SIN(Z)^(1)×Y^(1)", 1, 0)
    a4 = "-Y÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2)"
    e4 = ("K×EXP(Z)^(K-1)×Y^K", 0, INF, "EXP(Z)", "EXP(Z)^(1)×Y^(1)", 1, 0)
    ae4 = "-Y÷2÷(Y×EXP(Z)-Y^2×EXP(Z)^2÷2-1÷2)"
    r4 = ("K×SQRT(Z)^(2×K-1)×Y^K", 0, INF, "SQRT(Z)", "SQRT(Z)^(2)×Y^(1)", 1, 0)
    ar4 = "-Y×SQRT(Z)÷2÷(Y×Z-Y^2×Z^2÷2-1÷2)"
    takes4 = [(s4, f"{a4}; SIN(Z)≠0; ABS(Y×SIN(Z))<1"), (e4, f"{ae4}; ABS(Y×EXP(Z))<1"), (r4, f"{ar4}; Z≠0; ABS(Y×Z)<1")]
    refuses4 = [(s4, f"{a4}; ABS(Y×SIN(Z))<1"), (e4, f"{ae4}; EXP(Z)≠0; ABS(Y×EXP(Z))<1"),
                (s4, f"{a4}+1÷1000; SIN(Z)≠0; ABS(Y×SIN(Z))<1"), (r4, f"{ar4}; ABS(Y×Z)<1")]
    if any(judge_letter4(*t[:3], out, *t[3:])[0] for t, out in takes4) or \
            any(not judge_letter4(*t[:3], out, *t[3:])[0] for t, out in refuses4):
        print("FAIL scripts/sum-values.py: its CAS 026 judge accepts a planted answer or refuses a right one")
        sys.exit(1)
    # CAS 027 (abacus #10004, #10005): the missing value at F's zeros. Planted first: 1.0.0's answers for the class (no
    # value at π, to ∞, and to N where the sum has one at every N), and a value moved at F's zeros alone, refused; the
    # build's taken, and the 24's shape (to N, none at a larger N) taken and named. #10068's shape (to ∞ from lo < 0, q
    # 0 at lo) taken and named, and a value removed at F's zeros from lo < 0 with q not 0 there refused. Then the
    # zero-set row (#10004 (5)): the wide seed 20261101's Z^(2÷3)≠0 and a SIN(Z)^2≠0 taken, a Z^2≠0 for SIN(Z)≠0 refused
    s5 = ("K×SIN(Z)^(K-1)×Y^K", 1, INF, "SIN(Z)", "SIN(Z)^(1)×Y^(1)", 1, 0)
    a5 = "-Y÷(Y×SIN(Z)-1)-Y^2×SIN(Z)÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2); ABS(Y×SIN(Z))<1"
    t5 = ("SIN(Z)^(2×K-1)×Y^K", 1, "N", "SIN(Z)", "SIN(Z)^(2)×Y^(1)", 1, 0)
    w5 = ("SIN(Z)^K×Y^K", -1, "N", "SIN(Z)", "SIN(Z)^(1)×Y^(1)", 1, 0)
    z5 = ("((-1)×K)×(Z^(1÷3))^(2×K-2)×Y^(K-1)", 0, INF, "Z^(1÷3)", "(Z^(1÷3))^(2)×Y^(1)", 1, -1)
    n5 = ("(K+1)×SIN(Z)^(K+1)×Y^K", -1, INF, "SIN(Z)", "SIN(Z)^(1)×Y^(1)", 1, 0)
    k5 = ("K×SIN(Z)^(K+1)×Y^K", -1, INF, "SIN(Z)", "SIN(Z)^(1)×Y^(1)", 1, 0)
    ak5 = "-1÷(Y-Y^2×SIN(Z))-SIN(Z)÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2)"
    # (CAS 030: the 24's shape for SIN is a missing row now, so w5 is taken with SIN(Z)≠0, and the shape is named for
    # SIN(2×Z), a function of something else; 1.1.1's w5 is refused below)
    w5o = "1÷(Y×SIN(Z)-Y^2×SIN(Z)^2)-(Y×SIN(Z))^(N+2)÷(Y×SIN(Z)-Y^2×SIN(Z)^2); Y≠0; Z≠0; Y×SIN(Z)≠1"
    v5 = ("SIN(2×Z)^(-K)×Y^K", 1, "N", "SIN(2×Z)", "SIN(2×Z)^(-1)×Y^(1)", 1, 0)
    av5 = "-Y÷(Y-SIN(2×Z))+(Y÷SIN(2×Z))^(N+1)÷(Y÷SIN(2×Z)-1); Z≠0; Y÷SIN(2×Z)≠1"
    takes5 = [(s5, a5), (t5, "Y×SIN(Z)×(Y×SIN(Z)^2)^N÷(Y×SIN(Z)^2-1)-Y×SIN(Z)÷(Y×SIN(Z)^2-1); Y×SIN(Z)^2≠1"),
              (w5, w5o.replace("; Z≠0; ", "; Z≠0; SIN(Z)≠0; ")), (v5, av5),
              (z5, "1÷2÷(Y×Z^(2÷3)-Y^2×Z^(4÷3)÷2-1÷2); Y≠0; Z^(2÷3)≠0; ABS(Y×Z^(2÷3))<1"),
              (s4, f"{a4}; SIN(Z)^2≠0; ABS(Y×SIN(Z))<1"), (k5, f"{ak5}; Y≠0; ABS(Y×SIN(Z))<1"),
              # (CAS 031: n5, the 12's shape, has its value at π now; 1.1.2's line is refused below)
              (n5, "-SIN(Z)÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2); Y≠0; ABS(Y×SIN(Z))<1")]
    refuses5 = [(s5, "Y×SIN(Z)÷(SIN(Z)-Y×SIN(Z)^2)-Y^2×SIN(Z)÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2); Z≠0; ABS(Y×SIN(Z))<1"),
                (t5, "-Y×SIN(Z)÷(Y×SIN(Z)^2-1)-(Y×SIN(Z)^2)^(N+1)÷(SIN(Z)-Y×SIN(Z)^3); Z≠0; Y×SIN(Z)^2≠1"),
                (s5, a5.replace("; ", "+(0)^ABS(SIN(Z))÷1000; ", 1)), (s4, f"{a4}; Z^2≠0; ABS(Y×SIN(Z))<1"),
                (k5, f"({ak5})×SIN(Z)÷SIN(Z); Y≠0; ABS(Y×SIN(Z))<1")]
    if any(judge_letter4(*t[:3], out, *t[3:])[0] for t, out in takes5) or \
            "missing N" not in judge_letter4(*v5[:3], av5, *v5[3:])[1] or \
            any(not judge_letter4(*t[:3], out, *t[3:])[0] for t, out in refuses5):
        print("FAIL scripts/sum-values.py: its CAS 027 judge accepts a planted answer or refuses a right one")
        sys.exit(1)
    rnd4, n4 = random.Random(20261026), 60
    held4 = [s4, e4, r4, ("K×COS(Z)^(K-1)×Y^K", 0, INF, "COS(Z)", "COS(Z)^(1)×Y^(1)", 1, 0),
             ("K×SQRT(Z)^(K-1)×Y^K", 0, INF, "SQRT(Z)", "SQRT(Z)^(1)×Y^(1)", 1, 0),
             ("SIN(Z)^(-K)×Y^K", 1, INF, "SIN(Z)", "SIN(Z)^(-1)×Y^(1)", 1, 0),
             ("K×ABS(Z)^(2×K-1)×Y^K", 0, INF, "ABS(Z)", "ABS(Z)^(2)×Y^(1)", 1, 0),
             ("K×LN(Z)^(K-1)×Y^K", 0, INF, "LN(Z)", "LN(Z)^(1)×Y^(1)", 1, 0),
             ("K×TAN(Z)^(K-1)×Y^K", 0, INF, "TAN(Z)", "TAN(Z)^(1)×Y^(1)", 1, 0),
             ("SIN(Z)^K×Y^K", -1, INF, "SIN(Z)", "SIN(Z)^(1)×Y^(1)", 1, 0),
             # CAS 027: the class from lo >= 1 (SIN, COS, TAN, LN), the 24's shape, the letter's own 0 from -1
             s5, t5, w5, z5, n5, k5, ("K×COS(Z)^(K-1)×Y^K", 2, INF, "COS(Z)", "COS(Z)^(1)×Y^(1)", 1, 0),
             ("TAN(Z)^(2×K-1)×Y^K", 1, "N", "TAN(Z)", "TAN(Z)^(2)×Y^(1)", 1, 0),
             ("LN(Z)^(3×K-1)×Y^K", 1, INF, "LN(Z)", "LN(Z)^(3)×Y^(1)", 1, 0),
             ("COS(Z)^(-K)×Y^K", 1, "N", "COS(Z)", "COS(Z)^(-1)×Y^(1)", 1, 0),
             ("SQRT(Z)^(K+1)×Y^K", -1, "N", "SQRT(Z)", "SQRT(Z)^(1)×Y^(1)", 1, 0),
             ("ABS(Z)^(K+1)×Y^K", -1, "N", "ABS(Z)", "ABS(Z)^(1)×Y^(1)", 1, 0),
             ("(Z^(1÷3))^(K+1)×Y^K", -1, "N", "Z^(1÷3)", "(Z^(1÷3))^(1)×Y^(1)", 1, 0)]
    cases4 = [gen_letter4(rnd4) for _ in range(n4)] + [(s, lo, hi, "RAD", ("LETTER4", *t)) for s, lo, hi, *t in held4]
    n4named = n4miss = n4neg = 0
    for summand, lo, hi, angle, truth in cases4:
        out = run_sum(summand, lo, hi, angle)
        if out in ("NO_ROOM", "TOO_LONG") or out.startswith("DECIMAL_FORM"):
            soft += 1
            continue
        why, named_rows = judge_letter4(summand, lo, hi, out, *truth[1:])
        n4named += bool(set(named_rows) - {"missing N", "missing ∞"})
        n4miss += "missing N" in named_rows
        n4neg += "missing ∞" in named_rows
        if why:
            failed += 1
            print(f"FAIL sum {summand} K {lo} {hi} {angle}: {why}")
    # CAS 030 (work/030-factor-row-to-n.md): ROW030's factors keep their row to N where the answer has no value at a
    # zero. Planted first: 1.1.1's answers for the 24's shape (no SIN(Z)≠0, in RAD and DEG; no COS(Z)≠0 in GRAD, at
    # GRAD's 100; and CAS 027's w5), refused; the build's taken; a line the row would take past 191 B taken and named
    # "missing cap", and one under the cap with no row refused
    g6 = ("SIN(Z)^(-K)×Y^K", 1, "N", "SIN(Z)", "SIN(Z)^(-1)×Y^(1)", 1, 0)
    ag6 = "-Y÷(Y-SIN(Z))+(Y÷SIN(Z))^(N+1)÷(Y÷SIN(Z)-1); Z≠0; Y÷SIN(Z)≠1"
    c6 = ("COS(Z)^(-K)×Y^K", 1, "N", "COS(Z)", "COS(Z)^(-1)×Y^(1)", 1, 0)
    ac6 = "-Y÷(Y-COS(Z))+(Y÷COS(Z))^(N+1)÷(Y÷COS(Z)-1); Y÷COS(Z)≠1"
    k6 = ("K×SIN(Z)^(K-1)", -1, "N", "SIN(Z)", "SIN(Z)^(1)", 0, 0)
    ak6 = ("1÷(SIN(Z)-2×SIN(Z)^2+SIN(Z)^3)-1÷(SIN(Z)^2-SIN(Z)^3)-N×SIN(Z)^(N+1)÷(SIN(Z)-SIN(Z)^2)-SIN(Z)^(N+1)÷"
           "(SIN(Z)-SIN(Z)^2)+SIN(Z)^(N+1)÷2÷(SIN(Z)-SIN(Z)^2÷2-1÷2); Z≠0; SIN(Z)≠1")
    j6 = ("K×SIN(Z)^(K-1)", 0, "N", "SIN(Z)", "SIN(Z)^(1)", 0, 0)
    aj6 = ("-1÷2÷(SIN(Z)-SIN(Z)^2÷2-1÷2)-N×SIN(Z)^(N+1)÷(SIN(Z)-SIN(Z)^2)-SIN(Z)^(N+1)÷(SIN(Z)-SIN(Z)^2)+SIN(Z)^(N+1)÷"
           "2÷(SIN(Z)-SIN(Z)^2÷2-1÷2); Z≠0; SIN(Z)≠1")
    row6 = {"Z≠0; Y÷SIN(Z)": "Z≠0; SIN(Z)≠0; Y÷SIN(Z)", "; Y÷COS(Z)": "; COS(Z)≠0; Y÷COS(Z)",
            "Z≠0; SIN(Z)≠1": "Z≠0; SIN(Z)≠0; SIN(Z)≠1"}

    def with6(a):
        k = next(k for k in row6 if k in a)
        return a.replace(k, row6[k])
    takes6 = [(g6, with6(ag6), "RAD"), (g6, with6(ag6), "DEG"), (c6, with6(ac6), "GRAD"), (j6, with6(aj6), "RAD"),
              (k6, ak6, "RAD")]
    refuses6 = [(g6, ag6, "RAD"), (g6, ag6, "DEG"), (c6, ac6, "GRAD"), (j6, aj6, "RAD"), (w5, w5o, "RAD")]
    if any(judge_letter4(*t[:3], out, *t[3:], angle=a)[0] for t, out, a in takes6) or \
            "missing cap" not in judge_letter4(*k6[:3], ak6, *k6[3:])[1] or \
            any(not judge_letter4(*t[:3], out, *t[3:], angle=a)[0] for t, out, a in refuses6):
        print("FAIL scripts/sum-values.py: its CAS 030 judge accepts a planted answer or refuses a right one")
        sys.exit(1)
    rnd6, n6 = random.Random(20261030), 80
    held6 = [(g6, "RAD"), (g6, "DEG"), (g6, "GRAD"), (c6, "RAD"), (c6, "DEG"), (c6, "GRAD"), (j6, "DEG"), (k6, "RAD"),
             ("TAN(Z)^(2×K-1)×Y^K", 0, "N", "TAN(Z)", "TAN(Z)^(2)×Y^(1)", 1, 0, "GRAD"),
             ("LN(Z)^(3×K-1)×Y^K", 0, "N", "LN(Z)", "LN(Z)^(3)×Y^(1)", 1, 0, "RAD"),
             ("LOG(Z)^(-K)×Y^K", 1, "N", "LOG(Z)", "LOG(Z)^(-1)×Y^(1)", 1, 0, "RAD"),
             ("ACOS(Z)^(-K)×Y^K", 0, "N", "ACOS(Z)", "ACOS(Z)^(-1)×Y^(1)", 1, 0, "DEG"),
             ("ACOSH(Z)^(-K)×Y^K", 0, "N", "ACOSH(Z)", "ACOSH(Z)^(-1)×Y^(1)", 1, 0, "RAD"),
             ("(K-1)×COS(Z)^(K-1)×Y^K", 0, "N", "COS(Z)", "COS(Z)^(1)×Y^(1)", 1, 0, "DEG"),
             ("ASIN(Z)^(-K)×Y^K", 1, "N", "ASIN(Z)", "ASIN(Z)^(-1)×Y^(1)", 1, 0, "DEG"),
             ("SINH(Z)^K×Y^K", -1, "N", "SINH(Z)", "SINH(Z)^(1)×Y^(1)", 1, 0, "RAD"),
             (v5, "GRAD"), (n5, "DEG")]
    held6 = [(*t[0], t[1]) if len(t) == 2 else t for t in held6]
    cases6 = [gen_letter5(rnd6) for _ in range(n6)] + [(s, lo, hi, a, ("LETTER4", *t)) for s, lo, hi, *t, a in held6]
    n6miss = n6cap = 0
    for summand, lo, hi, angle, truth in cases6:
        out = run_sum(summand, lo, hi, angle)
        if out in ("NO_ROOM", "TOO_LONG") or out.startswith("DECIMAL_FORM"):
            soft += 1
            continue
        why, named_rows = judge_letter4(summand, lo, hi, out, *truth[1:], angle=angle)
        n6miss += "missing N" in named_rows
        n6cap += "missing cap" in named_rows
        if why:
            failed += 1
            print(f"FAIL sum {summand} K {lo} {hi} {angle}: {why}")
    # CAS 031 (work/031-common-factor-cancel.md): the closed form's common factor is cancelled, so the 12's shape (to ∞
    # from lo < 0, the term at lo 0) has the sum's value at F's zero. Planted first: 1.1.2's lines, 0÷0 there (SIN in
    # RAD, COS in GRAD, TAN and SIN(2×Z) in DEG, LN from -2), refused; the build's taken
    n7 = ("(K+1)×SIN(Z)^(K+1)×Y^K", -1, INF, "SIN(Z)", "SIN(Z)^(1)×Y^(1)", 1, 0)
    c7 = ("(K+1)×COS(Z)^(K+1)×Y^K", -1, INF, "COS(Z)", "COS(Z)^(1)×Y^(1)", 1, 0)
    t7 = ("(K+1)×TAN(Z)^(K+2)×Y^K", -1, INF, "TAN(Z)", "TAN(Z)^(1)×Y^(1)", 1, 0)
    l7 = ("(K+2)×LN(Z)^(K+2)×Y^K", -2, INF, "LN(Z)", "LN(Z)^(1)×Y^(1)", 1, 0)
    d7 = ("(K+1)×SIN(2×Z)^(K+1)×Y^K", -1, INF, "SIN(2×Z)", "SIN(2×Z)^(1)×Y^(1)", 1, 0)
    takes7 = [(n7, "-SIN(Z)÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2); Y≠0; ABS(Y×SIN(Z))<1", "RAD"),
              (c7, "-COS(Z)÷2÷(Y×COS(Z)-Y^2×COS(Z)^2÷2-1÷2); Y≠0; ABS(Y×COS(Z))<1", "GRAD"),
              (t7, "-TAN(Z)^2÷2÷(Y×TAN(Z)-Y^2×TAN(Z)^2÷2-1÷2); Y≠0; ABS(Y×TAN(Z))<1", "DEG"),
              (l7, "LN(Z)÷(Y-2×Y^2×LN(Z)+Y^3×LN(Z)^2); Y≠0; Z≠0; ABS(Y×LN(Z))<1", "RAD"),
              (d7, "-SIN(2×Z)÷2÷(Y×SIN(2×Z)-Y^2×SIN(2×Z)^2÷2-1÷2); Y≠0; ABS(Y×SIN(2×Z))<1", "DEG")]
    refuses7 = [(n7, "Y×SIN(Z)^2÷(Y×SIN(Z)-2×Y^2×SIN(Z)^2+Y^3×SIN(Z)^3); Y≠0; Z≠0; ABS(Y×SIN(Z))<1", "RAD"),
                (c7, "Y×COS(Z)^2÷(Y×COS(Z)-2×Y^2×COS(Z)^2+Y^3×COS(Z)^3); Y≠0; ABS(Y×COS(Z))<1", "GRAD"),
                (t7, "Y×TAN(Z)^3÷(Y×TAN(Z)-2×Y^2×TAN(Z)^2+Y^3×TAN(Z)^3); Y≠0; Z≠0; ABS(Y×TAN(Z))<1", "DEG"),
                (l7, "Y×LN(Z)^3÷(Y^2×LN(Z)^2-2×Y^3×LN(Z)^3+Y^4×LN(Z)^4); Y≠0; Z≠0; ABS(Y×LN(Z))<1", "RAD"),
                (d7, "Y×SIN(2×Z)^2÷(Y×SIN(2×Z)-2×Y^2×SIN(2×Z)^2+Y^3×SIN(2×Z)^3); Y≠0; Z≠0; ABS(Y×SIN(2×Z))<1",
                 "DEG")]
    if any(judge_letter4(*t[:3], out, *t[3:], angle=a)[0] for t, out, a in takes7) or \
            any(not judge_letter4(*t[:3], out, *t[3:], angle=a)[0] for t, out, a in refuses7):
        print("FAIL scripts/sum-values.py: its CAS 031 judge accepts a planted answer or refuses a right one")
        sys.exit(1)
    # held: the grid of the 12 (abacus #10515), five shapes for each factor whose zero isn't its letter's 0, in RAD
    rnd7, n7s = random.Random(20261031), 60
    held7 = [(sh.replace("F", fac), lo, INF, fac, f"{fac}^(1)×Y^(1)", 1, 0, "RAD")
             for fac in ROW030 + ("SIN(2×Z)",)
             for sh, lo in (("(K+1)×F^(K+1)×Y^K", -1), ("(K+1)×F^(K+2)×Y^K", -1), ("(K+2)×F^(K+2)×Y^K", -2),
                            ("(K+3)×F^(K+3)×Y^K", -3), ("(K+1)×F^(K+1)×Y^(K+1)", -1))]
    held7 += [(*t, a) for t, _, a in takes7]
    cases7 = [gen_letter6(rnd7) for _ in range(n7s)] + [(s, lo, hi, a, ("LETTER4", *t)) for s, lo, hi, *t, a in held7]
    for summand, lo, hi, angle, truth in cases7:
        out = run_sum(summand, lo, hi, angle)
        if out in ("NO_ROOM", "TOO_LONG") or out.startswith("DECIMAL_FORM"):
            soft += 1
            continue
        why, named_rows = judge_letter4(summand, lo, hi, out, *truth[1:], angle=angle)
        if why:
            failed += 1
            print(f"FAIL sum {summand} K {lo} {hi} {angle}: {why}")
    # CAS 032 (work/032-redundant-row.md): a letter's row that a kept row says is left out, by the rule and no further
    # (h032's reading, #10639). Planted first: 1.2.0's line with Z≠0 beside COS(Z)≠1 (NOT_SAID), and a line that drops
    # Z≠0 beside Z÷(Y-1)+1≠1, which has no value at Y = 1, refused; the build's taken
    takes8 = [("COS(Z)^K", 1, "N", "-COS(Z)÷(COS(Z)-1)+COS(Z)^(N+1)÷(COS(Z)-1); COS(Z)≠1", True),
              ("(1+Z×(Y-1)^(-1))^K", 0, "N", "1÷Z-Y÷Z+Y×(Z÷(Y-1)+1)^(N+1)÷Z-(Z÷(Y-1)+1)^(N+1)÷Z; Z≠0; Z÷(Y-1)+1≠1",
               False)]
    refuses8 = [("COS(Z)^K", 1, "N", "-COS(Z)÷(COS(Z)-1)+COS(Z)^(N+1)÷(COS(Z)-1); Z≠0; COS(Z)≠1", True),
                ("(1+Z×(Y-1)^(-1))^K", 0, "N", "1÷Z-Y÷Z+Y×(Z÷(Y-1)+1)^(N+1)÷Z-(Z÷(Y-1)+1)^(N+1)÷Z; Z÷(Y-1)+1≠1", False)]
    if any(judge_said(s8, lo, hi, out, said) for s8, lo, hi, out, said in takes8) or \
            any(not judge_said(s8, lo, hi, out, said) for s8, lo, hi, out, said in refuses8):
        print("FAIL scripts/sum-values.py: its CAS 032 judge accepts a planted answer or refuses a right one")
        sys.exit(1)
    rnd8, n8 = random.Random(20261032), 60
    held8 = [("COS(Z)^K", 1, "N", True), ("K×COS(Z)^(K-1)", 1, "N", True), ("EXP(Z)^K", 0, INF, True),
             ("(Y÷SIN(Z))^K", 1, "N", True), ("(1+Z×Y^2)^K", 0, "N", True), ("(1+Z÷Y)^K", 0, "N", False),
             ("(1+Z×SQRT(Y))^K", 0, "N", False), ("(1+Z×ABS(Y))^K", 0, "N", True), ("COS(Y×Z)^K", 1, "N", True),
             ("(1+Z×Y)^K", 0, INF, True)]        # the last three said since CAS 034
    cases8 = [gen_said(rnd8) for _ in range(n8)] + held8
    for summand, lo, hi, said in cases8:
        out = run_sum(summand, lo, hi)
        if out in ("NO_ROOM", "TOO_LONG") or out.startswith("DECIMAL_FORM"):
            soft += 1
            continue
        why = judge_said(summand, lo, hi, out, said)
        if why:
            failed += 1
            print(f"FAIL sum {summand} K {lo} {hi} RAD: {why}")
    # CAS 033 (work/033-divisor-rows.md): to ∞, B holding Y under a division inside a sum has no value where the
    # divisor is 0, and the answer mustn't have one there unless a row excludes the point. Planted first: 1.2.0's
    # lines for abacus's four (#10829), refused; the build's taken
    takes9 = [("(Z÷Y+1)^K", 0, "-Y÷Z; Y≠0; Z≠0; ABS(Z÷Y+1)<1", "Y"),
              ("(Z÷(Y-1)+1)^K", 0, "1÷Z-Y÷Z; Z≠0; Y-1≠0; ABS(Z÷(Y-1)+1)<1", "(Y-1)"),
              ("(Z÷SIN(Y)+1)^K", 0, "-SIN(Y)÷Z; Z≠0; SIN(Y)≠0; ABS(Z÷SIN(Y)+1)<1", "SIN(Y)")]
    refuses9 = [("(Z÷Y+1)^K", 0, "-Y÷Z; Z≠0; ABS(Z÷Y+1)<1", "Y"),
                ("(Z÷(Y-1)+1)^K", 0, "1÷Z-Y÷Z; Z≠0; ABS(Z÷(Y-1)+1)<1", "(Y-1)"),
                ("(Z÷Y^2+1)^K", 0, "-Y^2÷Z; Z≠0; ABS(Z÷Y^2+1)<1", "Y^2"),
                ("(Z÷SIN(Y)+1)^K", 0, "-SIN(Y)÷Z; Z≠0; ABS(Z÷SIN(Y)+1)<1", "SIN(Y)")]
    if any(judge_div(s9, lo, out, d) for s9, lo, out, d in takes9) or \
            any(not judge_div(s9, lo, out, d) for s9, lo, out, d in refuses9):
        print("FAIL scripts/sum-values.py: its CAS 033 judge accepts a planted answer or refuses a right one")
        sys.exit(1)
    rnd9, n9 = random.Random(20261033), 60
    held9 = [("(Z÷Y+1)^K", 0, "Y"), ("(Z÷Y^2+1)^K", 0, "Y^2"), ("(Z÷(Y-1)+1)^K", 0, "(Y-1)"),
             ("(Z÷SIN(Y)+1)^K", 0, "SIN(Y)"), ("(Z÷(Y^2-1)+1)^K", 0, "(Y^2-1)"), ("K×(Z÷COS(Y)+1)^K", 0, "COS(Y)"),
             ("K×(Z÷(Y-1)+1)^K", 1, "(Y-1)"), ("(Z÷LN(Y)+2)^K", -1, "LN(Y)")]
    cases9 = [gen_div(rnd9) for _ in range(n9)] + held9
    for summand, lo, d in cases9:
        out = run_sum(summand, lo, INF)
        if out in ("NO_ROOM", "TOO_LONG") or out.startswith("DECIMAL_FORM"):
            soft += 1
            continue
        why = judge_div(summand, lo, out, d)
        if why:
            failed += 1
            print(f"FAIL sum {summand} K {lo} ∞ RAD: {why}")
    # CAS 034 (work/034-total-functions.md): a row whose other letters are inside a function with a value at every real
    # input says the letter's row. Planted first: 1.3.0's line with Z≠0 beside Z×ABS(Y)+1≠1 (NOT_SAID), and drops of
    # Z≠0 beside Z×TAN(Y)+1≠1 (no value at Y = π÷2) and beside Z×SQRT(Y)+1≠1 (none below 0), refused, the last two by
    # the sentence as well as the rule; the build's taken
    takes10 = [("(1+ABS(Y)×Z)^K", 0, "N", "-1÷Z÷ABS(Y)+(Z×ABS(Y)+1)^(N+1)÷Z÷ABS(Y); Z×ABS(Y)+1≠1", True, "RAD"),
               ("(1+TAN(Y)×Z)^K", 0, "N", "-1÷Z÷TAN(Y)+(Z×TAN(Y)+1)^(N+1)÷Z÷TAN(Y); Z≠0; Z×TAN(Y)+1≠1", False, "RAD")]
    refuses10 = [("(1+ABS(Y)×Z)^K", 0, "N", "-1÷Z÷ABS(Y)+(Z×ABS(Y)+1)^(N+1)÷Z÷ABS(Y); Z≠0; Z×ABS(Y)+1≠1", True, "RAD"),
                 ("(1+TAN(Y)×Z)^K", 0, "N", "-1÷Z÷TAN(Y)+(Z×TAN(Y)+1)^(N+1)÷Z÷TAN(Y); Z×TAN(Y)+1≠1", False, "RAD"),
                 ("(1+SQRT(Y)×Z)^K", 0, "N", "-1÷SQRT(Y)÷Z+(SQRT(Y)×Z+1)^(N+1)÷SQRT(Y)÷Z; SQRT(Y)×Z+1≠1", False, "RAD")]
    if any(judge_total(*c) for c in takes10) or any(not judge_total(*c) for c in refuses10) or \
            any(not judge_total(s, lo, hi, out, True, a) for s, lo, hi, out, _, a in refuses10[1:]):
        print("FAIL scripts/sum-values.py: its CAS 034 judge accepts a planted answer or refuses a right one")
        sys.exit(1)
    rnd10, n10 = random.Random(20261034), 60
    held10 = [("(1+ABS(Y)×Z)^K", 0, INF, "RAD", True), ("(1+ATAN(Y)×Z)^K", 0, "N", "DEG", True),
              ("(1+ATAN(Y)×Z)^K", 0, "N", "GRAD", True), ("(1+SIN(Y)×Z)^K", 0, "N", "DEG", True),
              ("(1+COS(Y)×Z)^K", 0, "N", "GRAD", True), ("(1+ABS(SIN(Y))×Z)^K", 0, "N", "RAD", True),
              ("(1+EXP(COS(Y))×Z)^K", 1, "N", "RAD", True), ("(1+ALOG(Y)×Z)^K", 1, "N", "RAD", True),
              ("(1+TAN(Y)×Z)^K", 0, "N", "RAD", False), ("(1+SQRT(Y)×Z)^K", 0, "N", "RAD", False),
              ("(1+ABS(TAN(Y))×Z)^K", 0, "N", "RAD", False), ("(1+EXP(1÷Y)×Z)^K", 0, "N", "RAD", False),
              ("(1+ATAN(SQRT(Y))×Z)^K", 0, "N", "RAD", False), ("(1+ACOSH(Y+1)×Z)^K", 0, INF, "RAD", False)]
    cases10 = [gen_total(rnd10) for _ in range(n10)] + held10
    for summand, lo, hi, angle, said in cases10:
        out = run_sum(summand, lo, hi, angle)
        if out in ("NO_ROOM", "TOO_LONG") or out.startswith("DECIMAL_FORM"):
            soft += 1
            continue
        why = judge_total(summand, lo, hi, out, said, angle)
        if why:
            failed += 1
            print(f"FAIL sum {summand} K {lo} {hi} {angle}: {why}")
    # CAS 035 (work/035-typed-domain.md): the typed summand's domain. The evaluator's three controls first, each of
    # which fails without its fix: a divisor 0 to 40 digits has no value in strict mode (COS(π÷2) is 2.3E-59 with the
    # 90-digit π), LN(0) has none (Decimal gives -Infinity), and a thread values in the module's 60 digits
    ctl = []
    for t in ("1÷COS((π÷2))", "LN((0))"):
        try:
            value(t, "RAD", strict=True)
            ctl.append(t)
        except NOVAL035:
            pass
    got = {}
    th = threading.Thread(target=lambda: got.setdefault("v", value("1÷3")))
    th.start()
    th.join()
    if ctl or got.get("v") != value("1÷3"):
        print(f"FAIL scripts/sum-values.py: its evaluator values {ctl or 'a thread at another precision'}")
        sys.exit(1)
    # planted: 1.3.0's and 4c0523b's lines with a value where the typed summand has none are refused, the build's taken
    refuses11 = [("(Z×EXP(LN(Y))+1)^K", 0, INF, "-1÷Y÷Z; ABS(Y×Z+1)<1"), ("(Y÷Y)^K", 1, "N", "N"),
                 ("Z^K×SQRT(Y)^2", 0, INF, "-Y÷(Z-1); ABS(Z)<1"), ("(Z×LN(Y)÷LN(Y))^K", 0, INF, "-1÷(Z-1); ABS(Z)<1")]
    takes11 = [("(Z×EXP(LN(Y))+1)^K", 0, INF, "-1÷Y÷Z; Y>0; ABS(Y×Z+1)<1"), ("(Y÷Y)^K", 1, "N", "N; Y≠0"),
               ("Z^K×SQRT(Y)^2", 0, INF, "-Y÷(Z-1); Y≥0; ABS(Z)<1"),
               ("(Z×LN(Y)÷LN(Y))^K", 0, INF, "-1÷(Z-1); Y>0; Y≠1; ABS(Z)<1")]
    if any(not judge_typed(*c) for c in refuses11) or any(judge_typed(*c) for c in takes11):
        print("FAIL scripts/sum-values.py: its CAS 035 judge accepts a planted answer or refuses a right one")
        sys.exit(1)
    rnd11, n11 = random.Random(20261035), 60
    held11 = [("(Z×ALOG(LOG(Y))+1)^K", 0, INF), ("(Z×SQRT(Y)×SQRT(Y)+1)^K", 1, "N"), ("Z^K×(Y-1)÷(Y-1)", 0, INF),
              ("Z^K×SQRT(Y-2)^2", 1, "N"), ("(Z×(Y^(1÷4))^4)^K", 0, INF), ("Z^K×(0×LN(Y)+1)", 0, INF),
              ("Z^K×LN(Y)^0", 1, "N"), ("K×(Z×SQRT(Y)÷SQRT(Y))^K", 1, INF), ("(Z×EXP(LN(Y^2-1)))^K", 0, INF),
              ("Z^K×SQRT(-1)", 0, INF)]
    cases11 = [gen_typed(rnd11) for _ in range(n11)] + held11
    for summand, lo, hi in cases11:
        out = run_sum(summand, lo, hi)
        if out in ("NO_ROOM", "TOO_LONG") or out.startswith("DECIMAL_FORM"):
            soft += 1
            continue
        why = judge_typed(summand, lo, hi, out)
        if why:
            failed += 1
            print(f"FAIL sum {summand} K {lo} {hi} RAD: {why}")
    # CAS 029: typed quotients, their own seeded batch, and the judge's planted answers first: a value moved, and the
    # answer of canon's cancelled X^K for X^(2×K)÷X^K (a value at X = 0, where its terms are 0÷0), must be refused
    if judge_quot_values("X^K÷Y^K", 1, "N", "-X÷(X-Y)+(X÷Y)^(N+1)÷(X÷Y-1); Y≠0; X÷Y≠1") or \
            not judge_quot_values("X^K÷Y^K", 1, "N", "-X÷(X-Y)+(X÷Y)^(N+1)÷(X÷Y-1)+1÷1000; Y≠0; X÷Y≠1") or \
            not judge_quot_values("X^(2×K)÷X^K", 1, "N", "-X÷(X-1)+X^(N+1)÷(X-1); X≠1") or \
            not judge_quot_values("X^K÷Y^K", 0, INF, "-1÷(X÷Y-1)+1÷1000; Y≠0; ABS(X÷Y)<1"):
        print("FAIL scripts/sum-values.py: its quotient judge accepts a planted answer or refuses the right one")
        sys.exit(1)
    rnd5, n5 = random.Random(20261024), 80
    held5 = [("X^K÷Y^K", 0, "N", False), ("X^K÷Y^K", 1, INF, False), ("1÷Y^K", 1, "N", False),
             ("X^(K-1)÷Y^(K+2)", 1, "N", False), ("K×X^K÷Y^K", -1, INF, False), ("(X^K+1)÷Y^K", 0, "N", False),
             ("2^K÷(3×Y)^K", -2, "N", False), ("SIN(Z)^K÷Y^K", 1, "N", False), ("Y^K÷SIN(Z)^K", 0, INF, False),
             ("X^K÷Y^K+1", 1, "N", False), ("X^(K-1)÷Y^K-X^K", 0, INF, False),
             ("X^(2×K)÷X^K", 1, "N", True), ("X^K÷Y^(-K)", 0, "N", True), ("X^(-K)÷Y^(-K)", 1, "N", True)]
    cases5 = [gen_quot(rnd5) for _ in range(n5)] + [(s, lo, hi, "RAD", ("QUOT", r)) for s, lo, hi, r in held5]
    for summand, lo, hi, angle, truth in cases5:
        out = run_sum(summand, lo, hi, angle)
        why = judge_quot(summand, lo, hi, out, truth[1])
        if why:
            failed += 1
            print(f"FAIL sum {summand} K {lo} {hi} {angle}: {why}")
    rows, rfailed = check_examples()
    checks = n_cases + len(cases2) + len(cases3) + len(cases4) + len(cases6) + len(cases7) + len(cases8) + len(cases9) + \
        len(cases10) + len(cases5) + rows
    print(f"sum values: {n_cases} seeded sums ({', '.join(f'{k[4:]} {v}' for k, v in sorted(kinds.items()))}), "
          f"{n2} ratios in two or three letters (CAS 024; and {len(held2)} held), {n3} past another factor (CAS "
          f"025; and {len(held3)} held; {n_named} with a named row for nothing, {n_empty} with no value at L = 0 below "
          f"N = -1, {n_fn} with no row for a letter in a function) and {n4} with a function factor (CAS 026; and "
          f"{len(held4)} held; {n4named} with a named letter row, {n4miss} and {n4neg} with the missing values of the "
          f"24's shape and of #10068's, CAS 027), {n6} with CAS 030's factors in each angle mode (and {len(held6)} "
          f"held; {n6cap} where the row gives way at the cap, {n6miss} with the 24's shape named), {n7s} of the 12's "
          f"shape (CAS 031; and {len(held7)} held), {n8} with a letter's row another row may say (CAS 032; and "
          f"{len(held8)} held), {n9} with a divisor inside the ratio (CAS 033; and {len(held9)} held), {n10} with a "
          f"function with a value everywhere around another letter (CAS 034; and {len(held10)} held), {n5} typed quotients "
          f"(CAS 029; and {len(held5)} held) at {ARENA} B, {soft} NO_ROOM, TOO_LONG or DECIMAL_FORM; {rows} example "
          f"rows by value\nchecks: {checks} failed: {failed + rfailed}")
    sys.exit(1 if failed + rfailed else 0)


if __name__ == "__main__":
    main()
