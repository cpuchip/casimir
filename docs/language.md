# Casimir's input language

What Casimir reads and writes, for someone who has never seen the calculator it was written for. It describes the
library as it is. The rules come from the work units (work/) and docs/canonical-form.md, and nothing new is decided
here. Every example in the tables is a command line and the exact text it prints, and `make check` runs them all
again (scripts/doc-examples.py).

## The text

An expression is UTF-8 text in the calculator's notation:

| Write | For | Not |
|---|---|---|
| `×` (U+00D7), `÷` (U+00F7) | times, divide | `*`, `/` (SYNTAX) |
| `+`, `-`, `^` | plus, minus (and negation), power | `−` (U+2212) |
| `A` to `Z` | variables, one letter each | lower case, longer names |
| `π` (U+03C0) | pi | `pi` |
| `2`, `0.25`, `1.5E-3`, `1.2.3` | numbers: decimals, `E` for ×10^, and a.b.c for the fraction a + b÷c | `2E`, `#1F` (based numbers) |
| `SIN(X)` | a function, its argument in parentheses | `SIN X`, `sin(X)` |
| `L=R` | an equation (one `=`) | |
| `2×X` | a product, written out | `2X`: implied products are not read |

Numbers are exact rationals: `0.1` is 1/10, and `0.1+0.2` is `0.3`. A number that cannot be held exactly (more than
18 digits, or a sum or product past 2^62) is kept as typed and is not folded.

**Functions Casimir knows mathematically:** `LN`, `LOG` (base 10), `EXP`, `ALOG` (10^x), `SQ` (x^2), `SQRT`, `INV`
(1/x), `ABS`, `SIN`, `COS`, `TAN`, `ASIN`, `ACOS`, `ATAN`, `SINH`, `COSH`, `TANH`, `ASINH`, `ACOSH`, `ATANH`.

**The calculator's other names** are opaque. simplify, expand, collect and subst keep them, simplify their
arguments and collect like terms (`X!+X!` is `2×X!`). derive refuses them by name with NOT_DIFFERENTIABLE. They include
`IP`, `FP`, `RND`, `INTG`, `SGN`, `IDIV`, `RMDR`, `XROOT`, `nCr`, `nPr`, `%`, `%CHG`, the unit conversions (`→KM`
and the like), the statistics names, `!` (factorial, postfix), and the draws `RAND` and `SEED`, which are never
merged, cancelled or reordered (`RAND+RAND` stays). Complex literals, based numbers and vectors are refused.

A SYNTAX status puts the offset of the first error in out, counted in characters from 0.

## The models: how powers chain

The calculator types an equation in one of three models, and Casimir reads each text in the model it was typed in
(casim_req's `model`; the CLI's `--model`):

- **35** (the default) and **33** group `^` left to right, and a minus that starts an exponent binds to the next
  number: `2^3^2` is (2^3)^2 = 64, and `2^-3^2` is (2^-3)^2.
- **STU** groups `^` right to left, and a minus that starts an exponent covers the chain after it: `2^3^2` is
  2^(3^2) = 512, and `2^-3^2` is 2^-(3^2).

The output reads the same in every model. Each chain of powers carries its parentheses, `(a^b)^c` or `a^(b^c)`, and
each product its `×`.

| Command | Prints |
|---|---|
| `casim --model 35 simplify "2^3^2" RAD` | `64` |
| `casim --model STU simplify "2^3^2" RAD` | `512` |

## The angle modes

`RAD`, `DEG` or `GRAD` (CASIM_RAD, CASIM_DEG, CASIM_GRAD). The mode matters where angles do:
- derivatives and integrals of SIN, COS and TAN carry the mode's factor (π÷180 in DEG, π÷200 in GRAD);
- exact values: `SIN(30)` is `0.5` in DEG, and `SIN(π÷6)` is `0.5` in RAD;
- the inverses return their principal angle in the mode.

## The operations

Each operation is one call: `casim_run` with the operation in the request (casim.h), or one function per operation.
Its answer goes in `out` with a status. Every operation also returns:
- SYNTAX for text it cannot read;
- NO_ROOM when the arena runs out, or when nesting passes the depth guard (44 levels);
- BAD_ARG for a variable, angle or model out of range.

The statuses particular to each operation are listed under it.

The command line, `build/casim`, prints the answer, or the status and what it reports, and exits 0 for OK, 1 for a
status and 2 for a usage error. The examples below are its commands.

### derive: the derivative in a variable

An expression gives its derivative, `L=R` gives `L'=R'`, simplified. Statuses: NOT_DIFFERENTIABLE (out: the name it
refused).

| Command | Prints |
|---|---|
| `casim derive "X^2×SIN(X)" X RAD` | `2×X×SIN(X)+X^2×COS(X)` |
| `casim derive "SIN(X)" X DEG` | `π×COS(X)÷180` |
| `casim derive "X=Y^2" Y RAD` | `0=2×Y` |
| `casim derive "IP(X)×X" X RAD` | `NOT_DIFFERENTIABLE IP` |
| `casim derive "2×" X RAD` | `SYNTAX 2` |

### simplify: the canonical form

One text for each expression, with the same value wherever the input is defined (docs/canonical-form.md). It does
these things:
- collects like terms;
- folds numbers exactly;
- makes a sum that is a factor primitive (`(2×X+2)×Y` is `2×Y×(X+1)`);
- evaluates exact constants: SIN, COS and TAN at multiples of π/6 and π/4, their inverses, LN(1), EXP(0), LOG of a
  power of 10 and perfect powers;
- prints a product with π in its numerator with its coefficient as a fraction (`π÷4`, not `0.25×π`); π under a ÷ or
  a root keeps the decimal (`0.5÷π`);
- combines the logs of positive numbers in a sum: LN with LN and LOG with LOG, when their coefficients have one
  denominator, the coefficients' gcd outside (`2×LN(2)+2×LN(3)` is `2×LN(6)`), and LN(1÷m) is -LN(m). Coefficients
  with different denominators stay apart, and so do logs of letters: `LN(X)+LN(Y)` is not `LN(X×Y)`, which is
  defined where X and Y are both negative.

It never expands, factors or applies trigonometric identities. Statuses: UNDEFINED (an exact 0 under a negative
power, or a pole: the expression has no value).

**Poles (CAS 016).** A function at a pole has no value, in every op, as 1÷0 has none: TAN where COS is 0 (π÷2 and its
odd multiples in RAD, 90 and 270 in DEG, 100 and 300 in GRAD), LN and LOG of 0, ATANH of 1 and -1. simplify, expand,
subst, derive, integrate, collect, solve, factor and the rest answer UNDEFINED there. TAN(90) in RAD is no pole and is
kept. A function outside its real domain where it has a complex value is kept as typed, for the calculator to value:
LN(-1), ASIN(2), SQRT(-4), (-8)^(1÷3), ACOSH(0). So that a pole behind them is seen, SINH, TANH, ASINH and ATANH of 0 are
0, COSH(0) and ALOG(0) are 1, and ACOSH(1) is 0: LN(SINH(0)) is UNDEFINED. In solve and factor a pole decides wherever
it is: SIN(X)+TAN(π÷2)=0 is UNDEFINED. A known difference: there a division by 0 (X÷0, 1÷0+X) is still NOT_NUMERIC, the
one place a ÷0 isn't UNDEFINED, until the next unit that touches solve.

