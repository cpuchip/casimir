#!/usr/bin/env python3
"""solve's steps judged by value (make check; work/steps-solve.md, oracle 2 to 4). Every solve the golden corpus and
docs/language.md hold, and seeded random polynomials built from known factors, go through `build/casim --tsv steps
solve`, both ways. A step's polynomials are evaluated exactly here (fractions, and a root's a+b·SQRT(r) in Q(SQRT(r))),
by a reader that shares no code with casim; what was typed is evaluated by casim itself (subst at a point), since its
reading of the typed text is the one that counts:
  - each depth-0 line's left side is the typed L-R times one constant, the same at every point;
  - INTEGER COEFFICIENTS' template equals its result, a division template is exact and divides what the line before
    left, a discriminant line's arithmetic is b^2-4ac of its factor and is negative;
  - every root on any line makes its factor and the whole polynomial zero, exactly; a quadratic's template roots are
    its result's;
  - the factors at depth 1 are the last product line's, in the same order, with the same multiplicities; each factor
    line adds one factor; the order is the root 0, the linear factors ascending by root, the quadratics ascending by
    (a, b, c), then a factor left;
  - each side of every line, read back by casim (subst at a point), has this reader's value (abacus #6288, D2);
  - the last step is plain solve's answer byte for byte, k runs 0..count-1, the one-line steps are the two-line
    steps' results, a second run is identical, and a refusal is plain solve's status.
    scripts/ssteps-values.py [N] [SEED]     (default 300 random equations, seed 20261010)"""
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
spec = importlib.util.spec_from_file_location("stepvalues", os.path.join(ROOT, "scripts", "step-values.py"))
sv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sv)
POINTS = [F(3, 7), F(-5, 11), F(13, 9), F(17, 5), F(-29, 13)]
ANGLE = {"0": "DEG", "1": "RAD", "2": "GRAD"}
MODEL = {"35": "35", "33": "33", "1": "STU"}


class QR:
    """a + b·SQRT(r), a and b fractions, r a square-free integer > 1 (or b = 0)"""

    def __init__(self, a, b=F(0), r=1):
        self.a, self.b, self.r = F(a), F(b), r if b else 1

    @staticmethod
    def lift(x):
        return x if isinstance(x, QR) else QR(x)

    def _r(self, o):
        if self.r != 1 and o.r != 1 and self.r != o.r:
            raise ValueError("two different square roots")
        return self.r if self.r != 1 else o.r

    def __add__(self, o):
        o = QR.lift(o)
        return QR(self.a + o.a, self.b + o.b, self._r(o))

    def __neg__(self):
        return QR(-self.a, -self.b, self.r)

    def __sub__(self, o):
        return self + -QR.lift(o)

    def __mul__(self, o):
        o = QR.lift(o)
        r = self._r(o)
        return QR(self.a * o.a + self.b * o.b * r, self.a * o.b + self.b * o.a, r)

    def inv(self):
        n = self.a * self.a - self.b * self.b * self.r
        if n == 0:
            raise ZeroDivisionError
        return QR(self.a / n, -self.b / n, self.r)

    def __truediv__(self, o):
        return self * QR.lift(o).inv()

    def __pow__(self, n):
        out, base = QR(1), self if n >= 0 else self.inv()
        for _ in range(abs(n)):
            out = out * base
        return out

    def zero(self):
        return self.a == 0 and self.b == 0

    def __eq__(self, o):
        return (self - QR.lift(o)).zero()

    def float(self):
        return float(self.a) + float(self.b) * math.sqrt(self.r)


def sqrt_of(n):
    """SQRT(n) for a whole n >= 0 as a QR: k·SQRT(m), m square-free"""
    if n < 0:
        raise ValueError("SQRT of a negative")
    k, m, p = 1, n, 2
    while p * p <= m:
        while m % (p * p) == 0:
            m //= p * p
            k *= p
        p += 1
    return QR(k) if m == 1 else QR(0, k, m)


