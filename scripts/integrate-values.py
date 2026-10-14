#!/usr/bin/env python3
"""CAS 017's integrals by substitution and by parts, judged by a different method than casim's own check (make check;
work/017-substitution-and-parts.md). casim proves an answer symbolically (closes(), and the chain and product rules on
checked pieces); this judge reads the answer and the integrand as typed in Decimal at 90 digits and differentiates the
answer numerically, a central difference with h = 10^-25, at seeded points where the integrand has a value: F' must
equal f there to 40 digits, and F must have a value wherever f has one (CAS 006's guarantee, abacus #8059).

The integrands are seeded families built so that an antiderivative exists: substitution k×G(u)×u' and parts
(polynomials against EXP, SIN, COS, SINH and COSH of a linear argument, X^n×LN(aX+b), LN(X)^m, EXP(aX)×SIN or COS(bX)),
in RAD, DEG and GRAD. A refusal (NOT_INTEGRABLE, NO_ROOM, TOO_LONG) is allowed and counted; a wrong answer fails.
Integrands with no elementary antiderivative must refuse. The judge refuses its planted wrong answers first, and judges
the rows tests/golden/moved/cas-017.txt lists and work/017-examples.txt's answered rows too.
    scripts/integrate-values.py [N] [SEED]     (default 360 cases, seed 20261014)"""
import decimal
import os
import random
import re
import shlex
import subprocess
import sys
from decimal import Decimal as D
from fractions import Fraction

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "build", "casim")
decimal.getcontext().prec = 90
H = D(10) ** -25
TOL = D(10) ** -40


class Undefined(Exception):
    """no real value"""


def d_pi():
    def atan_inv(x):
        x2, t, s, k = x * x, D(1) / x, D(0), 0
        while True:
            term = t / (2 * k + 1)
            if abs(term) < D(10) ** -100:
                return s
            s += term if k % 2 == 0 else -term
            t /= x2
            k += 1
    return 4 * (4 * atan_inv(D(5)) - atan_inv(D(239)))


PI = d_pi()


def d_sin(x):
    x = x % (2 * PI)
    t, s, k = x, D(0), 1
    while abs(t) > D(10) ** -100:
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
    if x > D("0.4"):
        return 2 * d_atan(x / (1 + (1 + x * x).sqrt()))
    t, s, k = x, D(0), 0
    while abs(t) > D(10) ** -100:
        s += t / (2 * k + 1) if k % 2 == 0 else -t / (2 * k + 1)
        t *= x * x
        k += 1
    return s


def d_ln(x):
    if x <= 0:
        raise Undefined
    return x.ln()


def d_sqrt(x):
    if x < 0:
        raise Undefined
    return x.sqrt()


def d_asin(x):
    if abs(x) > 1:
        raise Undefined
    if abs(x) == 1:
        return x * PI / 2
    return d_atan(x / (1 - x * x).sqrt())


def d_pow(a, b):
    if b == b.to_integral_value():
        if a == 0 and b < 0:
            raise Undefined
        return a ** int(b)
    if a > 0:
        return (b * a.ln()).exp()
    if a == 0:
        if b > 0:
            return D(0)
        raise Undefined
    f = Fraction(str(b)).limit_denominator(1000)
    if f.denominator % 2 == 0:
        raise Undefined
    r = ((b * (-a).ln()).exp())
    return -r if f.numerator % 2 else r


def angle_in(mode):
    return {"RAD": D(1), "DEG": PI / 180, "GRAD": PI / 200}[mode]


def fns(mode):
    k = angle_in(mode)

    def tan(x):
        c = d_cos(x * k)
        if c == 0:
            raise Undefined
        return d_sin(x * k) / c

    def inv(x):
        if x == 0:
            raise Undefined
        return 1 / x

    def atanh(x):
        if abs(x) >= 1:
            raise Undefined
        return (d_ln((1 + x) / (1 - x))) / 2

    def acosh(x):
        if x < 1:
            raise Undefined
        return d_ln(x + (x * x - 1).sqrt())

    return {
        "EXP": lambda x: x.exp(), "LN": d_ln, "LOG": lambda x: d_ln(x) / D(10).ln(), "ALOG": lambda x: (x * D(10).ln()).exp(),
        "SQRT": d_sqrt, "SQ": lambda x: x * x, "INV": inv, "ABS": abs,
        "SGN": lambda x: D(0) if x == 0 else D(1) if x > 0 else D(-1),
        "SIN": lambda x: d_sin(x * k), "COS": lambda x: d_cos(x * k), "TAN": tan,
        "ASIN": lambda x: d_asin(x) / k, "ACOS": lambda x: (PI / 2 - d_asin(x)) / k, "ATAN": lambda x: d_atan(x) / k,
        "SINH": lambda x: (x.exp() - (-x).exp()) / 2, "COSH": lambda x: (x.exp() + (-x).exp()) / 2,
        "TANH": lambda x: (x.exp() - (-x).exp()) / (x.exp() + (-x).exp()),
        "ASINH": lambda x: d_ln(x + (x * x + 1).sqrt()), "ACOSH": acosh, "ATANH": atanh,
    }