| Command | Prints |
|---|---|
| `casim simplify "2×X+3×X-X×X^2" RAD` | `5×X-X^3` |
| `casim simplify "(2×X+2)×Y" RAD` | `2×Y×(X+1)` |
| `casim simplify "0.1+0.2+1÷3" RAD` | `19÷30` |
| `casim simplify "LN(EXP(X))+X^(1÷2)" RAD` | `X+SQRT(X)` |
| `casim simplify "SIN(30)+COS(45)" DEG` | `0.5×SQRT(2)+0.5` |
| `casim simplify "ATAN(1)+ACOS(-0.5)" RAD` | `11×π÷12` |
| `casim simplify "LN(6)-LN(2)" RAD` | `LN(3)` |
| `casim simplify "3×LN(2)-LN(4)" RAD` | `LN(2)` |
| `casim simplify "4×LN(2)+6×LN(3)" RAD` | `2×LN(108)` |
| `casim simplify "2×LN(2)+2×LN(3)+LOG(20)-LOG(2)" RAD` | `2×LN(6)+1` |
| `casim simplify "LN(0.5)+LN(X)+LN(Y)" RAD` | `LN(X)+LN(Y)-LN(2)` |
| `casim simplify "LN(8)+LN(4)÷3" RAD` | `LN(4)÷3+LN(8)` |
| `casim simplify "1÷COS(π÷2)" RAD` | `UNDEFINED` |
| `casim simplify "TAN(π÷2)" RAD` | `UNDEFINED` |
| `casim simplify "TAN(90)" DEG` | `UNDEFINED` |
| `casim simplify "LN(0)" RAD` | `UNDEFINED` |
| `casim simplify "LN(-1)" RAD` | `LN(-1)` |

### expand, collect, subst

- **expand** distributes `×` over `+` and raises sums to whole powers, at every depth. A sum in a denominator stays,
  with its inside expanded.
- **collect** writes the expression as Σ c×var^k in descending k, with the terms where var is not a whole power after
  them.
- **subst** replaces every var with a second text, as if it were in parentheses, then simplifies.

Statuses: TOO_LONG (the answer is past 191 bytes; expand stops before building more than 64 terms).

| Command | Prints |
|---|---|
| `casim expand "(X+1)^3" RAD` | `3×X+3×X^2+X^3+1` |
| `casim expand "(X+1)÷(X×Y)" RAD` | `1÷Y+1÷X÷Y` |
| `casim expand "(X+Y+Z+1)^9" RAD` | `TOO_LONG` |
| `casim collect "A×X+B×X^2+C×X+SIN(X)" X RAD` | `B×X^2+(A+C)×X+SIN(X)` |
| `casim subst "X^2+X" X "Y+1" RAD` | `Y+(Y+1)^2+1` |

### solve: real roots of a polynomial equation

`L=R` is read as L−R=0, and an expression as expr=0. The polynomial is read as typed: a variable in a denominator or
inside a function is refused even where it would cancel. The coefficients must be exact rationals. out is one line,
its result first:
- SOLVED: every root, each found by exact division, with any multiplicity above 1 as (MULT k);
- PARTIAL, DEGREE n LEFT: a factor of degree 3 or more is left, with the roots found so far;
- NO REAL ROOTS, NO SOLUTION (for a nonzero constant), or EVERY X (for 0).

Statuses: NOT_POLYNOMIAL, NOT_NUMERIC (a coefficient that is another letter, π or an irrational number), NO_ROOM
(degree past 12, or a coefficient past 10^7 once the polynomial is cleared to integers), TOO_LONG.

| Command | Prints |
|---|---|
| `casim solve "X^2-5×X+6=0" X RAD` | `SOLVED: X=2; X=3` |
| `casim solve "X^3-3×X+2" X RAD` | `SOLVED: X=-2; X=1 (MULT 2)` |
| `casim solve "2×X^2-4" X RAD` | `SOLVED: X=-SQRT(2); X=SQRT(2)` |
| `casim solve "X^2+1" X RAD` | `NO REAL ROOTS` |
| `casim solve "(X+3)×(X^3-2×X-5)" X RAD` | `PARTIAL, DEGREE 3 LEFT: X=-3` |
| `casim solve "SIN(X)=0.5" X RAD` | `NOT_POLYNOMIAL` |
| `casim solve "A×X^2-1" X RAD` | `NOT_NUMERIC` |
| `casim solve "X^14-1" X RAD` | `NO_ROOM` |

### integrate: an antiderivative

The table's forms, recognised as typed:
- sums, and constant factors;
- x^n and (aX+b)^n for rational n, with 1÷(aX+b) as LN(ABS(…));
- EXP, SIN, COS, TAN, ALOG, SINH and COSH of a linear argument;
- a polynomial times a polynomial.

The answer has no "+C". Every answer is differentiated back and must equal the integrand exactly before it is
written. Statuses: NOT_INTEGRABLE (not a form in the table, or an answer the check could not close), BAD_ARG (an
equation), TOO_LONG.

| Command | Prints |
|---|---|
| `casim integrate "3×X^2+COS(2×X)" X RAD` | `0.5×SIN(2×X)+X^3` |
| `casim integrate "1÷(2×X+1)" X RAD` | `0.5×LN(ABS(2×X+1))` |
| `casim integrate "SIN(X)" X DEG` | `-180×COS(X)÷π` |
| `casim integrate "X×EXP(X^2)" X RAD` | `NOT_INTEGRABLE` |
| `casim integrate "X=1" X RAD` | `BAD_ARG` |

When the table refuses, a rational integrand n÷d (as partfrac reads it) is integrated through its partial fractions
(work/013-rational-integrals.md): the polynomial part by powers, c÷(aX+b)^j as LN(ABS(aX+b))÷a or by the power rule,
and (pX+q)÷(aX^2+bX+c) with b^2−4ac < 0 as an LN (no ABS: the quadratic is positive) and an ATAN, which carries
π÷180 in DEG and π÷200 in GRAD. Each term's antiderivative is differentiated back to its own term before anything
prints, and the answer is the canonical form of their sum. It refuses (NOT_INTEGRABLE):
- a quadratic factor with b^2−4ac > 0 that is not a square (X^2−2), or one to a power of 2 or more;
- a factor of degree 3 or more;
- a fraction where something cancels (X^2÷X): the reduced function's antiderivative would be defined where the
  integrand isn't;
- an integrand that mixes the two (SIN(X)+1÷(X^2−1)), or holds another letter.

It answers NO_ROOM only when partfrac itself runs out of room (or past int64), never for a refusal of the form.