class Reader:
    """a step's text: whole numbers, X, + - × ÷ ^ (whole exponents), parentheses, SQRT(whole); the usual precedence
    (^ above unary minus: -X^2 is -(X^2)), left to right"""

    def __init__(self, text, x):
        self.t, self.i, self.x = text, 0, x

    def peek(self):
        return self.t[self.i] if self.i < len(self.t) else ""

    def eat(self, s):
        if self.t.startswith(s, self.i):
            self.i += len(s)
            return True
        return False

    def read(self):
        v = self.sum()
        if self.i != len(self.t):
            raise ValueError(f"unread: {self.t[self.i:]!r} in {self.t!r}")
        return v

    def sum(self):
        v = self.term()
        while True:
            if self.eat("+"):
                v = v + self.term()
            elif self.eat("-"):
                v = v - self.term()
            else:
                return v

    def term(self):
        v = self.unary()
        while True:
            if self.eat("×"):
                v = v * self.unary()
            elif self.eat("÷"):
                v = v / self.unary()
            else:
                return v

    def unary(self):
        if self.eat("-"):
            return -self.unary()
        return self.power()

    def power(self):
        v = self.atom()
        if self.eat("^"):
            neg = self.eat("-")
            m = re.match(r"\d+", self.t[self.i:])
            if not m:
                raise ValueError(f"an exponent in {self.t!r}")
            self.i += m.end()
            v = v ** (-int(m.group()) if neg else int(m.group()))
        return v

    def atom(self):
        if self.eat("("):
            v = self.sum()
            if not self.eat(")"):
                raise ValueError(f"a parenthesis in {self.t!r}")
            return v
        if self.eat("SQRT("):
            m = re.match(r"-?\d+", self.t[self.i:])
            if not m:
                raise ValueError(f"SQRT in {self.t!r}")
            self.i += m.end()
            if not self.eat(")"):
                raise ValueError(f"SQRT in {self.t!r}")
            return sqrt_of(int(m.group()))
        if self.eat("X"):
            return QR.lift(self.x)
        m = re.match(r"\d+", self.t[self.i:])
        if not m:
            raise ValueError(f"at {self.i} in {self.t!r}")
        self.i += m.end()
        return QR(int(m.group()))


def ev(text, x):
    return Reader(text, x).read()


def top_split(text, sep):
    """text split at sep outside parentheses"""
    out, depth, start, i = [], 0, 0, 0
    while i < len(text):
        c = text[i]
        depth += c == "("
        depth -= c == ")"
        if depth == 0 and text.startswith(sep, i):
            out.append(text[start:i])
            i += len(sep)
            start = i
            continue
        i += 1
    out.append(text[start:])
    return out


def closing(text):
    """the index of the parenthesis closing text's first"""
    depth = 0
    for i, c in enumerate(text):
        depth += (c == "(") - (c == ")")
        if depth == 0:
            return i
    raise ValueError(f"unclosed: {text!r}")


def factors_of(lhs):
    """a product line's left side as [(factor text, exponent)], each factor unparenthesized"""
    out = []
    for f in top_split(lhs, "×"):
        m = re.fullmatch(r"\((.*)\)\^(\d+)", f) or re.fullmatch(r"\((.*)\)()", f) or re.fullmatch(r"(X)\^(\d+)", f) \
            or re.fullmatch(r"(X)()", f)
        if not m:
            return None
        out.append((m.group(1), int(m.group(2) or 1)))
    return out


def coeffs(poly, deg=12):
    """a polynomial text's coefficients, exactly (by evaluation at 0..deg and solving: Lagrange through deg+1 points)"""
    xs = list(range(deg + 1))
    ys = [ev(poly, F(x)).a for x in xs]
    # Newton's divided differences, then expand
    d = ys[:]
    for j in range(1, len(xs)):
        for i in range(len(xs) - 1, j - 1, -1):
            d[i] = (d[i] - d[i - 1]) / (xs[i] - xs[i - j])
    c = [F(0)] * (deg + 1)
    basis = [F(1)]
    for j in range(len(xs)):
        for k, b in enumerate(basis):
            c[k] += d[j] * b
        nb = [F(0)] * (len(basis) + 1)
        for k, b in enumerate(basis):
            nb[k + 1] += b
            nb[k] -= xs[j] * b
        basis = nb
    while len(c) > 1 and c[-1] == 0:
        c.pop()
    return c


