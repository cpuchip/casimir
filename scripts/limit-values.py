#!/usr/bin/env python3
"""CAS 015's limits judged against truths built by construction (make check; work/015-limits.md). The opposite method
to casim's L'Hôpital on derivatives: power series over Fractions (scripts/taylor-values.py's generator). A quotient
N÷D of two seeded series at 0 has its limit from their first nonzero terms: N's order i, D's order j, so i = j is
n_i÷d_j, i > j is 0, and i < j is ∞ with the sign of n_i÷d_j and of the side to the power j - i (two-sided, an odd
j - i has no limit). The same quotient is asked at a point a (X written as X-a), at ∞ (X as 1÷X: 0 from above) and at
-∞ (X as 1÷X again: 0 from below). Beside them, rationals at ±∞ by their leading terms, and three families with known
values: (1+p÷X)^(q×X) is EXP(p×q), SQRT(a^2×X^2+b×X+c)-a×X is b÷(2a) (and its negative), and X^k÷EXP(X), LN(X)÷X^k
(0) and EXP(X)÷X^k (∞), and the products X^k×EXP(-c×X) at ∞ and X^k×LN(X) at 0+ (0).
casim's answer is read back in Decimal at 60 digits and must equal the truth to 45, so a rounded answer fails; its
statuses must be the truth's, except UNDECIDED, which is allowed anywhere and counted (rule 5). The judge refuses its
planted answers first.
With --forced, the same cases go to CAS 021's series route alone (build/casim-ser2), the route's own oracle; the
examples' texts are the plain run's.
    scripts/limit-values.py [--forced] [N] [SEED]     (default 300 cases, seed 20261013)"""
import decimal
import importlib.util
import os
import random
import re
import subprocess
import sys
from decimal import Decimal as D
from fractions import Fraction as F

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FORCED = "--forced" in sys.argv
CLI = os.path.join(ROOT, "build", "casim-ser2" if FORCED else "casim")
INF, NINF, NONE = "∞", "-∞", "UNDEFINED"

_spec = importlib.util.spec_from_file_location("tv", os.path.join(ROOT, "scripts", "taylor-values.py"))
tv = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(tv)

decimal.getcontext().prec = 60


# ---- Decimal functions (the standard library has exp, ln and sqrt) ----

def d_pi():
    """π by Machin's formula"""
    def atan_inv(x):
        x2, t, s, k = x * x, D(1) / x, D(0), 0
        while True:
            term = t / (2 * k + 1)
            if abs(term) < D(10) ** -70:
                return s
            s += term if k % 2 == 0 else -term
            t /= x2
            k += 1
    return 4 * (4 * atan_inv(D(5)) - atan_inv(D(239)))


PI = d_pi()


def d_sin(x):
    x = x % (2 * PI)
    t, s, k = x, D(0), 1
    while abs(t) > D(10) ** -70:
        s += t
        t = -t * x * x / ((k + 1) * (k + 2))
        k += 2
    return s


def d_cos(x):
    return d_sin(x + PI / 2)


def d_atan(x):
    if x < 0:
        return -d_atan(-x)
    if x > 1:
        return PI / 2 - d_atan(1 / x)
    if x > D("0.4"):                       # atan x = 2 atan(x ÷ (1 + sqrt(1 + x^2)))
        return 2 * d_atan(x / (1 + (1 + x * x).sqrt()))
    t, s, k = x, D(0), 0
    while abs(t) > D(10) ** -70:
        s += t / (2 * k + 1) if k % 2 == 0 else -t / (2 * k + 1)
        t *= x * x
        k += 1
    return s


FN = {"EXP": lambda x: x.exp(), "LN": lambda x: x.ln(), "SQRT": lambda x: x.sqrt(), "SIN": d_sin, "COS": d_cos,
      "TAN": lambda x: d_sin(x) / d_cos(x), "ATAN": d_atan}
TOK = re.compile(r"\d+\.\d+|\d+|[A-Z]+|π|[-+×÷^()]")


def dvalue(text):
    """an answer with no variable (numbers, π, EXP, LN, SQRT, SIN, COS, TAN, ATAN, + - × ÷ ^) in Decimal"""
    toks = TOK.findall(text)
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
            return PI
        if t == "-":
            return -factor()
        if t in FN:
            take()
            v = expr()
            take()
            return FN[t](v)
        if t[0].isdigit():
            return D(t)
        raise ValueError(f"not readable: {text} at {t}")

    def factor():
        v = atom()
        if peek() == "^":
            take()
            e = factor()
            v = v ** int(e) if e == int(e) else (e * v.ln()).exp()
        return v

    def term():
        v = factor()
        while peek() in ("×", "÷"):
            v = v * factor() if take() == "×" else v / factor()
        return v

    def expr():
        v = term() if peek() != "-" else (take(), -term())[1]
        while peek() in ("+", "-"):
            v = v + term() if take() == "+" else v - term()
        return v

    return expr()