TOK = re.compile(r"\d+(?:\.\d+)?(?:E-?\d+)?|\.\d+|[A-Z]+|π|[-+×÷^()]")


def value(text, env, mode):
    """text (casim's syntax: + - × ÷ ^ with ^ binding tightest, a minus starting a term, functions in capitals) in
    Decimal; Undefined where it has no real value"""
    toks = TOK.findall(text)
    if "".join(toks) != text.replace(" ", ""):
        raise ValueError(f"not readable: {text}")
    F = fns(mode)
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
            return -power()
        if t in F:
            take()
            v = expr()
            take()
            return F[t](v)
        if t[0].isdigit() or t[0] == ".":
            return D(t)
        if len(t) == 1 and t in env:
            return env[t]
        raise ValueError(f"not readable: {text} at {t}")

    def power():
        v = atom()
        if peek() == "^":
            take()
            e = power()
            v = d_pow(v, e)
        return v

    def term():
        v = power()
        while peek() in ("×", "÷"):
            if take() == "×":
                v = v * power()
            else:
                w = power()
                if w == 0:
                    raise Undefined
                v = v / w
        return v

    def expr():
        v = -term() if peek() == "-" and take() else term()
        while peek() in ("+", "-"):
            v = v + term() if take() == "+" else v - term()
        return v

    return expr()


def integrate(text, var="X", mode="RAD", arena=None):
    a = [CLI] + (["--arena", str(arena)] if arena else []) + ["integrate", text, var, mode]
    p = subprocess.run(a, stdout=subprocess.PIPE, text=True)
    return p.returncode, p.stdout.strip()


UNJUDGED = "unjudged"            # f has no real value at any point tried (a constant like ASIN(π)): counted, not passed
EXACT = [D(0), D(1), D(-1), D(2), D(-2)]
RANGES = [(D("0.15"), D("0.85")), (D("1.2"), D("2.8")), (D("-0.85"), D("-0.15")), (D("-2.8"), D("-1.2"))]


def judge(f, F, var, mode, rnd, lo=None, hi=None):
    """None when F' = f to 40 digits at up to two points in each range where f has a value (0.15 to 0.85, 1.2 to 2.8,
    and their negatives, or (lo, hi)), and F has a value wherever f does; else why. A sign that is wrong only for
    negative X (an odd power through a square root) is found there"""
    env = {chr(65 + j): D("1.3") + D(j) / 7 for j in range(26)}
    good = 0
    for x in (EXACT if lo is None else ()):       # 0 and the integers, where a jump (SGN) or a corner would hide
        env[var] = x
        try:
            fx = value(f, env, mode)
        except Undefined:
            continue
        side = {}
        for k, at in (("x", x), ("up", x + H), ("dn", x - H)):
            env[var] = at
            try:
                side[k] = value(F, env, mode)
            except Undefined:
                pass
        if "x" not in side:
            return f"F has no value at {var} = {x}, where f = {fx:.6}"
        if "up" in side and "dn" in side:          # inside the domain: central, and a jump shows
            d, tol = (side["up"] - side["dn"]) / (2 * H), TOL
        elif "up" in side or "dn" in side:         # an edge (SQRT(X) at 0, ASIN at 1): one-sided, the error near SQRT(H)
            d = (side["up"] - side["x"]) / H if "up" in side else (side["x"] - side["dn"]) / H
            tol = D(10) ** -9
        else:
            continue
        if abs(d - fx) > tol * max(D(1), abs(fx)):
            return f"F' = {d:.12} but f = {fx:.12} at {var} = {x}"
        good += 1
    for lo, hi in ([(lo, hi)] if lo is not None else RANGES):
        found = 0
        for _ in range(12):
            if found == 2:
                break
            x = lo + (hi - lo) * D(rnd.randint(1, 10 ** 6)) / D(10 ** 6)
            env[var] = x
            try:
                fx = value(f, env, mode)
            except Undefined:
                continue
            try:
                env[var] = x + H
                up = value(F, env, mode)
                env[var] = x - H
                dn = value(F, env, mode)
            except Undefined:
                return f"F has no value at {var} = {x}, where f = {fx:.6}"
            d = (up - dn) / (2 * H)
            if abs(d - fx) > TOL * max(D(1), abs(fx)):
                return f"F' = {d:.12} but f = {fx:.12} at {var} = {x:.8}"
            found += 1
        good += found
    return None if good else UNJUDGED