def casim_value(text, x, var="X", model="35"):
    """text at var = x, as casim reads it: subst, then the result's value (None when casim refuses)"""
    p = subprocess.run([CLI, "--model", model, "subst", text, var, f"{x.numerator}÷{x.denominator}", "RAD"],
                       stdout=subprocess.PIPE, text=True, encoding="utf-8")
    if p.returncode != 0:
        return None
    return sv.value(p.stdout.strip().replace(var, "X"), 0.0, False)


def typed_values(e, var, model):
    """the typed L-R at POINTS, as casim reads it; None at a point casim refuses"""
    sides = top_split(e, "=")
    out = []
    for x in POINTS:
        vals = [casim_value(s, x, var, model) for s in sides]
        out.append(None if None in vals else vals[0] - (vals[1] if len(vals) > 1 else 0))
    return out


def proportional(vals, ref, exact=False):
    """vals = c·ref for one constant c != 0 at every point both have (ref's zeros skipped); 0 for a typed 0 (EVERY X)"""
    if all(r is not None and r == 0 for r in ref):
        return all(v == 0 for v in vals)
    ratios = [v / r for v, r in zip(vals, ref) if v is not None and r is not None and (r != 0 if exact else abs(r) > 1e-12)]
    if not ratios or ratios[0] == 0:
        return False
    if exact:
        return all(q == ratios[0] for q in ratios)
    return all(abs(q - ratios[0]) <= 1e-9 * abs(ratios[0]) for q in ratios)


def roots_of(text):
    """'X=r; X=s (MULT k)' as [(r text, k)]"""
    out = []
    for part in text.split("; "):
        m = re.fullmatch(r"X=(.+?)(?: \(MULT (\d+)\))?", part)
        if not m:
            return None
        out.append((m.group(1), int(m.group(2) or 1)))
    return out


def key(f):
    """the order (#6288, D1): X, then linear by root, then quadratics by (a, b, c); a factor left last"""
    if f == "X":
        return (0,)
    c = coeffs(f)
    if len(c) == 2:
        return (1, -c[0] / c[1])
    if len(c) == 3:
        return (2, c[2], c[1], c[0])
    return (3,)