# ---- the truths ----

def first(s):
    """the order of a series' first nonzero term, or None"""
    return next((k for k, c in enumerate(s) if c != 0), None)


def quot_truth(sn, sd, side):
    """lim N÷D at 0 from side (1, -1, 0 for both) from the series; a Fraction, INF, NINF or NONE; None if unknown"""
    i, j = first(sn), first(sd)
    if j is None:
        return None
    if i is None:
        return None if len(sn) - 1 < j else F(0)       # N is 0 to the series' order: known only when that passes j
    if i >= j:
        return sn[i] / sd[j] if i == j else F(0)
    r = sn[i] / sd[j]
    if side == 0:
        if (j - i) % 2:
            return NONE
        return INF if r > 0 else NINF
    s = 1 if r > 0 else -1
    if side < 0 and (j - i) % 2:
        s = -s
    return INF if s > 0 else NINF


def sub_x(text, by):
    """X written as by, in a generated text (not the X of EXP)"""
    return re.sub(r"(?<![A-Z])X(?![A-Z])", lambda m: by, text)


def ftext(c):
    """a Fraction as casim reads it: 3, or (1÷2)"""
    return str(c.numerator) if c.denominator == 1 else f"({c.numerator}÷{c.denominator})"


def gen_case(rnd):
    """(text, point, side, truth): truth a Fraction, a Decimal, INF, NINF or NONE"""
    r = rnd.random()
    n = 8
    if r < 0.55:
        a, sa = tv.gen(rnd, n)
        b, sb = tv.gen(rnd, n)
        if rnd.random() < 0.5 and sa[0] != 0:          # the constant taken off: 0÷0 more often
            c = sa[0]
            a, sa = (f"({a}-{ftext(c)})" if c > 0 else f"({a}+{ftext(-c)})"), [F(0)] + sa[1:]
        if rnd.random() < 0.6 and sb[0] != 0:
            c = sb[0]
            b, sb = (f"({b}-{ftext(c)})" if c > 0 else f"({b}+{ftext(-c)})"), [F(0)] + sb[1:]
        text = f"({a})÷({b})"
        where = rnd.choice(["0", "0", "a", "inf", "ninf"])
        side = rnd.choice([0, 0, 1, -1])
        if where == "0":
            return text, "0", side, quot_truth(sa, sb, side)
        if where == "a":
            p = rnd.choice([1, 2, -1, -3])
            return sub_x(text, f"(X-{p})" if p > 0 else f"(X+{-p})"), str(p), side, quot_truth(sa, sb, side)
        if where == "inf":
            return sub_x(text, "(1÷X)"), "∞", 0, quot_truth(sa, sb, 1)
        return sub_x(text, "(1÷X)"), "-∞", 0, quot_truth(sa, sb, -1)
    if r < 0.75:
        dn, dd = rnd.randint(0, 4), rnd.randint(0, 4)
        cn = [rnd.randint(-5, 5) for _ in range(dn)] + [rnd.choice([1, 2, 3, -1, -2])]
        cd = [rnd.randint(-5, 5) for _ in range(dd)] + [rnd.choice([1, 2, 3, -1, -2])]

        def poly(cs):
            t = "".join(f"{'+' if c > 0 else '-'}{abs(c)}×X^{k}" for k, c in enumerate(cs) if c != 0)
            return t.lstrip("+") or "0"
        neg = rnd.random() < 0.4
        lead = F(cn[-1], cd[-1])
        if dn < dd:
            truth = F(0)
        elif dn == dd:
            truth = lead
        else:
            s = 1 if lead > 0 else -1
            if neg and (dn - dd) % 2:
                s = -s
            truth = INF if s > 0 else NINF
        return f"({poly(cn)})÷({poly(cd)})", "-∞" if neg else "∞", 0, truth
    if r < 0.85:
        p, q = rnd.choice([1, 2, 3, -1, -2]), rnd.choice([1, 2, 3])
        return f"(1+{p}÷X)^({q}×X)" if p > 0 else f"(1-{-p}÷X)^({q}×X)", "∞", 0, D(p * q).exp()
    if r < 0.93:
        b, c = rnd.choice([1, 2, 3, -2, 4]), rnd.choice([0, 1, 5, -3])
        a = rnd.choice([1, 1, 2, 3])
        sb, sc = f"+{b}" if b > 0 else f"-{-b}", "" if c == 0 else f"+{c}" if c > 0 else f"-{-c}"
        root, ax = f"SQRT({a * a}×X^2{sb}×X{sc})", "X" if a == 1 else f"{a}×X"
        if rnd.random() < 0.5:                         # √(a²X²+bX+c) - aX → b÷(2a), and the other way round
            return f"{root}-{ax}", "∞", 0, F(b, 2 * a)
        return f"{ax}-{root}", "∞", 0, F(-b, 2 * a)
    k, c = rnd.randint(1, 4), rnd.randint(1, 3)
    return rnd.choice([(f"X^{k}÷EXP(X)", "∞", 0, F(0)), (f"LN(X)÷X^{k}", "∞", 0, F(0)),
                       (f"EXP(X)÷X^{k}", "∞", 0, INF),
                       (f"X^{k}×EXP(-{c}×X)", "∞", 0, F(0)), (f"X^{k}×LN(X)", "0", 1, F(0))])     # 0×∞ both ways