| Command | Prints |
|---|---|
| `casim integrate "(5×X+1)÷(X^2+X-2)" X RAD` | `2×LN(ABS(X-1))+3×LN(ABS(X+2))` |
| `casim integrate "1÷(1-X^4)" X RAD` | `-0.25×LN(ABS(X-1))+0.25×LN(ABS(X+1))+0.5×ATAN(X)` |
| `casim integrate "1÷(X^2+X+1)" X RAD` | `2×ATAN(2×(X+0.5)÷SQRT(3))÷SQRT(3)` |
| `casim integrate "1÷(X^2+1)" X DEG` | `π×ATAN(X)÷180` |
| `casim integrate "X^2÷X" X RAD` | `NOT_INTEGRABLE` |
| `casim integrate "1÷(X^2-2)" X RAD` | `NOT_INTEGRABLE` |

### defint: a definite integral, exactly

`defint "<integrand>" X LO HI` gives F(HI) − F(LO), with integrate's checked F and the exact values above. The bounds
are constants: a letter is BAD_ARG, a bound with no value is UNDEFINED, and a bound that is not real is BAD_ARG. LO >
HI reverses the sign. Statuses: IMPROPER (a pole or domain edge of the integrand on the closed interval, or within
1E-20 of it), integrate's statuses, BAD_ARG, UNDEFINED, TOO_LONG.

| Command | Prints |
|---|---|
| `casim defint "SIN(X)" X 0 π RAD` | `2` |
| `casim defint "X^2" X 1 3 RAD` | `26÷3` |
| `casim defint "1÷X" X 2 6 RAD` | `LN(3)` |
| `casim defint "1÷X" X -1 1 RAD` | `IMPROPER` |
| `casim defint "X^2" X B 1 RAD` | `BAD_ARG` |

### factor: over the rationals

The rational content first (when it is not 1), then each irreducible factor once. Each factor is primitive with
integer coefficients and a positive leading coefficient, with `^k` for a multiplicity. Each factor of degree 2 or
more is proven irreducible, and the product is read back and must equal the input exactly. Statuses: PARTIAL (out
is a correct factorization, but one factor of degree 4 or more could not be proven irreducible), NOT_POLYNOMIAL,
NOT_NUMERIC, NO_ROOM, BAD_ARG (an equation), TOO_LONG.

| Command | Prints |
|---|---|
| `casim factor "X^4-1" X RAD` | `(X-1)×(X+1)×(X^2+1)` |
| `casim factor "6×X^2+5×X+1" X RAD` | `(2×X+1)×(3×X+1)` |
| `casim factor "X^3-6×X^2+12×X-8" X RAD` | `(X-2)^3` |
| `casim factor "SIN(X)+1" X RAD` | `NOT_POLYNOMIAL` |

### steps: derive's working, a step a page

`steps "<text>" X` prints the working of derive as a student would walk it (work/steps-derive.md): each rule first
with its parts still to do (`d/dX(u)` for each), then each part's own steps, indented, then the rule's result. The
last line is derive's answer. `--one-line` prints the results only, the answer still last. A part without X is one
CONSTANT step; X itself is VARIABLE. In C, `casim_step(rq, k, flags, &info, out, ...)` returns page k of the same
walk, with its depth, line and rule (`casim_rule_name` names it), so a caller can page forwards and backwards; each
call is complete in itself. Statuses: derive's, and NO_ROOM when the steps don't fit the arena.

| Command | Prints (lines joined by " / ") |
|---|---|
| `casim steps "X^2×SIN(X)" X RAD` | `PRODUCT: d/dX(X^2×SIN(X)) = d/dX(X^2)×SIN(X)+X^2×d/dX(SIN(X)) /   POWER: d/dX(X^2) = 2×X /   SIN: d/dX(SIN(X)) = COS(X) / PRODUCT: d/dX(X^2×SIN(X)) = 2×X×SIN(X)+X^2×COS(X)` |
| `casim steps "X×SIN(X)" X RAD` | `PRODUCT: d/dX(X×SIN(X)) = d/dX(X)×SIN(X)+X×d/dX(SIN(X)) /   VARIABLE: d/dX(X) = 1 /   SIN: d/dX(SIN(X)) = COS(X) / PRODUCT: d/dX(X×SIN(X)) = SIN(X)+X×COS(X)` |
| `casim steps "3×X^2" X RAD` | `CONSTANT MULTIPLE: d/dX(3×X^2) = 3×d/dX(X^2) /   POWER: d/dX(X^2) = 2×X / CONSTANT MULTIPLE: d/dX(3×X^2) = 6×X` |
| `casim steps "SIN(X^2)" X RAD` | `CHAIN: d/dX(SIN(X^2)) = COS(X^2)×d/dX(X^2) /   POWER: d/dX(X^2) = 2×X / CHAIN: d/dX(SIN(X^2)) = 2×X×COS(X^2)` |
| `casim steps "2^X+π^2" X RAD` | `SUM: d/dX(π^2+2^X) = d/dX(π^2)+d/dX(2^X) /   CONSTANT: d/dX(π^2) = 0 /   EXPONENTIAL: d/dX(2^X) = LN(2)×2^X / SUM: d/dX(π^2+2^X) = LN(2)×2^X` |
| `casim steps "X^X" X RAD` | `GENERAL POWER: d/dX(X^X) = X^X×(LN(X)+1)` |
| `casim steps "X^2=Y+3×X" X RAD` | `BOTH SIDES: d/dX(X^2)=d/dX(3×X+Y) /   POWER: d/dX(X^2) = 2×X /   SUM: d/dX(3×X+Y) = d/dX(3×X)+d/dX(Y) /     CONSTANT MULTIPLE: d/dX(3×X) = 3 /     CONSTANT: d/dX(Y) = 0 /   SUM: d/dX(3×X+Y) = 3 / BOTH SIDES: 2×X=3` |
| `casim --one-line steps "X^2×SIN(X)" X RAD` | `POWER: d/dX(X^2) = 2×X /   SIN: d/dX(SIN(X)) = COS(X) / PRODUCT: d/dX(X^2×SIN(X)) = 2×X×SIN(X)+X^2×COS(X)` |

`steps integrate "<text>" X` walks integrate the same way (work/steps-integrate.md). Each integrand is shown as it
was typed and each answer in canonical form, without "+C". The steps are:
- SUM splits a sum into its terms;
- CONSTANT MULTIPLE takes the constant out;
- a table form (POWER, RECIPROCAL, EXP, SIN, …) first states its substitution `∫(SIN(U))dU÷2, U=2×X` when the
  argument is aX+b, not X itself;
- POLYNOMIAL multiplies a product out;
- every term's CHECK differentiates its answer back before its result.