def judge(rows, e, var, model, plain, tv):
    """why rows (k, depth, line, rule, text) are wrong for e; [] when right"""
    why = []
    if [int(r[0]) for r in rows] != list(range(len(rows))):
        why.append("k is not 0..count-1")
    last = rows[-1]
    if (last[1], last[2], last[3]) != ("0", "R", "ANSWER") or last[4] != plain:
        why.append(f"the last step {last[3]}: {last[4]} is not solve's answer {plain}")
    res = [r for r in rows if r[2] == "R"]          # the one-line steps: no result repeats the one before it
    for a, b in list(zip(rows, rows[1:])) + list(zip(res, res[1:])):
        if a[4] == b[4]:
            why.append(f"step {b[0]} repeats step {a[0]}: {b[4]}")
    if any(r[3] in ("BOTH SIDES", "POLYNOMIAL") and r[4] in (e, e + "=0") for r in rows):
        why.append("the move repeats the text as typed")
    d0 = [r for r in rows if r[1] == "0" and r[2] == "R" and r[3] != "ANSWER"]
    d1 = [r for r in rows if r[1] == "1" and r[2] == "R"]
    prods, prev = [], None
    for r in rows:
        k, depth, line, rule, text = r
        try:
            if depth == "0" and line == "R" and rule != "ANSWER":
                lhs = text.removesuffix("=0")
                vals = [ev(lhs, x).a for x in POINTS]
                if not proportional([float(v) for v in vals], tv):
                    why.append(f"step {k} {rule}: {text} is not the typed L-R times one constant")
                if prev is not None and not proportional(vals, prev, exact=True):
                    why.append(f"step {k} {rule}: {text} is not the line before times one constant, exactly")
                prev = vals
                fs = factors_of(lhs)
                if rule in ("LINEAR FACTOR", "QUADRATIC FACTOR", "COMMON FACTOR"):
                    prods.append((k, fs))
            elif rule == "INTEGER COEFFICIENTS" and line == "T":
                nxt = rows[int(k) + 1][4].removesuffix("=0")
                if any(ev(text.removesuffix("=0"), x) != ev(nxt, x) for x in POINTS):
                    why.append(f"step {k}: {text} is not its result {nxt}")
                inner = text[1:closing(text)] if text.startswith("(") else None
                before = rows[int(k) - 1] if int(k) > 0 else None
                if inner is None:
                    why.append(f"step {k}: {text} is not (P)×m=0")
                elif before and before[1] == "0" and before[2] == "R":
                    if inner != before[4].removesuffix("=0"):
                        why.append(f"step {k}: {text} does not clear the line before, {before[4]}")
                elif not all(t is None or abs(float(ev(inner, x).a) - t) <= 1e-9 * max(1.0, abs(t)) for x, t in zip(POINTS, tv)):
                    why.append(f"step {k}: {text} does not clear the typed L-R")
                cs = coeffs(nxt)
                g = 0
                for v in cs:
                    g = math.gcd(g, v.numerator) if v.denominator == 1 else -1
                if g != 1 or cs[-1] <= 0:
                    why.append(f"step {int(k) + 1}: {nxt}: not whole coefficients with no common factor, the first positive")
            elif rule in ("LINEAR FACTOR", "QUADRATIC FACTOR") and line == "T":
                left, q = top_split(text, "=")
                a, f = top_split(left, "÷")
                if a == f:
                    why.append(f"step {k}: {text} divides a factor by itself (the line before shows it)")
                dv = factors_of(f)
                nxt = rows[int(k) + 1]
                after = factors_of(nxt[4].removesuffix("=0")) if nxt[2] == "R" else None
                if not dv or len(dv) != 1 or len(coeffs(dv[0][0])) - 1 != (1 if rule == "LINEAR FACTOR" else 2):
                    why.append(f"step {k} {rule}: {text}: its divisor is not one factor of that degree")
                elif nxt[3] != rule or after is None:
                    why.append(f"step {k}: {text}: the next step is not its product, {rule}")
                elif (q == "1" and after[-1] != dv[0]) or (q != "1" and (after[-1] != (q, 1) or after[-2] != dv[0])):
                    why.append(f"step {k}: {text}: the next line {nxt[4]} does not add {f} and leave {q}")
                for x in POINTS:
                    fv = ev(f, x)
                    if not fv.zero() and ev(a, x) / fv != ev(q, x):
                        why.append(f"step {k}: {text} is not an exact division")
                        break
                before = rows[int(k) - 1] if int(k) > 0 else None
                had = None
                if before and before[1] == "0" and before[2] == "R" and before[3] != "ANSWER":
                    fl = factors_of(before[4].removesuffix("=0"))
                    had = fl[-1][0] if fl and len(fl) > 1 else before[4].removesuffix("=0")
                    if a.strip("()") != had:
                        why.append(f"step {k}: {text} divides {a}, not what the line before left, {had}")
                elif not proportional([float(ev(a, x).a) for x in POINTS], tv):
                    why.append(f"step {k}: {text} divides {a}, not the typed L-R")
            elif depth == "1":
                fpart, rest = text.split("=0: ", 1) if "=0: " in text else (None, None)
                if fpart is not None and re.fullmatch(r"X(\^\d+)?", fpart):
                    n = int(fpart[2:] or 1)
                    if rest != "X=0" + (f" (MULT {n})" if n > 1 else ""):
                        why.append(f"step {k}: {text}: not the root 0 of {fpart}")
                    continue
                if fpart is None:
                    why.append(f"step {k}: {text} is not 'F=0: ...'")
                    continue
                c = coeffs(fpart)
                if rule == "LEFT":
                    m = re.fullmatch(r"DEGREE (\d+) LEFT", rest)
                    if not m or int(m.group(1)) != len(c) - 1 or len(c) < 4:
                        why.append(f"step {k}: {text}: not its degree, or below 3")
                elif rule == "QUADRATIC FORMULA" and line == "T" and rest.endswith("<0"):
                    arith, dv = rest.removesuffix("<0").split("=")
                    if len(c) != 3 or ev(arith, F(0)) != QR(int(dv)) or int(dv) >= 0 or \
                            F(int(dv)) != c[1] * c[1] - 4 * c[2] * c[0]:
                        why.append(f"step {k}: {text}: not b^2-4ac of {fpart}, or not negative")
                elif rest == "NO REAL ROOTS":
                    if len(c) != 3 or c[1] * c[1] - 4 * c[2] * c[0] >= 0:
                        why.append(f"step {k}: {text}: it has real roots")
                else:
                    rs = roots_of(rest)
                    if rs is None:
                        why.append(f"step {k}: {text}: roots not read")
                        continue
                    for rt, _ in rs:
                        if not ev(fpart, ev(rt, F(0))).zero():
                            why.append(f"step {k}: {text}: {rt} is not a root of {fpart}")
                    if rule == "QUADRATIC FORMULA" and line == "T":
                        res = roots_of(rows[int(k) + 1][4].split("=0: ", 1)[1]) or []
                        if [ev(r, F(0)).float() for r, _ in rs] != [ev(r, F(0)).float() for r, _ in res]:
                            why.append(f"step {k}: {text}: not the roots its result has")
                    elif len(rs) != (1 if len(c) == 2 else 2 if c[1] ** 2 != 4 * c[2] * c[0] else 1):
                        why.append(f"step {k}: {text}: not every root of {fpart}")
        except (ValueError, ZeroDivisionError, IndexError) as x:
            why.append(f"step {k} {rule}: {text}: not read ({x})")
    # the factors: the last product line's, at depth 1, in the order, each line adding one
    try:
        whole = d0[-1][4].removesuffix("=0") if d0 else None
        fl = factors_of(whole) if whole else None
        if fl is None or (len(fl) == 1 and fl[0][1] == 1 and not d1):
            fl = []
        listed = []
        for r in d1:
            if r[2] != "R":
                continue
            f, rest = r[4].split("=0: ", 1)
            m = re.fullmatch(r"X(?:\^(\d+))?", f)
            if m:
                listed.append(("X", int(m.group(1) or 1)))
                continue
            rs = roots_of(rest)
            if rs and len({m for _, m in rs}) != 1:
                why.append(f"step {r[0]}: {r[4]}: its roots differ in multiplicity")
            listed.append((f, rs[0][1] if rs else None))
        if fl:
            if [f for f, _ in fl] != [f for f, _ in listed]:
                why.append(f"the depth-1 factors {[f for f, _ in listed]} are not the product's {[f for f, _ in fl]}")
            for (f, n), (_, m) in zip(fl, listed):
                if m is not None and m != n:
                    why.append(f"{f}: (MULT {m}) at depth 1, ^{n} in the product")
        elif len(listed) == 1 and listed[0][0] != "X":
            if not proportional([float(ev(listed[0][0], x).a) ** (listed[0][1] or 1) for x in POINTS], tv):
                why.append(f"the one factor {listed[0][0]} is not the typed L-R")
        keys = [key(f) for f, _ in listed]
        if keys != sorted(keys):
            why.append(f"the factors are not in the order: {[f for f, _ in listed]}")
        # a product line is the factors so far and what is left (its last one): the next keeps the factors so far and
        # adds one, what it left being the last again (or nothing left)
        if prods and prods[0][1] is not None and len(prods[0][1]) - (prods[0][1][0][0] == "X") > 2:
            why.append(f"step {prods[0][0]} takes out more than one factor")
        for (k0, a), (k1, b) in zip(prods, prods[1:]):
            if a is None or b is None or b[:len(a) - 1] != a[:-1] or len(b) - len(a) not in (0, 1):
                why.append(f"step {k1} does not add one factor to step {k0}")
        # every root zeroes the whole polynomial
        full = whole if whole else (listed[0][0] if len(listed) == 1 else None)
        if full:
            for r in d1:
                rest = r[4].split("=0: ", 1)[1] if "=0: " in r[4] else ""
                for rt, _ in (roots_of(rest) or []) if rest.startswith("X=") else []:
                    if not ev(full, ev(rt, F(0))).zero():
                        why.append(f"{rt} is not a root of {full}")
    except (ValueError, ZeroDivisionError, IndexError) as x:
        why.append(f"the factors not read ({x})")
    return why