# ---- the families ----

SUBS = [("X^2+1", "2×X"), ("X^3-2", "3×X^2"), ("2×X^2+3", "4×X"), ("SIN(X)", "COS(X)"), ("COS(X)", "-SIN(X)"),
        ("EXP(X)", "EXP(X)"), ("LN(X)", "1÷X"), ("SQRT(X)", "1÷SQRT(X)"), ("ATAN(X)", "1÷(1+X^2)"),
        ("1+EXP(X)", "EXP(X)"), ("X^2+X+1", "(2×X+1)")]
GS = ["EXP(U)", "SIN(U)", "COS(U)", "U^2", "U^3", "U^5", "U^(-2)", "SQRT(U)", "U^1.5", "1÷U", "ATAN(U)", "LN(U)"]
FNS = ["EXP", "SIN", "COS", "SINH", "COSH"]


def poly(rnd, deg):
    t = []
    for k in range(deg, -1, -1):
        c = rnd.choice([1, 2, 3, -1, -2]) if k == deg else rnd.choice([0, 1, -1, 2, 3])
        if c:
            t.append(f"{c}×X^{k}" if k > 1 else f"{c}×X" if k == 1 else f"{c}")
    return "(" + "+".join(t).replace("+-", "-") + ")"


def gen_case(rnd):
    kind = rnd.choice(["sub", "sub", "sub", "poly", "poly", "lnpow", "ln", "cycle"])
    mode = rnd.choice(["RAD", "RAD", "RAD", "DEG", "GRAD"])
    k = rnd.choice(["", "3×", "0.5×", "-2×"])
    if kind == "sub":
        u, du = rnd.choice(SUBS)
        g = rnd.choice(GS).replace("U", "(" + u + ")")
        return kind, f"{k}{g}×{du}", mode
    if kind == "poly":
        a, b = rnd.choice([1, 2, 3, -1]), rnd.choice([0, 1, -2])
        arg = f"{a}×X" + (f"+{b}" if b > 0 else f"{b}" if b < 0 else "")
        return kind, f"{k}{poly(rnd, rnd.randint(1, 3))}×{rnd.choice(FNS)}({arg})", mode
    if kind == "lnpow":
        return kind, f"{k}LN(X)^{rnd.randint(1, 3)}", mode
    if kind == "ln":
        n, a, b = rnd.randint(0, 3), rnd.choice([1, 2]), rnd.choice([0, 1, 3])
        arg = f"{a}×X" + (f"+{b}" if b else "")
        return kind, (f"{k}X^{n}×LN({arg})" if n else f"{k}LN({arg})"), mode
    a, b = rnd.choice([1, 2, -1]), rnd.choice([1, 2, 3])
    return kind, f"{k}EXP({a}×X)×{rnd.choice(['SIN', 'COS'])}({b}×X)", mode


NONELEMENTARY = ["EXP(X^2)", "SIN(X)÷X", "EXP(X)÷X", "SQRT(SIN(X))", "SIN(X^2)", "X^X", "1÷LN(X)", "X^2×EXP(-X^2)",
                 "EXP(EXP(X))", "COS(X)÷X", "SQRT(1+X^3)", "LN(X)÷(1+X)"]


def planted():
    """wrong answers the judge must refuse: a sign, a factor of 2, a dropped term, a constant inside"""
    rnd = random.Random(7)
    return [judge("X×EXP(X)", "EXP(X)+X×EXP(X)", "X", "RAD", rnd),
            judge("X×SIN(X)", "2×(SIN(X)-X×COS(X))", "X", "RAD", rnd),
            judge("LN(X)", "X×LN(X)", "X", "RAD", rnd),
            judge("X×EXP(X^2)", "0.5×EXP(X^2+1)", "X", "RAD", rnd),
            judge("SIN(X)×COS(X)", "0.5×SIN(X)^2", "X", "DEG", rnd),
            judge("1÷X", "LN(X)", "X", "RAD", random.Random(1), lo=D("-0.9"), hi=D("-0.1")),
            judge("X^2", "SQRT(X^2)^3÷3", "X", "RAD", rnd),           # right only where X > 0
            judge("SGN(X)×EXP(X)", "EXP(X)×SGN(X)", "X", "RAD", rnd)]  # right but at 0, where F jumps (review)


