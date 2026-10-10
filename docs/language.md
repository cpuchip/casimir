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
- NO_ROOM when the arena runs out, or when nesting passes the depth guard (46 levels);
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
power: the expression has no value).

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

## The statuses

| Value | Status | Means | out holds |
|---|---|---|---|
| 0 | OK | the answer | the answer |
| 1 | SYNTAX | the text cannot be read | the character offset of the first error |
| 2 | NOT_DIFFERENTIABLE | derive met a name it cannot differentiate (or subst would copy a draw) | the name |
| 3 | TOO_LONG | the answer does not fit: past cap, or past 191 bytes where the operation has that bound | "" |
| 4 | BAD_ARG | an argument out of range, or an equation where an expression is needed | "" |
| 5 | NO_ROOM | the arena ran out, the depth guard stopped a walk, or a bound (degree, coefficient) was passed | "" |
| 6 | NOT_POLYNOMIAL | solve, factor and the polynomial operations: not a polynomial in var as typed | "" |
| 7 | NOT_NUMERIC | the same: a coefficient that is not an exact rational | "" |
| 8 | NOT_INTEGRABLE | integrate and defint: not a form in the table, or the check did not close | "" |
| 9 | PARTIAL | factor: a correct factorization with one factor unproven | the factorization |
| 10 | UNDEFINED | an exact 0 under a negative power, a division by 0, a bound with no value | "" |
| 11 | IMPROPER | defint: a pole or domain edge on the interval | "" |

## The bounds

- **191 bytes.** On the calculator an equation slot holds 191 bytes and its NUL. Every operation except derive and
  simplify returns TOO_LONG past 191 bytes, whatever cap the caller gives. derive and simplify fill the caller's cap.
- **The arena.** The calculator gives Casimir 8 KiB, and the visible examples are measured there. A larger arena
  answers more (the command line's default is 8,192; `--arena` changes it). At most 524,280 bytes of it are used.
- **The depth guard.** Nesting past 46 levels is NO_ROOM, so the stack Casimir uses is bounded: at most 11,808
  bytes on the ESP32-S3, plus one interrupt frame (192 bytes there) that a tick saves on the running task's stack.
  A task of 12 KB holds both with 288 bytes to spare.
- **Polynomials.** solve, factor, pdiv, pgcd and cancel take degree ≤ 12 and coefficients ≤ 10^7 once the polynomial
  is cleared to integers.
- **Numbers.** Exact rationals with numerator and denominator below 2^62. Past that, a number is kept as typed and
  not folded.