def run(args):
    p = subprocess.run([CLI] + args, stdout=subprocess.PIPE, text=True, encoding="utf-8")
    return p.returncode, p.stdout


def readback(rows, var, model):
    """each side of every line, read by casim (subst at a point) and by this reader, agree (D2)"""
    why, x = [], F(3, 7)
    for r in rows:
        if r[3] == "ANSWER":
            continue
        sides = []
        for part in re.split(r"=0: |; |=|<0", r[4]):
            part = re.sub(r" \(MULT \d+\)$", "", part).removeprefix("X=").strip()
            if part and part not in ("0", "NO REAL ROOTS") and not part.startswith("DEGREE"):
                sides.append(part)
        for s in sides:
            mine = ev(s, x).float()
            theirs = casim_value(s.replace("X", var), x, var, model)
            if theirs is None or abs(theirs - mine) > 1e-9 * max(1.0, abs(mine)):
                why.append(f"step {r[0]}: {s} reads back in casim as {theirs}, here {mine}")
    return why


def check(e, var, angle, model, counts):
    """e through plain solve and both steps; the reasons it fails"""
    pc, pout = run(["--model", model, "solve", e, var, angle])
    pout = pout.strip()
    why = []
    outs = {}
    for one in (False, True):
        rc, out = run(["--tsv", "--model", model] + (["--one-line"] if one else []) + ["steps", "solve", e, var, angle])
        outs[one] = (rc, out)
        if (rc == 0) != (pc == 0) or (rc != 0 and out.strip() != pout):
            why.append(f"steps {'one line' if one else 'two lines'}: exit {rc} {out.strip()[-60:]!r}, solve {pc} {pout!r}")
        if run(["--tsv", "--model", model] + (["--one-line"] if one else []) + ["steps", "solve", e, var, angle])[1] != out:
            why.append("a second run differs")
    if pc != 0 or why:
        return why
    rows = [r.split("\t") for r in outs[False][1].rstrip("\n").split("\n")]
    ones = [r.split("\t") for r in outs[True][1].rstrip("\n").split("\n")]
    if [r[1:] for r in ones] != [r[1:] for r in rows if r[2] == "R"]:
        why.append("the one-line steps are not the two-line steps' results")
    tv = typed_values(e, var, model)
    if var != "X":
        alone = re.compile(rf"(?<![A-Z]){var}(?![A-Z])")
        rows = [[r[0], r[1], r[2], r[3], alone.sub("X", r[4])] for r in rows]
        pout = alone.sub("X", pout)
    why += judge(rows, e, var, model, pout, tv)
    why += readback(rows, var, model)
    counts["lines"] += len(rows)
    return why