def golden_rows():
    """the moved golden rows (tests/golden/moved/cas-017.txt): inputs, then the answer after"""
    path = os.path.join(ROOT, "tests", "golden", "moved", "cas-017.txt")
    if not os.path.exists(path):
        return []
    rows, cur = [], None
    for line in open(path, encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        if line.startswith("  after\t"):
            st, ans = (line.rstrip("\n").split("\t") + [""])[1:3]
            if cur and st == "OK":
                rows.append((cur, ans))
        elif not line.startswith(" "):
            cur = line.rstrip("\n").split("\t")
    return rows


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 360
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 20261014
    rnd = random.Random(seed)
    fails, checks = 0, 0
    for i, why in enumerate(planted()):
        checks += 1
        if why is None:
            fails += 1
            print(f"FAIL scripts/integrate-values.py: the judge accepted planted wrong answer {i}")
    counts, unjudged = {}, []
    for _ in range(n):
        kind, f, mode = gen_case(rnd)
        arena = 65536 if rnd.random() < 0.25 else None
        rc, out = integrate(f, "X", mode, arena)
        c = counts.setdefault(kind, [0, 0])
        c[1] += 1
        checks += 1
        if rc != 0 or out.split(" ")[0] in ("NOT_INTEGRABLE", "NO_ROOM", "TOO_LONG"):
            if out.split(" ")[0] not in ("NOT_INTEGRABLE", "NO_ROOM", "TOO_LONG"):
                fails += 1
                print(f"FAIL scripts/integrate-values.py: integrate {f} X {mode}: {out}")
            continue
        c[0] += 1
        why = judge(f, out, "X", mode, rnd)
        if why == UNJUDGED:
            unjudged.append(f)
            continue
        if why:
            fails += 1
            print(f"FAIL scripts/integrate-values.py: integrate {f} X {mode} = {out}: {why}")
    for f in NONELEMENTARY:
        for mode in ("RAD", "DEG"):
            checks += 1
            rc, out = integrate(f, "X", mode)
            if out.split(" ")[0] not in ("NOT_INTEGRABLE", "NO_ROOM", "TOO_LONG"):
                fails += 1
                print(f"FAIL scripts/integrate-values.py: integrate {f} X {mode} answered {out}, with no elementary answer")
    rows = golden_rows()
    for cur, ans in rows:
        checks += 1
        var, mode = chr(65 + int(cur[2])), ["DEG", "RAD", "GRAD"][int(cur[3])]
        why = judge(cur[7], ans, var, mode, rnd)
        if why == UNJUDGED:
            unjudged.append(cur[7])
            continue
        if why:
            fails += 1
            print(f"FAIL scripts/integrate-values.py: golden {cur[7]} {var} {mode} = {ans}: {why}")
    ex = 0
    for line in open(os.path.join(ROOT, "work", "017-examples.txt"), encoding="utf-8"):
        if not line.startswith("| `casim integrate"):
            continue
        a = shlex.split(line.split("`")[1])
        ans = line.split("`")[3]
        if ans.split(" ")[0] in ("NOT_INTEGRABLE", "NO_ROOM", "TOO_LONG", "UNDEFINED", "BAD_ARG"):
            continue
        checks += 1
        ex += 1
        why = judge(a[2], ans, a[3], a[4], rnd)
        if why == UNJUDGED:
            unjudged.append(a[2])
            continue
        if why:
            fails += 1
            print(f"FAIL scripts/integrate-values.py: work/017-examples.txt integrate {a[2]} = {ans}: {why}")
    answered = ", ".join(f"{k} {v[0]} of {v[1]}" for k, v in sorted(counts.items()))
    print(f"integrate-values: answered {answered}; {len(NONELEMENTARY) * 2} non-elementary refused; "
          f"{len(rows)} moved golden rows and {ex} examples judged ({len(unjudged)} with no real value anywhere, unjudged: "
          f"{'; '.join(unjudged) or 'none'}); the judge refused its {len(planted())} planted answers")
    print(f"checks: {checks} failed: {fails}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