| Command | Prints (lines joined by " / ") |
|---|---|
| `casim steps integrate "X^2+SIN(2×X)" X RAD` | `SUM: ∫(X^2+SIN(2×X))dX = ∫(X^2)dX+∫(SIN(2×X))dX /   CHECK: d/dX(X^3÷3) = X^2 /   POWER: ∫(X^2)dX = X^3÷3 /   SIN: ∫(SIN(2×X))dX = ∫(SIN(U))dU÷2, U=2×X /   CHECK: d/dX(-0.5×COS(2×X)) = SIN(2×X) /   SIN: ∫(SIN(2×X))dX = -0.5×COS(2×X) / SUM: ∫(X^2+SIN(2×X))dX = -0.5×COS(2×X)+X^3÷3` |
| `casim steps integrate "3×COS(X)" X RAD` | `CONSTANT MULTIPLE: ∫(3×COS(X))dX = 3×∫(COS(X))dX /   COS: ∫(COS(X))dX = SIN(X) / CHECK: d/dX(3×SIN(X)) = 3×COS(X) / CONSTANT MULTIPLE: ∫(3×COS(X))dX = 3×SIN(X)` |
| `casim steps integrate "1÷(2×X+1)" X RAD` | `RECIPROCAL: ∫(1÷(2×X+1))dX = ∫(1÷U)dU÷2, U=2×X+1 / CHECK: d/dX(0.5×LN(ABS(2×X+1))) = 1÷(2×X+1) / RECIPROCAL: ∫(1÷(2×X+1))dX = 0.5×LN(ABS(2×X+1))` |
| `casim steps integrate "(X+1)×(X-2)" X RAD` | `POLYNOMIAL: ∫((X+1)×(X-2))dX = ∫(-X+X^2-2)dX / CHECK: d/dX(-2×X-0.5×X^2+X^3÷3) = (X+1)×(X-2) / POLYNOMIAL: ∫((X+1)×(X-2))dX = -2×X-0.5×X^2+X^3÷3` |
| `casim steps integrate "5-EXP(3×X)" X RAD` | `SUM: ∫(5-EXP(3×X))dX = ∫(5)dX-∫(EXP(3×X))dX /   CHECK: d/dX(5×X) = 5 /   CONSTANT: ∫(5)dX = 5×X /   EXP: ∫(EXP(3×X))dX = ∫(EXP(U))dU÷3, U=3×X /   CHECK: d/dX(EXP(3×X)÷3) = EXP(3×X) /   EXP: ∫(EXP(3×X))dX = EXP(3×X)÷3 / SUM: ∫(5-EXP(3×X))dX = 5×X-EXP(3×X)÷3` |
| `casim steps integrate "(-3)×SQRT(X)^3" X RAD` | `CONSTANT MULTIPLE: ∫((-3)×SQRT(X)^3)dX = -3×∫(SQRT(X)^3)dX /   POWER: ∫(SQRT(X)^3)dX = 0.4×X^2.5 / CHECK: d/dX(-1.2×X^2.5) = (-3)×SQRT(X)^3 / CONSTANT MULTIPLE: ∫((-3)×SQRT(X)^3)dX = -1.2×X^2.5` |

An integrand the table refuses but integrate answers as a rational function (CAS 013) walks through its partial
fractions (work/013-rational-integrals.md §Steps; more in work/013-steps-examples.txt):
- PARTIAL FRACTIONS rewrites the integrand as partfrac writes it, with partfrac's own pages a level down; it is left
  out when partfrac's answer is the integrand itself in canonical form, and the one term's walk is then the whole;
- SUM, then each term: the polynomial part and each c÷(m×(aX+b)^j) by the table's own steps, the constant c÷m taken
  out first by CONSTANT MULTIPLE;
- a quadratic f with no real root: SPLIT writes (B×X+C)÷f as a multiple of f'÷f and a multiple of 1÷f (CONSTANT
  MULTIPLE when only one is there); LOG DERIVATIVE takes ∫(f'÷f)dX = LN(f), with no ABS since f is never 0;
  COMPLETE THE SQUARE writes f = a×((X+h)^2+k), left out for X^2+k; ARCTANGENT substitutes U=X+h, and gives the ATAN
  with the angle mode's factor;
- each part's CHECK is proved before it prints, and the last line is integrate's answer.

| Command | Prints (lines joined by " / ") |
|---|---|
| `casim steps integrate '(X^2+1)÷(X-1)^3' X RAD` | `PARTIAL FRACTIONS: ∫((X^2+1)÷(X-1)^3)dX = ∫(1÷(X-1)+2÷(X-1)^2+2÷(X-1)^3)dX /   FACTOR: X^3-3×X^2+3×X-1=(X-1)^3 /   FORM: (X^2+1)÷(X^3-3×X^2+3×X-1)=A÷(X-1)+B÷(X-1)^2+C÷(X-1)^3 /   CLEARED: X^2+1=A×(X-1)^2+B×(X-1)+C /     COVER-UP: X=1: 2=C /     COVER-UP: C=2 /     COEFFICIENTS: X^2: 1=A /     COEFFICIENTS: X: 0=-2×A+B /     COEFFICIENTS: X^0: -1=A-B /     COEFFICIENTS: A=1; B=2 /   ANSWER: 1÷(X-1)+2÷(X-1)^2+2÷(X-1)^3 /   SUM: ∫(1÷(X-1)+2÷(X-1)^2+2÷(X-1)^3)dX = ∫(1÷(X-1))dX+∫(2÷(X-1)^2)dX+∫(2÷(X-1)^3)dX /     RECIPROCAL: ∫(1÷(X-1))dX = ∫(1÷U)dU, U=X-1 /     CHECK: d/dX(LN(ABS(X-1))) = 1÷(X-1) /     RECIPROCAL: ∫(1÷(X-1))dX = LN(ABS(X-1)) /     CONSTANT MULTIPLE: ∫(2÷(X-1)^2)dX = 2×∫(1÷(X-1)^2)dX /       POWER: ∫(1÷((X-1)^2))dX = ∫(1÷U^2)dU, U=X-1 /       CHECK: d/dX(-1÷(X-1)) = 1÷(X-1)^2 /       POWER: ∫(1÷(X-1)^2)dX = -1÷(X-1) /     CONSTANT MULTIPLE: ∫(2÷(X-1)^2)dX = -2÷(X-1) /     CONSTANT MULTIPLE: ∫(2÷(X-1)^3)dX = 2×∫(1÷(X-1)^3)dX /       POWER: ∫(1÷((X-1)^3))dX = ∫(1÷U^3)dU, U=X-1 /       CHECK: d/dX(-0.5÷(X-1)^2) = 1÷(X-1)^3 /       POWER: ∫(1÷(X-1)^3)dX = -0.5÷(X-1)^2 /     CONSTANT MULTIPLE: ∫(2÷(X-1)^3)dX = -1÷(X-1)^2 /   SUM: ∫(1÷(X-1)+2÷(X-1)^2+2÷(X-1)^3)dX = LN(ABS(X-1))-1÷(X-1)^2-2÷(X-1) / PARTIAL FRACTIONS: ∫((X^2+1)÷(X-1)^3)dX = LN(ABS(X-1))-1÷(X-1)^2-2÷(X-1)` |
| `casim steps integrate '(X+3)÷(X^2+2×X+5)' X RAD` | `SPLIT: ∫((X+3)÷(X^2+2×X+5))dX = 1÷2×∫((2×X+2)÷(X^2+2×X+5))dX+2×∫(1÷(X^2+2×X+5))dX /   LOG DERIVATIVE: ∫((2×X+2)÷(X^2+2×X+5))dX = ∫(1÷U)dU, U=X^2+2×X+5 /   CHECK: d/dX(LN(2×X+X^2+5)) = (2×X+2)÷(X^2+2×X+5) /   LOG DERIVATIVE: ∫((2×X+2)÷(X^2+2×X+5))dX = LN(2×X+X^2+5) /   COMPLETE THE SQUARE: X^2+2×X+5=(X+1)^2+4 /   ARCTANGENT: ∫(1÷(X^2+2×X+5))dX = ∫(1÷(U^2+4))dU, U=X+1 /   CHECK: d/dX(0.5×ATAN(0.5×X+0.5)) = 1÷(X^2+2×X+5) /   ARCTANGENT: ∫(1÷(X^2+2×X+5))dX = 0.5×ATAN(0.5×X+0.5) / SPLIT: ∫((X+3)÷(X^2+2×X+5))dX = 0.5×LN(2×X+X^2+5)+ATAN(0.5×X+0.5)` |
| `casim steps integrate '1÷(X^2+4)' X DEG` | `CHECK: d/dX(π×ATAN(0.5×X)÷360) = 1÷(X^2+4) / ARCTANGENT: ∫(1÷(X^2+4))dX = π×ATAN(0.5×X)÷360` |
| `casim --one-line steps integrate '4×X÷(X^4-1)' X RAD` | `FACTOR: X^4-1=(X+1)×(X-1)×(X^2+1) /   FORM: 4×X÷(X^4-1)=A÷(X+1)+B÷(X-1)+(C×X+D)÷(X^2+1) /   CLEARED: 4×X=A×(X-1)×(X^2+1)+B×(X+1)×(X^2+1)+(C×X+D)×(X+1)×(X-1) /     COVER-UP: A=1 /     COVER-UP: B=1 /     COEFFICIENTS: C=-2; D=0 /   ANSWER: 1÷(X+1)+1÷(X-1)-2×X÷(X^2+1) /     CHECK: d/dX(LN(ABS(X+1))) = 1÷(X+1) /     RECIPROCAL: ∫(1÷(X+1))dX = LN(ABS(X+1)) /     CHECK: d/dX(LN(ABS(X-1))) = 1÷(X-1) /     RECIPROCAL: ∫(1÷(X-1))dX = LN(ABS(X-1)) /       CHECK: d/dX(LN(X^2+1)) = 2×X÷(X^2+1) /       LOG DERIVATIVE: ∫(2×X÷(X^2+1))dX = LN(X^2+1) /     CONSTANT MULTIPLE: ∫(-2×X÷(X^2+1))dX = -LN(X^2+1) /   SUM: ∫(1÷(X+1)+1÷(X-1)-2×X÷(X^2+1))dX = LN(ABS(X-1))+LN(ABS(X+1))-LN(X^2+1) / PARTIAL FRACTIONS: ∫(4×X÷(X^4-1))dX = LN(ABS(X-1))+LN(ABS(X+1))-LN(X^2+1)` |

`steps solve "<text>" X` pages solve's working (work/steps-solve.md). Each line at the left is the equation as it
stands, `…=0`, and each indented line is one factor set to 0, `F=0: X=…`. The polynomials print as a student writes
them, in descending powers. The steps are:
- BOTH SIDES (or POLYNOMIAL for an expression) moves everything to one side and multiplies it out; this is left out
  when the equation was typed that way;
- INTEGER COEFFICIENTS multiplies or divides by what makes the coefficients whole with no common factor, the first
  one positive;
- COMMON FACTOR takes out the root 0's factor X^k;
- LINEAR FACTOR and QUADRATIC FACTOR divide out one factor, to its multiplicity: the division, then the product so
  far (left out when what is left is that factor itself). A factor is found numerically and proven by this division,
  which is exact;
- then each factor = 0, in the order root 0, linear factors ascending by root, quadratics: LINEAR (X^k=0 for the
  root 0 when k > 1; X alone has no line, the product above shows it); QUADRATIC FORMULA, the formula's values (or
  b^2-4ac when it is negative) then the roots, one line when the values are the roots already; LEFT for a factor of
  degree 3 or more, as (F)^k when it is a power;