# solves the corpus lacks: a step line past 191 bytes, bounded by the caller's cap and not by solve's (D5), and
# quadratics to a power (the hidden set's (MULT 5), abacus #6288), with and without real roots
LONG = ("(X-1)×(X^11+1234567×X^10-1345678×X^9+1456789×X^8-1567891×X^7+1678912×X^6-1789123×X^5+1891234×X^4-1912345×X^3"
        "+1123456×X^2-1234561×X+1345671)")
AIMED = [(LONG, "X", "RAD", "35"), ("(X^2-2)^5", "X", "RAD", "35"), ("(X^2+X+1)^3×(X-2)^2", "X", "RAD", "35"),
         ("X^3×(3×X^2-5)^2=0", "X", "RAD", "35"), ("(2×X+1)^4×(X^2-3)^3", "X", "RAD", "35")]


def known_inputs():
    """(text, var, angle, model) of every solve in the golden corpus, docs/language.md and AIMED"""
    out = set(AIMED)
    for line in open(os.path.join(ROOT, "tests", "golden", "corpus.txt"), encoding="utf-8"):
        f = line.rstrip("\n").split("\t")
        if f[0] == "solve" and "\\" not in f[7]:
            out.add((f[7], chr(65 + int(f[2])), ANGLE[f[3]], MODEL.get(f[1], "35")))
    for m in re.finditer(r'`casim (?:steps )?solve "([^"]+)" ([A-Z]) (RAD|DEG|GRAD)`',
                         open(os.path.join(ROOT, "docs", "language.md"), encoding="utf-8").read()):
        out.add((m.group(1), m.group(2), m.group(3), "35"))
    return sorted(out)


