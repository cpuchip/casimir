#!/usr/bin/env python3
"""CAS 014's Taylor polynomials judged exactly (make check; work/014-taylor.md). Seeded expressions at 0, built so every
coefficient is rational: X, small integers, +, -, ×, ÷, integer powers, EXP, SIN and COS of u with u(0) = 0, and LN of u
with u(0) = 1. The truth is exact power-series arithmetic over Fractions (truncated products, quotients and the series
of each function), the method opposite casim's repeated derivatives. casim's answer is read back exactly at n + 1
rational points, so equal values there are equal polynomials. Also checked: rule 6's spelling (ascending powers, no zero
term, no coefficient written as 1), and the judge refuses its planted faults.
    scripts/taylor-values.py [N] [SEED]     (default 300 expressions, seed 20261013)"""
import os
import random
import re
import subprocess
import sys
from fractions import Fraction as F

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "build", "casim")


# ---- truncated power series over Fractions, to order N ----

def s_const(c, n):
    return [F(c)] + [F(0)] * n


def s_x(n):
    return [F(0), F(1)] + [F(0)] * (n - 1) if n >= 1 else [F(0)]


def s_add(a, b):
    return [x + y for x, y in zip(a, b)]


def s_mul(a, b):
    n = len(a) - 1
    return [sum(a[i] * b[k - i] for i in range(k + 1)) for k in range(n + 1)]


def s_div(a, b):
    """a ÷ b, b(0) != 0"""
    n = len(a) - 1
    q = []
    for k in range(n + 1):
        q.append((a[k] - sum(q[i] * b[k - i] for i in range(k))) / b[0])
    return q


def s_compose_unit(coef, u):
    """Σ coef[j] u^j, u(0) = 0"""
    n = len(u) - 1
    out, p = s_const(coef[0], n), s_const(1, n)
    for j in range(1, n + 1):
        p = s_mul(p, u)
        out = s_add(out, [coef[j] * x for x in p])
    return out


def fact(k):
    r = 1
    for i in range(2, k + 1):
        r *= i
    return r


def s_exp(u):
    n = len(u) - 1
    return s_compose_unit([F(1, fact(j)) for j in range(n + 1)], u)