- ANSWER is solve's own answer.

| Command | Prints (lines joined by " / ") |
|---|---|
| `casim steps solve "X^2÷2-X÷3=1" X RAD` | `BOTH SIDES: X^2÷2-X÷3-1=0 / INTEGER COEFFICIENTS: (X^2÷2-X÷3-1)×6=0 / INTEGER COEFFICIENTS: 3×X^2-2×X-6=0 /   QUADRATIC FORMULA: 3×X^2-2×X-6=0: X=(2-SQRT(76))÷6; X=(2+SQRT(76))÷6 /   QUADRATIC FORMULA: 3×X^2-2×X-6=0: X=(1-SQRT(19))÷3; X=(1+SQRT(19))÷3 / ANSWER: SOLVED: X=(1-SQRT(19))÷3; X=(1+SQRT(19))÷3` |
| `casim steps solve "X^3-3×X+2" X RAD` | `LINEAR FACTOR: (X^3-3×X+2)÷(X+2)=X^2-2×X+1 / LINEAR FACTOR: (X+2)×(X^2-2×X+1)=0 / LINEAR FACTOR: (X^2-2×X+1)÷(X-1)^2=1 / LINEAR FACTOR: (X+2)×(X-1)^2=0 /   LINEAR: X+2=0: X=-2 /   LINEAR: X-1=0: X=1 (MULT 2) / ANSWER: SOLVED: X=-2; X=1 (MULT 2)` |
| `casim steps solve "X^3-X" X RAD` | `COMMON FACTOR: X×(X^2-1)=0 / LINEAR FACTOR: (X^2-1)÷(X+1)=X-1 / LINEAR FACTOR: X×(X+1)×(X-1)=0 /   LINEAR: X+1=0: X=-1 /   LINEAR: X-1=0: X=1 / ANSWER: SOLVED: X=-1; X=0; X=1` |
| `casim steps solve "(X+3)×(X^3-2×X-5)" X RAD` | `POLYNOMIAL: X^4+3×X^3-2×X^2-11×X-15=0 / LINEAR FACTOR: (X^4+3×X^3-2×X^2-11×X-15)÷(X+3)=X^3-2×X-5 / LINEAR FACTOR: (X+3)×(X^3-2×X-5)=0 /   LINEAR: X+3=0: X=-3 /   LEFT: X^3-2×X-5=0: DEGREE 3 LEFT / ANSWER: PARTIAL, DEGREE 3 LEFT: X=-3` |
| `casim steps solve "X^4+X^2=0" X RAD` | `COMMON FACTOR: X^2×(X^2+1)=0 /   LINEAR: X^2=0: X=0 (MULT 2) /   QUADRATIC FORMULA: X^2+1=0: 0^2-4×1×1=-4<0 /   QUADRATIC FORMULA: X^2+1=0: NO REAL ROOTS / ANSWER: SOLVED: X=0 (MULT 2)` |
| `casim steps solve "X+1=X+2" X RAD` | `BOTH SIDES: -1=0 / ANSWER: NO SOLUTION` |
| `casim steps solve "(X^2-2)^2" X RAD` | `POLYNOMIAL: X^4-4×X^2+4=0 / QUADRATIC FACTOR: (X^4-4×X^2+4)÷(X^2-2)^2=1 / QUADRATIC FACTOR: (X^2-2)^2=0 /   QUADRATIC FORMULA: X^2-2=0: X=-SQRT(8)÷2; X=SQRT(8)÷2 /   QUADRATIC FORMULA: X^2-2=0: X=-SQRT(2) (MULT 2); X=SQRT(2) (MULT 2) / ANSWER: SOLVED: X=-SQRT(2) (MULT 2); X=SQRT(2) (MULT 2)` |
| `casim steps solve "(X-1)×(X^3+X+1)^2" X RAD` | `POLYNOMIAL: X^7-X^6+2×X^5-X^3+X^2-X-1=0 / LINEAR FACTOR: (X^7-X^6+2×X^5-X^3+X^2-X-1)÷(X-1)=(X^3+X+1)^2 / LINEAR FACTOR: (X-1)×(X^3+X+1)^2=0 /   LINEAR: X-1=0: X=1 /   LEFT: (X^3+X+1)^2=0: DEGREE 6 LEFT / ANSWER: PARTIAL, DEGREE 6 LEFT: X=1` |
| `casim --one-line steps solve "X^3-3×X+2" X RAD` | `LINEAR FACTOR: (X+2)×(X^2-2×X+1)=0 / LINEAR FACTOR: (X+2)×(X-1)^2=0 /   LINEAR: X+2=0: X=-2 /   LINEAR: X-1=0: X=1 (MULT 2) / ANSWER: SOLVED: X=-2; X=1 (MULT 2)` |