def poly_text(c):
    """coefficients as casim's input: descending, p÷q after the power"""
    out = ""
    for k in range(len(c) - 1, -1, -1):
        v = c[k]
        if v == 0:
            continue
        a = abs(v)
        s = "-" if v < 0 else "+" if out else ""
        body = ("" if a.numerator == 1 and k else str(a.numerator)) + ("×" if a.numerator != 1 and k else "")
        body += ("X" + (f"^{k}" if k > 1 else "")) if k else ""
        body += f"÷{a.denominator}" if a.denominator != 1 else ""
        out += s + body
    return out or "0"


def pmul(a, b):
    out = [F(0)] * (len(a) + len(b) - 1)
    for i, x in enumerate(a):
        for j, y in enumerate(b):
            out[i + j] += x * y
    return out


def random_case(rnd):
    """a polynomial from known factors (linear, quadratic, a cubic left, X^k), multiplicities, a rational content;
    typed as the product, multiplied out, or an equation with terms moved"""
    fs, deg = [], 0
    while deg < rnd.randint(1, 9):
        r = rnd.random()
        if r < 0.5:
            f = [F(rnd.choice([-1, 1]) * rnd.randint(0, 9)), F(rnd.randint(1, 4))]
        elif r < 0.85:
            f = [F(rnd.randint(-9, 9)), F(rnd.randint(-6, 6)), F(rnd.randint(1, 3))]
        elif r < 0.95:
            f = [F(-5), F(-2), F(0), F(1)]               # X^3-2X-5: no rational root, left
        else:
            f = [F(0), F(1)]
        m = rnd.choice([1, 1, 1, 2, 3])
        if deg + m * (len(f) - 1) > 12:
            break
        fs.append((f, m))
        deg += m * (len(f) - 1)
    if not fs:
        fs = [([F(-2), F(1)], 1)]
    k = F(rnd.choice([1, 1, 2, -1, -3]), rnd.choice([1, 1, 2, 3]))
    style = rnd.random()
    if style < 0.4:
        text = ("" if k == 1 else f"({poly_text([k])})×") + "×".join(
            f"({poly_text(f)})" + (f"^{m}" if m > 1 else "") for f, m in fs)
        return text
    p = [k]
    for f, m in fs:
        for _ in range(m):
            p = pmul(p, f)
    if style < 0.7:
        return poly_text(p)
    # an equation: some terms on the right
    left, right = p[:], [F(0)] * len(p)
    for i in range(len(p)):
        if rnd.random() < 0.4:
            right[i], left[i] = -p[i], F(0)
    return poly_text(left) + "=" + poly_text(right)