# ---- the judge ----

def wrong(out, truth):
    """'' when out is the truth; UNDECIDED is the caller's"""
    if truth in (INF, NINF, NONE):
        return "" if out == truth else f"{out} for {truth}"
    if out in (INF, NINF, NONE) or not out:
        return f"{out} for {truth}"
    try:
        got = dvalue(out)
    except (ValueError, ZeroDivisionError, decimal.InvalidOperation) as e:
        return str(e)
    want = D(truth.numerator) / D(truth.denominator) if isinstance(truth, F) else truth
    if abs(got - want) > D(10) ** -45 * max(D(1), abs(want)):
        return f"{out} = {got:.20} for {want:.20}"
    return ""


def limit(text, point, side, angle="RAD", model="35"):
    p = subprocess.run([CLI, "--model", model, "limit", text, "X", point, side, angle], stdout=subprocess.PIPE,
                       text=True, encoding="utf-8")
    return p.stdout.strip()


def check_examples():
    """work/015-examples.txt: every row as the build prints it. Returns (rows, failures)"""
    rows = failed = 0
    path = os.path.join(ROOT, "work", "015-examples.txt")
    for line in open(path, encoding="utf-8"):
        if not line.startswith("L"):
            continue
        f = [x.strip() for x in line.split("|")]
        rows += 1
        side = "" if f[5] == "both" else f[5]
        got = limit(f[2], f[4], side, f[6], f[7])
        if got != f[8]:
            failed += 1
            print(f"FAIL work/015-examples.txt {f[0]}: casim {got}, the example {f[8]}")
    return rows, failed


def main():
    planted = [("1", F(1, 2)), ("0.33333333333333333333", F(1, 3)), ("∞", NINF), ("UNDEFINED", INF),
               ("2.718281828459045235360287", D(1).exp()), ("EXP(2)", D(3).exp()), ("-1÷6", F(1, 6)), ("0", NONE)]
    for out, truth in planted:
        if not wrong(out, truth):
            print(f"FAIL scripts/limit-values.py: its judge accepts {out}")
            sys.exit(1)
    for out, truth in [("EXP(3)", D(3).exp()), ("-1÷6", F(-1, 6)), ("π÷2", PI / 2), ("0.5", F(1, 2))]:
        if wrong(out, truth):                          # and accepts the right ones
            print(f"FAIL scripts/limit-values.py: its judge refuses {out}")
            sys.exit(1)
    args = [a for a in sys.argv[1:] if a != "--forced"]
    n_cases = int(args[0]) if args else 300
    rnd = random.Random(int(args[1]) if len(args) > 1 else 20261013)
    judged = failed = undecided = soft = unknown = 0
    kinds = {}
    for _ in range(n_cases):
        text, point, side, truth = gen_case(rnd)
        if truth is None:
            unknown += 1
            continue
        s = "" if side == 0 else "+" if side > 0 else "-"
        out = limit(text, point, s)
        if out == "UNDECIDED":
            undecided += 1
            continue
        if out in ("NO_ROOM", "TOO_LONG"):
            soft += 1
            continue
        judged += 1
        k = truth if truth in (INF, NINF, NONE) else "finite"
        kinds[k] = kinds.get(k, 0) + 1
        w = wrong(out, truth)
        if w:
            failed += 1
            print(f"FAIL scripts/limit-values.py: limit {text} X {point} '{s}': {out}\n  {w}")
    rows, efailed = check_examples() if not FORCED else (0, 0)
    tally = ", ".join(f"{v} {k}" for k, v in sorted(kinds.items()))
    print(f"limit values{' (the series route alone)' if FORCED else ''}: "
          f"{judged} of {n_cases} seeded cases judged ({tally}); "
          f"{undecided} UNDECIDED, {soft} NO_ROOM or TOO_LONG, {unknown} past the series' order; the judge refused its "
          f"{len(planted)} planted answers; work/015-examples.txt: {rows} rows as printed\n"
          f"checks: {judged + rows} failed: {failed + efailed}")
    sys.exit(1 if failed or efailed or (rows == 0 and not FORCED) else 0)


if __name__ == "__main__":
    main()