`steps partfrac "<n÷d>" X` pages partfrac's working (work/steps-partfrac.md), as a student writes it, with the
polynomials multiplied out in descending powers. The steps are:
- COMMON FACTOR: the gcd of n and d out, with cancel's rows (left out when it is 1);
- POLYNOMIAL PART: the division n÷d = q + r÷d (left out when n÷d is proper);
- FACTOR: d as its factors over Q, in partfrac's order (left out when d is one factor);
- FORM: r÷d as an unknown numerator over each factor's powers, the letters A, B, … skipping the variable;
- CLEARED: the form multiplied by d;
- COVER-UP, indented: each linear factor's root put in the cleared form, which leaves only its highest power's
  unknown, then its value;
- COEFFICIENTS, indented: what cover-up left, by equating the coefficients of each power of X (every power, so the
  last ones check the values), then their values;
- ANSWER is partfrac's own answer. A fraction that is already one factor to the first power goes straight to it.

Every page returns PARTIAL when partfrac does.

| Command | Prints (lines joined by " / ") |
|---|---|
| `casim steps partfrac "(X^4+1)÷(X^3+1)" X RAD` | `POLYNOMIAL PART: (X^4+1)÷(X^3+1)=X+(-X+1)÷(X^3+1) / FACTOR: X^3+1=(X+1)×(X^2-X+1) / FORM: (-X+1)÷(X^3+1)=A÷(X+1)+(B×X+C)÷(X^2-X+1) / CLEARED: -X+1=A×(X^2-X+1)+(B×X+C)×(X+1) /   COVER-UP: X=-1: 2=3×A /   COVER-UP: A=2÷3 /   COEFFICIENTS: X^2: -2÷3=B /   COEFFICIENTS: X: -1÷3=B+C /   COEFFICIENTS: X^0: 1÷3=C /   COEFFICIENTS: B=-2÷3; C=1÷3 / ANSWER: X+2÷(3×(X+1))-(2×X-1)÷(3×(X^2-X+1))` |
| `casim steps partfrac "(3×X+5)÷((X-3)×(X+4))" X RAD` | `FACTOR: X^2+X-12=(X+4)×(X-3) / FORM: (3×X+5)÷(X^2+X-12)=A÷(X+4)+B÷(X-3) / CLEARED: 3×X+5=A×(X-3)+B×(X+4) /   COVER-UP: X=-4: -7=-7×A /   COVER-UP: A=1 /   COVER-UP: X=3: 14=7×B /   COVER-UP: B=2 / ANSWER: 1÷(X+4)+2÷(X-3)` |
| `casim steps partfrac "(X^2-1)÷((X-1)×(X+2))" X RAD` | `COMMON FACTOR: (X^2-1)÷(X^2+X-2)=(X+1)÷(X+2); X=1 / POLYNOMIAL PART: (X+1)÷(X+2)=1-1÷(X+2) / ANSWER: 1-1÷(X+2); X=1` |
| `casim steps partfrac "(X+1)÷(X^2+1)" X RAD` | `ANSWER: (X+1)÷(X^2+1)` |
| `casim --one-line steps partfrac "(3×X+5)÷((X-3)×(X+4))" X RAD` | `FACTOR: X^2+X-12=(X+4)×(X-3) / FORM: (3×X+5)÷(X^2+X-12)=A÷(X+4)+B÷(X-3) / CLEARED: 3×X+5=A×(X-3)+B×(X+4) /   COVER-UP: A=1 /   COVER-UP: B=2 / ANSWER: 1÷(X+4)+2÷(X-3)` |

### pdiv, pgcd, cancel: polynomial division

The polynomials are read as solve reads them, with degree ≤ 12 and coefficients ≤ 10^7.
- **pdiv** prints `q; r` with f = q×g + r and deg r < deg g, checked exactly. Dividing by 0 is UNDEFINED.
- **pgcd** prints the greatest common divisor, primitive with a positive leading coefficient. It is accepted only
  when it divides both exactly.
- **cancel** takes `n÷d` and prints it reduced by the gcd, then each value the cancelling removed: `X=a` for a
  rational root of the gcd, and `p=0` for the rest of the gcd.

Statuses: UNDEFINED, NOT_POLYNOMIAL, NOT_NUMERIC, NO_ROOM, BAD_ARG, TOO_LONG.

| Command | Prints |
|---|---|
| `casim pdiv "X^3+2" X "X-1" RAD` | `X+X^2+1; 3` |
| `casim pdiv "X^2+1" X "0" RAD` | `UNDEFINED` |
| `casim pgcd "X^3-X" X "X^2+2×X+1" RAD` | `X+1` |
| `casim cancel "(X^2-1)÷(X^2+2×X+1)" X RAD` | `(X-1)÷(X+1); X=-1` |
| `casim cancel "(X^4-4)÷(X^2-2)" X RAD` | `X^2+2; X^2-2=0` |
| `casim cancel "0÷(X+1)" X RAD` | `0; X=-1` |

### partfrac: partial fractions over the rationals

partfrac reads `n÷d` as cancel does (or a polynomial) and writes it as a polynomial part and a sum of proper fractions
over d's factors over Q (work/012-partial-fractions.md):
- a common factor of n and d comes out first, and the values it removed follow as cancel's rows (`; X=1`);
- the polynomial part comes first, descending, each coefficient a whole number or p÷q after its power (`3×X÷2+9÷2`);
- then each factor F of d to the power k, as a term over F^j for j = 1 to k, ascending: `c×(N)÷(m×(F)^j)`, with N a
  primitive integer polynomial of degree below F's, its lead positive, in parentheses only when it is a sum, the sign
  out front, and c and m left out when 1;