def planted():
    """the judge refuses wrong steps, each changed from a right run: returns the ones it accepts"""
    e = "X^3-3×X+2"
    rc, out = run(["--tsv", "steps", "solve", e, "X", "RAD"])
    rows = [r.split("\t") for r in out.rstrip("\n").split("\n")]
    plain = run(["solve", e, "X", "RAD"])[1].strip()
    tv = typed_values(e, "X", "35")
    accepted = []
    control = judge(rows, e, "X", "35", plain, tv)
    if control:
        return [], control
    changes = [
        ("a product line times X", lambda t: t.replace("(X+2)×(X^2-2×X+1)=0", "X×(X+2)×(X^2-2×X+1)=0")),
        ("a wrong root", lambda t: t.replace("X+2=0: X=-2", "X+2=0: X=2")),
        ("a multiplicity dropped", lambda t: t.replace("(X-1)^2=0", "(X-1)=0")),
        ("a division not exact", lambda t: t.replace("÷(X+2)=X^2-2×X+1", "÷(X+2)=X^2-2×X+2")),
        ("a divisor and its quotient swapped", lambda t: t.replace("÷(X+2)=X^2-2×X+1", "÷(X^2-2×X+1)=X+2")),
        ("a division short of its multiplicity", lambda t: t.replace("÷(X-1)^2=1", "÷(X-1)=X-1")),
        ("the depth-1 order", lambda t: t),
    ]
    for name, change in changes:
        bad = [r[:4] + [change(r[4])] for r in rows]
        if name == "the depth-1 order":
            i = [j for j, r in enumerate(bad) if r[1] == "1"]
            bad[i[0]], bad[i[1]] = bad[i[1]][:1] + bad[i[0]][1:], bad[i[0]][:1] + bad[i[1]][1:]
        if bad == rows or not judge(bad, e, "X", "35", plain, tv):
            accepted.append(name)
    q = [["0", "1", "T", "QUADRATIC FORMULA", "X^2+X+1=0: 1^2-4×1×1=-5<0"],
         ["1", "1", "R", "QUADRATIC FORMULA", "X^2+X+1=0: NO REAL ROOTS"], ["2", "0", "R", "ANSWER", "NO REAL ROOTS"]]
    if not judge(q, "X^2+X+1", "X", "35", "NO REAL ROOTS", typed_values("X^2+X+1", "X", "35")):
        accepted.append("a wrong discriminant")
    return accepted, []


def main():
    rc, out = run(["--tsv", "steps", "solve", LONG, "X", "RAD"])
    if rc != 0 or max(len(r.split("\t")[4].encode()) for r in out.rstrip("\n").split("\n")) <= 191:
        print(f"FAIL scripts/ssteps-values.py: no step line past 191 bytes pages (exit {rc}): the cap is not tested")
        sys.exit(1)
    acc, control = planted()
    if acc:
        print(f"FAIL scripts/ssteps-values.py: its judge accepts: {', '.join(acc)}")
        sys.exit(1)
    if control:                         # the planted faults' own case is wrong: say why, then judge the rest
        print("FAIL scripts/ssteps-values.py: X^3-3×X+2, the planted faults' case\n  " + "\n  ".join(control[:6]))
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    rnd = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 20261010)
    cases = known_inputs() + [(random_case(rnd), "X", "RAD", "35") for _ in range(n)]
    counts = {"lines": 0}
    failed = 0
    for e, var, angle, model in cases:
        why = check(e, var, angle, model, counts)
        if why:
            failed += 1
            print(f"FAIL scripts/ssteps-values.py: {e} ({var}, {angle}, model {model})\n  " + "\n  ".join(why[:6]))
    print(f"solve steps: {len(cases)} equations ({len(cases) - n} from the corpus, the docs and aimed ones), {counts['lines']} "
          f"steps judged exactly and read back by casim (one line past 191 bytes); the judge refused its eight planted faults\n"
          f"checks: {len(cases)} failed: {failed}")
    sys.exit(1 if failed or control else 0)


if __name__ == "__main__":
    main()