def s_sin(u):
    n = len(u) - 1
    return s_compose_unit([F(0) if j % 2 == 0 else F((-1) ** (j // 2), fact(j)) for j in range(n + 1)], u)


def s_cos(u):
    n = len(u) - 1
    return s_compose_unit([F((-1) ** (j // 2), fact(j)) if j % 2 == 0 else F(0) for j in range(n + 1)], u)


def s_ln(u):
    """LN(u), u(0) = 1: LN(1 + w) = Σ (-1)^(j+1) w^j ÷ j"""
    n = len(u) - 1
    w = s_add(u, s_const(-1, n))
    return s_compose_unit([F(0)] + [F((-1) ** (j + 1), j) for j in range(1, n + 1)], w)


# ---- seeded expressions: (casim text, series) ----

def gen(rnd, n, depth=0):
    """an expression whose series at 0 exists, with its text"""
    r = rnd.random()
    if depth >= 2 or r < 0.25:
        k = rnd.choice([1, 2, 3, -1, -2])
        m = rnd.choice([1, 1, 2, 3])
        t = "X" if m == 1 else f"X^{m}"
        s = s_x(n)
        for _ in range(m - 1):
            s = s_mul(s, s_x(n))
        if k != 1:
            t = f"{k}×{t}" if k > 0 else f"({k})×{t}"
            s = [k * x for x in s]
        return t, s
    if r < 0.45:
        a, sa = gen(rnd, n, depth + 1)
        b, sb = gen(rnd, n, depth + 1)
        op = rnd.choice(["+", "-", "×"])
        if op == "+":
            return f"({a}+{b})", s_add(sa, sb)
        if op == "-":
            return f"({a}-{b})", s_add(sa, [-x for x in sb])
        return f"({a})×({b})", s_mul(sa, sb)
    if r < 0.55:
        a, sa = gen(rnd, n, depth + 1)
        c = rnd.choice([1, 2, 3])
        return f"({a})÷({c}-X)", s_div(sa, s_add(s_const(c, n), [-x for x in s_x(n)]))
    a, sa = gen(rnd, n, depth + 1)
    if sa[0] != 0:                          # a function's argument must vanish at 0: its constant taken off
        c = sa[0]
        a = f"({a}-{c})" if c > 0 else f"({a}+{-c})"
        sa = [F(0)] + sa[1:]
    f = rnd.choice(["EXP", "SIN", "COS", "LN"])
    if f == "EXP":
        return f"EXP({a})", s_exp(sa)
    if f == "SIN":
        return f"SIN({a})", s_sin(sa)
    if f == "COS":
        return f"COS({a})", s_cos(sa)
    return f"LN(1+{a})", s_ln(s_add(s_const(1, n), sa))


# ---- casim's answer read back exactly ----

TOK = re.compile(r"\d+\.\d+|\d+|X|[-+×÷^()]")


def value(text, x):
    """casim's answer (numbers, X, + - × ÷ ^, brackets) at a rational x, exactly"""
    toks = TOK.findall(text)
    if "".join(toks) != text:
        raise ValueError(f"not a rational polynomial: {text}")
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
        if t == "X":
            return x
        if t == "-":
            return -factor()
        return F(t)

    def factor():
        v = atom()
        while peek() == "^":
            take()
            v = v ** int(take())
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


def spelling_wrong(text, a="0"):
    """rule 6: ascending powers of X, no zero term, no coefficient written as 1"""
    if text == "0":
        return ""
    if re.search(r"(^|[-+])1×", text) or re.search(r"(^|[-+])0×", text):
        return "a coefficient written as 1 or 0"
    terms = re.findall(r"(?:^-?|[-+])[^-+]*(?:\([^()]*\)[^-+]*)*", text)      # a leading minus is the first term's
    powers = []
    for t in terms:
        m = re.search(r"X\^(\d+)", t)
        powers.append(int(m.group(1)) if m else (1 if "X" in t else 0))
    if powers != sorted(powers) or len(set(powers)) != len(powers):
        return f"powers not ascending: {powers}"
    return ""


def taylor(text, n, point="0", angle="RAD", arena=8192):
    p = subprocess.run([CLI, "--arena", str(arena), "taylor", text, "X", point, str(n), angle], stdout=subprocess.PIPE,
                       text=True, encoding="utf-8")
    return p.returncode, p.stdout.strip()


def wrong(out, truth):
    """'' when out is the polynomial whose coefficients are truth"""
    n = len(truth) - 1
    for j in range(n + 1):
        x = F(2 * j + 1, 7)
        want = sum(c * x ** k for k, c in enumerate(truth))
        try:
            got = value(out, x)
        except (ValueError, IndexError, ZeroDivisionError) as e:
            return str(e)
        if got != want:
            return f"at X={x}: {got} for {want}"
    return spelling_wrong(out)


def binom(r, k):
    """r choose k for a Fraction r"""
    out = F(1)
    for i in range(k):
        out = out * (r - i) / (i + 1)
    return out


def example_truths(n):
    """the exact series at 0 of work/014-examples.txt's rows with rational coefficients, by hand: id -> series"""
    x = s_x(n)
    return {
        "SIN(X)": s_sin(x), "COS(X)": s_cos(x), "EXP(X)": s_exp(x), "LN(1+X)": s_ln(s_add(s_const(1, n), x)),
        "ATAN(X)": [F(0) if k % 2 == 0 else F((-1) ** (k // 2), k) for k in range(n + 1)],
        "TAN(X)": s_div(s_sin(x), s_cos(x)),
        "SQRT(1+X)": [binom(F(1, 2), k) for k in range(n + 1)],
        "EXP(X)×SIN(X)": s_mul(s_exp(x), s_sin(x)),
        "X^2+X": s_add(s_mul(x, x), x),
    }


def check_examples():
    """work/014-examples.txt: every row as the build prints it; the rows at 0 in RAD with rational coefficients also
    exactly against their series by hand. Returns (rows, exact, failures)"""
    rows = exact = failed = 0
    for line in open(os.path.join(ROOT, "work", "014-examples.txt"), encoding="utf-8"):
        if not line.startswith("T"):
            continue
        f = [x.strip() for x in line.split("|")]
        rows += 1
        p = subprocess.run([CLI, "--model", f[7], "taylor", f[2], f[3], f[4], f[5], f[6]], stdout=subprocess.PIPE,
                           text=True, encoding="utf-8")
        got = p.stdout.strip()
        if got != f[8]:
            failed += 1
            print(f"FAIL work/014-examples.txt {f[0]}: casim {got}, the example {f[8]}")
            continue
        truths = example_truths(int(f[5])) if f[5].isdigit() and int(f[5]) <= 12 else {}
        if f[4] == "0" and f[6] == "RAD" and f[2] in truths:
            exact += 1
            w = wrong(got, truths[f[2]])
            if w:
                failed += 1
                print(f"FAIL work/014-examples.txt {f[0]}: {got}: {w}")
    return rows, exact, failed


def check_corners():
    """CAS 016 rule 2 by its truth: X^j×ABS(X)^m at 0 is SGN(X)^m×X^(j+m). For m even that is X^(j+m), with a series at
    every order; for m odd its derivatives are 0 at 0 up to order j+m-1 and order j+m has none. An answer must be that
    series, UNDEFINED only where it is true, and UNDECIDED is never wrong. Returns (cases, answered, failures)"""
    cases = answered = failed = 0
    for j in range(4):
        for m in range(1, 4):
            text = f"ABS(X)^{m}" if j == 0 else f"X^{j}×ABS(X)^{m}"
            if m == 1:
                text = "ABS(X)" if j == 0 else f"X^{j}×ABS(X)" if j > 1 else "X×ABS(X)"
            for n in range(0, 7):
                cases += 1
                _, out = taylor(text, n)
                d = j + m
                exists = m % 2 == 0 or n < d
                want = "0" if n < d else f"X^{d}" if d > 1 else "X"
                if out == "UNDECIDED" or (out == "UNDEFINED" and not exists):
                    continue
                if out == want and exists:
                    answered += 1
                    continue
                failed += 1
                print(f"FAIL scripts/taylor-values.py: taylor {text} X 0 {n}: {out}, the truth "
                      f"{want if exists else 'UNDEFINED'}")
    return cases, answered, failed


def main():
    # the judge must refuse these
    planted = [("X+X^3÷6", [F(0), F(1), F(0), F(-1, 6)]),            # a sign
               ("X-X^3÷3", [F(0), F(1), F(0), F(-1, 6)]),            # k! as (k-1)!
               ("X+0×X^2-X^3÷6", [F(0), F(1), F(0), F(-1, 6)]),      # a zero term printed
               ("1×X-X^3÷6", [F(0), F(1), F(0), F(-1, 6)]),          # a coefficient of 1 printed
               ("X", [F(0), F(1), F(0), F(-1, 6)]),                   # one order short
               ("-X^3÷6+X", [F(0), F(1), F(0), F(-1, 6)]),           # descending
               ("-1-1-X", [F(-2), F(-1)])]                           # a power twice (two constants)
    for out, truth in planted:
        if not wrong(out, truth):
            print(f"FAIL scripts/taylor-values.py: its judge accepts {out}")
            sys.exit(1)
    n_cases = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    rnd = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 20261013)
    judged = failed = soft = 0
    cases = []
    for _ in range(n_cases):
        n = rnd.randint(1, 10)
        text, s = gen(rnd, n)
        cases.append((text, s, n))
    # each at the calculator's 8,192 B arena (384 B text buffers) and at 65,536 B (1,024 B ones, CAS 016 rule 4)
    big = {"judged": 0, "soft": 0}
    for (text, s, n), arena in [(c, a) for a in (8192, 65536) for c in cases]:
        rc, out = taylor(text, n, arena=arena)
        if rc != 0:
            if out in ("NO_ROOM", "TOO_LONG"):
                soft += arena == 8192
                big["soft"] += arena != 8192
                continue
            failed += 1
            print(f"FAIL scripts/taylor-values.py: taylor {text} X 0 {n} (arena {arena}): {out}")
            continue
        judged += arena == 8192
        big["judged"] += arena != 8192
        w = wrong(out, s)
        if w:
            failed += 1
            print(f"FAIL scripts/taylor-values.py: taylor {text} X 0 {n} (arena {arena}): {out}\n  {w}")
    rows, exact, efailed = check_examples()
    ccases, canswered, cfailed = check_corners()
    efailed += cfailed
    print(f"taylor values: {judged} of {n_cases} seeded expressions judged exactly at 0 ({soft} NO_ROOM or TOO_LONG), "
          f"and {big['judged']} at 65,536 B with 1,024 B buffers ({big['soft']}); "
          f"the judge refused its {len(planted)} planted answers; work/014-examples.txt: {rows} rows as printed, "
          f"{exact} of them exactly by their series; ABS's corners: {ccases} cases of X^j×ABS(X)^m, {canswered} "
          f"answered exactly, the rest UNDEFINED where true or UNDECIDED\nchecks: {judged + big['judged'] + rows + ccases} failed: {failed + efailed}")
    sys.exit(1 if failed or efailed or rows == 0 else 0)


if __name__ == "__main__":
    main()