- the factors in order of their roots: the linear ones by root ascending (X is the root 0, in its place among them),
  then the quadratics by their coefficients (a, b, c), then the rest;
- the answer is checked exactly against n÷d before it prints (n = q×d + r, and each factor's numerators against r);
  a factor of degree 4 or more that factor cannot prove
  irreducible gives PARTIAL, with the answer still exact over the factors found.

It takes the same bounds as cancel. Statuses: PARTIAL, UNDEFINED, NOT_POLYNOMIAL, NOT_NUMERIC, NO_ROOM (also when the
exact arithmetic passes 2^62), BAD_ARG, TOO_LONG.

| Command | Prints |
|---|---|
| `casim partfrac "(3×X+5)÷((X-3)×(X+4))" X RAD` | `1÷(X+4)+2÷(X-3)` |
| `casim partfrac "(X^4+1)÷(X^3+1)" X RAD` | `X+2÷(3×(X+1))-(2×X-1)÷(3×(X^2-X+1))` |
| `casim partfrac "(7×X^2+9×X-1)÷(3×X-2)^4" X RAD` | `7÷(9×(3×X-2)^2)+55÷(9×(3×X-2)^3)+73÷(9×(3×X-2)^4)` |
| `casim partfrac "(X^2-1)÷((X-1)×(X+2))" X RAD` | `1-1÷(X+2); X=1` |
| `casim partfrac "1÷(1-X^4)" X RAD` | `1÷(4×(X+1))-1÷(4×(X-1))+1÷(2×(X^2+1))` |

### taylor: a Taylor polynomial

`taylor "<text>" X A N` is the Taylor polynomial of the text in X about the point A, to order N (0 to 12;
work/014-taylor.md): c_0 + c_1×(X-A) + … + c_N×(X-A)^N, each c_k = f^(k)(A)÷k! exact (derive's derivatives, valued at A
as a definite integral's bound is). It prints in ascending powers of (X-A), or of X when A is 0, each term in canonical
form, a zero term left out and a coefficient of 1 not written; 0 when every term is 0. A is a real constant read as
defint's bounds (π÷3, -2, SQRT(2)); other letters in the text are constants. DEG and GRAD carry their factor, as derive
does.
Statuses: UNDEFINED (the text or one of its derivatives up to N has no value at A: a removable singularity such as
SIN(X)÷X at 0, a function outside its domain such as LN(X) at 0 or X×LN(X) at 0, a fractional power of a negative
number, TAN at its poles, or ABS's corner, so ABS(X) at 0 has no first derivative), UNDECIDED (an SGN(0) it can't
decide, below), NOT_DIFFERENTIABLE for a name derive refuses (X!, IP), NO_ROOM (a derivative outgrows the arena, or its
text the buffer between steps: a nested composition at a high order), TOO_LONG, and
BAD_ARG (an equation, a letter in A, or N outside 0 to 12). The typed text decides UNDEFINED: X÷X at 0 is UNDEFINED,
though its canonical form is 1.

ABS's derivative is SGN, which has no value where its argument is 0 (CAS 016). SGN is bounded, so beside a factor that
is 0 there it is 0: X×ABS(X)'s derivative ABS(X)+X×SGN(X) is 0 at 0, and order 1 answers 0. Its second derivative,
2×SGN(X), jumps at 0, so order 2 is UNDEFINED, as ABS(X) is at order 1: a lone SGN(u), u changing sign at A, is a jump.
Any other SGN(0) is UNDECIDED, never a wrong answer: ABS(X)^2 is X^2, but its second derivative holds SGN(X)^2;
K×ABS(X) depends on whether K is 0; and ABS(X)-SQRT(X^2), which is 0, has a second hole beside the corner that cancels
it. In the same way a 0÷0 that appears only in a derivative's formula says that formula can't tell, and is UNDECIDED:
X×SQRT(X^2)'s derivative holds X^2×(X^2)^(-1÷2) at 0, where the true f'(0) is 0. A c÷0 with c not 0 is proof:
X^(3÷2) to order 2 (0.75×X^(-1÷2) at 0) is UNDEFINED, and so is any hole in the typed text (X÷X). UNDECIDED is sound but
can be less than the truth: SQRT(X^2) to order 1 is UNDECIDED, though it is ABS(X) and has no first derivative at 0, and
ABS(X)^3 or ABS(ATAN(X))^2 to order 2 are UNDECIDED, though they have one. A later unit composes power series term by
term instead of taking derivatives, and these are on its list.

Each derivative is larger than the last, and nested functions grow fastest. In the calculator's 8,192-byte arena, TAN,
ASIN, LN(COS(X)), 1÷(1+X^2) and EXP(X^2) reach order 12; EXP(SIN(X)) stops after order 7 and SIN(COS(X)) after 6, and
past that the answer is NO_ROOM. A long answer is TOO_LONG first: COS(X) about π÷3 carries SQRT(3) in every other term.

Between steps each derivative is kept as text, in one of two buffers at the arena's top. They are 384 bytes each, and
1,024 in an arena of 16,384 bytes or more (CAS 016): the host and the web library, whose arena is 65,536, not the
calculator. EXP(SIN(X))'s eighth derivative is 416 bytes as text, so it reaches order 9 in a 16,384-byte arena and not
in 16,383. On the calculator both buffers come out of its 8,192 bytes, and a bigger buffer answers fewer: measured on
600 seeded cases of each kind (casim #7916, on 4b91739), cases answered:

| Cases (600 each) | 8,192 B, buffers 384 / 768 / 1,024 | 65,536 B, buffers 384 / 768 / 1,024 |
|---|---|---|
| taylor | 496 / 486 / 478 | 521 / 557 / 567 |
| limit | 499 / 499 / 499 | 514 / 514 / 514 |
| 0÷0 compositions (limit) | 342 / 342 / 342 | 359 / 359 / 359 |

Every NO_ROOM at 8,192 bytes is the arena's. Repeated derivatives grow faster than the buffers: of taylor's 104, 33
don't answer even in 1 MiB with 1,024-byte buffers (X^3 inside, order 9 or 10).

| Command | Prints |
|---|---|
| `casim taylor 'SIN(X)' X 0 7 RAD` | `X-X^3÷6+X^5÷120-X^7÷5040` |
| `casim taylor 'LN(X)' X 1 4 RAD` | `(X-1)-0.5×(X-1)^2+(X-1)^3÷3-0.25×(X-1)^4` |
| `casim taylor 'COS(X)' X 'π÷3' 3 RAD` | `0.5-0.5×(X-π÷3)×SQRT(3)-0.25×(X-π÷3)^2+(X-π÷3)^3×SQRT(3)÷12` |
| `casim taylor '1÷(1-X)' X -2 3 RAD` | `1÷3+(X+2)÷9+(X+2)^2÷27+(X+2)^3÷81` |
| `casim taylor 'SIN(X)' X 0 3 DEG` | `X×π÷180-X^3×π^3÷34992000` |
| `casim taylor 'EXP(K×X)' X 0 3 RAD` | `1+K×X+0.5×K^2×X^2+K^3×X^3÷6` |
| `casim taylor 'SIN(X)÷X' X 0 4 RAD` | `UNDEFINED` |
| `casim taylor 'X÷X' X 0 2 RAD` | `UNDEFINED` |
| `casim taylor 'EXP(SIN(X))' X 0 8 RAD` | `NO_ROOM` |
| `casim taylor 'X×ABS(X)' X 0 1 RAD` | `0` |
| `casim taylor 'X×ABS(X)' X 0 2 RAD` | `UNDEFINED` |
| `casim taylor 'ABS(X)^2' X 0 2 RAD` | `UNDECIDED` |

### limit: a limit at a point or at ±∞

`limit "<text>" X A S` is the limit of the text as X goes to A (work/015-limits.md). A is a real constant read as
defint's bounds (0, 1÷2, π÷3, -2), or `∞` or `-∞`. S is the side: `+` from above, `-` from below, or `''` (empty) for
both, which must agree; at ∞ and -∞ the side is implied and S is `''`. The answer is exact, in canonical form (EXP(1),
π÷2, -1÷6, 0.5), or `∞` or `-∞`; other letters in the text are constants. DEG and GRAD carry their factor, as derive
does: ATAN(X) at ∞ is 90 in DEG.

Where every part of the text has a value at A, that's the limit, even where the typed text as a whole has none: X÷X
at 0 is 1 (taylor answers UNDEFINED there; it answers about f where f is defined, and limit answers the removable
question). A part with no value isn't continuity even when the whole folds it away: (X-π÷2)×TAN(X) at π÷2 is -1, not
0. Otherwise the walk
takes the text by its parts: sums, products and powers by their parts' limits; functions by continuity and by their
ends (LN(0+) is -∞, EXP(-∞) is 0); 0÷0 and ∞÷∞ by L'Hôpital, at most 12 steps; 0×∞ as a quotient; B^E with X in E as
EXP(E×LN(B)); at ∞ a polynomial by its leading term; an ∞-∞ with a square root by its conjugate; at a point, an
∞-∞ that neither term dominates over one denominator (CAS 016: 1÷X-1÷SIN(X) is (SIN(X)-X)÷(X×SIN(X)), then
L'Hôpital, which there keeps the numerator and denominator apart). The side near A comes
from the first derivative that isn't 0 there, so 1÷X at 0 is ∞ from above, -∞ from below, and has no two-sided limit.

Statuses: UNDEFINED means there is no limit, and only where that's proven: the one-sided limits differ (1÷X at 0), or
SIN, COS or TAN of an argument going to ±∞ (SIN(X) at ∞, SIN(X)+1, 2×SIN(X), COS(1÷X) at 0). UNDECIDED means the walk
couldn't decide: more than 12 L'Hôpital steps, an ∞-∞ the rewrites don't resolve, a damped oscillation (SIN(X)÷X at ∞
is 0, but by the squeeze theorem, which the walk doesn't have), or a form it doesn't cover. NOT_DIFFERENTIABLE: a name
derive refuses (X!, IP), since L'Hôpital derives. BAD_ARG: an equation, a letter in A, a side other than `+`, `-` and
`''`, or a side at ∞.

| Command | Prints |
|---|---|
| `casim limit 'SIN(X)÷X' X 0 '' RAD` | `1` |
| `casim limit '(SIN(X)-X)÷X^3' X 0 '' RAD` | `-1÷6` |
| `casim limit '1÷X' X 0 + RAD` | `∞` |
| `casim limit '1÷X' X 0 '' RAD` | `UNDEFINED` |
| `casim limit 'X×LN(X)' X 0 + RAD` | `0` |
| `casim limit '(2×X^2+1)÷(3×X^2-X)' X ∞ '' RAD` | `2÷3` |
| `casim limit 'SQRT(X^2+X)-X' X ∞ '' RAD` | `0.5` |
| `casim limit '(1+3÷X)^X' X ∞ '' RAD` | `EXP(3)` |
| `casim limit 'ATAN(X)' X -∞ '' DEG` | `-90` |
| `casim limit 'TAN(X)' X 'π÷2' - RAD` | `∞` |
| `casim limit 'SIN(X)' X ∞ '' RAD` | `UNDEFINED` |
| `casim limit 'SIN(X)÷X' X ∞ '' RAD` | `UNDECIDED` |
| `casim limit '1÷X-1÷SIN(X)' X 0 '' RAD` | `0` |
| `casim limit '1÷LN(1+X)-1÷X' X 0 '' RAD` | `0.5` |

## The statuses

| Value | Status | Means | out holds |
|---|---|---|---|
| 0 | OK | the answer | the answer |
| 1 | SYNTAX | the text cannot be read | the character offset of the first error |
| 2 | NOT_DIFFERENTIABLE | derive met a name it cannot differentiate (or subst would copy a draw) | the name |
| 3 | TOO_LONG | the answer does not fit: past cap, or past 191 bytes where the operation has that bound | "" |
| 4 | BAD_ARG | an argument out of range, or an equation where an expression is needed | "" |
| 5 | NO_ROOM | the arena ran out, the depth guard stopped a walk, or a bound (degree, coefficient) was passed | "" |
| 6 | NOT_POLYNOMIAL | solve, factor and the polynomial operations (partfrac too): not a polynomial in var as typed | "" |
| 7 | NOT_NUMERIC | the same: a coefficient that is not an exact rational | "" |
| 8 | NOT_INTEGRABLE | integrate and defint: not a form in the table, or the check did not close | "" |
| 9 | PARTIAL | factor: a correct factorization with one factor unproven | the factorization |
| 10 | UNDEFINED | an exact 0 under a negative power, a division by 0, a pole (TAN(π÷2), LN(0)), a bound with no value | "" |
| 11 | IMPROPER | defint: a pole or domain edge on the interval | "" |
| 12 | UNDECIDED | limit: the walk couldn't decide (past 12 L'Hôpital steps, a damped oscillation, a form not covered); never "no limit". taylor: an SGN(0) it can't decide | "" |

## The bounds

- **191 bytes.** On the calculator an equation slot holds 191 bytes and its NUL. Every operation except derive and
  simplify returns TOO_LONG past 191 bytes, whatever cap the caller gives. derive and simplify fill the caller's cap,
  and so does a step line (a solve's answer, its last step, still stops at 191 bytes, as solve's does).
- **The arena.** The calculator gives Casimir 8 KiB, and the visible examples are measured there. A larger arena
  answers more (the command line's default is 8,192; `--arena` changes it). At most 524,280 bytes of it are used.
- **The depth guard.** Nesting past 44 levels is NO_ROOM, so the stack Casimir uses is bounded: at most 11,328
  bytes on the ESP32-S3, read with interrupts masked, plus an allowance of 384 for interrupts (two 192-byte frames;
  unmasked readings have exceeded masked ones by up to 272 bytes, 270 readings of 10 cases on the bench, for a reason
  not yet found). A task of 12 KB holds both with 576 bytes to spare.
- **Polynomials.** solve, factor, pdiv, pgcd, cancel and partfrac take degree ≤ 12 and coefficients ≤ 10^7 once the polynomial
  is cleared to integers.
- **Numbers.** Exact rationals with numerator and denominator below 2^62. Past that, a number is kept as typed and
  not folded.
