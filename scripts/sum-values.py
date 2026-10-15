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


def ft(c):
    """a Fraction as casim reads it inside a product: 3, (1÷2), (-3)"""
    if c.denominator == 1:
        return str(c.numerator) if c >= 0 else f"({c.numerator})"
    return f"({c.numerator}÷{c.denominator})"


def value(text, angle="RAD"):
    """text with no letter in Decimal, SIN and COS in the angle mode"""
    to_rad = {"RAD": D(1), "DEG": lv.PI / 180, "GRAD": lv.PI / 200}[angle]
    keep = dict(lv.FN)
    lv.FN["SIN"] = lambda x: lv.d_sin(x * to_rad)
    lv.FN["COS"] = lambda x: lv.d_cos(x * to_rad)
    try:
        return lv.dvalue(text)
    finally:
        lv.FN.clear()
        lv.FN.update(keep)


def term(summand, k, angle):
    """the summand at k, or None where it has no value"""
    try:
        return value(sub(summand, "K", k), angle)
    except (ZeroDivisionError, decimal.InvalidOperation, decimal.DivisionByZero):
        return None


def close(got, want):
    return abs(got - want) <= TOL * max(D(1), abs(want))


def judge_n(summand, lo, out, angle="RAD", points=26):
    """'' when out, an answer in N, is the running sum from lo at N = lo-1 .. lo+points-2"""
    if out in STATUS or out in (INF, NINF) or not out:
        return f"{out} for a closed form"
    run = D(0)
    for n in range(lo - 1, lo - 1 + points):
        if n >= lo:
            t = term(summand, n, angle)
            if t is None:
                return f"the truth has a pole at {n}"
            run += t
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


FAMILIES = [(gen_poly, 3), (gen_geom, 3), (gen_geom2, 1), (gen_tele, 3), (gen_open, 1), (gen_direct, 2), (gen_pole, 1)]


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
        if out in ("NO_ROOM", "TOO_LONG") and truth not in ("NO_CLOSED_FORM", "UNDEFINED"):
            soft += 1
            continue
        why = judge_n(summand, lo, out, angle) if truth == "N" else judge_value(out, truth, angle)
        if why:
            failed += 1
            print(f"FAIL sum {summand} K {lo} {hi} {angle}: {why}")
    rows, rfailed = check_examples()
    print(f"sum values: {n_cases} seeded sums ({', '.join(f'{k[4:]} {v}' for k, v in sorted(kinds.items()))}) at "
          f"{ARENA} B, {soft} NO_ROOM or TOO_LONG; {rows} example rows by value\n"
          f"checks: {n_cases + rows} failed: {failed + rfailed}")
    sys.exit(1 if failed + rfailed else 0)


if __name__ == "__main__":
    main()
