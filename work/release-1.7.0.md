Casimir 1.7.0: solve refuses an equation whose coefficient of the variable is 0 where canon can't see it, in place of 1.6.0's SOLVED with a row that never holds. `(SIN(A)^2+COS(A)^2-1)×X=1` is NOT_NUMERIC, as `A×X-A×X=1` already was.

## What's new since 1.6.0

- **A coefficient that is 0 where canon can't see it** (CAS 040). Before a letter solve answers, X's coefficient is read at six fixed points, each letter its own value (A takes 1/5, -2/3, -9/4, 19/3, 147/4 and -75/2), by the bound every printed sign rests on (CAS 022 rule 8). Shown not 0 at a point, the equation answers as at 1.6.0. Read 0 at each point where it has a value, and not 0 at none, it is refused NOT_NUMERIC, never taken as 0 from its digits: `(EXP(A+B)-EXP(A)×EXP(B))×X=C`, `(TAN(A)×COS(A)-SIN(A))×X=1` in GRAD and `(LN(A^2)-2×LN(A))×X=1` are each NOT_NUMERIC, and show steps refuses them the same way. A coefficient that isn't 0 keeps its answer: `(SIN(A)^2+COS(A)^2)×X=1` is `SOLVED: X=1÷(SIN(A)^2+COS(A)^2); SIN(A)^2+COS(A)^2≠0`, and `(ATAN(1÷A)+ATAN(A)-π÷2)×X=1`, 0 only for A>0, keeps its line. work/cas040.md has the points, the readings and the shapes refused though not 0.
- **What moves**, against 1.6.0:
  - the golden corpus: none of its 10,822 lines;
  - the documented examples: none of 1.6.0's 1,323 commands;
  - the developer tests: none of tests/solve_test.c's 416 earlier checks;
  - the reviewer's hidden sets of the earlier units: 27,858 records byte for byte, but the Pythagorean identity row of CAS 038's and CAS 039's sets, now NOT_NUMERIC.
- **Measured:** S3 code 270,410 bytes (605 more than 1.6.0's), the first release past decision 92's 270,336 B line, inside decision 94's 301,056 B (Michael's pre-approved 30 KiB raise). No accepted function's frame moved, on the host or the S3. On the ESP32-S3 the bench passes at this tag, and its worst stack is unchanged (11,360 B masked).

**The guarantee is unchanged:** an exact answer, or a status that says why not. Never an approximation presented as an answer.

## The known wrong answers

None. CAS 040's hidden set found no wrong value, on its first run.

## How it's tested (at this tag)

- `make test`: 20 programs with 118,633 checks. They read every answer back through an independent evaluator (the calculator firmware's core) and compare values. New in 1.7.0: the identities behind the refused rows are checked to be 0 on the core at letter values of each sign, and the rows the zero test must leave alone print their 1.6.0 lines.
- The golden corpus, 10,822 calls compared byte for byte; none moved.
- Frame and symbol gates: no frame over 1 KiB, no `pow` linked, and every function's frame against the recorded table. No frame moved from 1.6.0, on the host or the S3 (every row diffed). The new function takes 96 B on the S3 (ls_zero).
- Value checks beside the corpus judge Taylor polynomials, limits, integrals, sums and solve's steps against independent constructions.
- The docs: every `casim` row of docs/language.md and the units' example files is run (2,860 checks, each again with the series route compiled out), and every backticked answer in docs/language.md's prose must be in one of them or in the golden corpus.
- Two fuzzes, against 1.6.0's command line (scripts/zero-diff.py, scripts/zero-plant.py): 20,000 random letter equations, none printing differently; 4,000 with an identity planted in the coefficient, all 2,057 zero ones refused and all 1,943 others as at 1.6.0.
- CI on every push: `make check` under gcc and clang, `make test`, CMake under gcc and clang, WebAssembly, and the ESP-IDF component for the ESP32-S3.
- Mutants (scripts/mutants.py): 588 deliberate faults. The run at this tag's tree (8a76a80) is under way, and its count goes here when it ends (work/v1.7.0-mutants.txt). CAS 040's 12 and the three CAS 038 mutants it re-anchored are caught at its accepted tree (69b97e6; work/cas040.md).
- Hidden sets kept by the project's reviewer, independent of this repository, rerun at each unit's acceptance. At CAS 040's: its own, 223 rows at two arenas, 0 of 446 runs failing, and the earlier units'.
- On the ESP32-S3, at this tag's tree (8a76a80), every one of the bench's 1,454 cases is byte-identical to the host, and the steps of the 297 derivatives, integrals, solves and partial fractions page as the host's:
  - with a 16,384 B arena, the size the calculator runs it with;
  - worst stack 11,360 B read with interrupts masked, plus a 400 B interrupt allowance (two frames, or the most measured over the masked reading where that is larger), 11,760 of a 12 KiB task;
  - nesting deeper than 44 levels is refused (NO_ROOM);
  - about 270 KB of Casimir's code at -O2 (270,410 B), 30,646 B under decision 94's 301,056 B line.

## Not yet, after 1.7.0

These print a line without the note it could have, print a row the answer doesn't need, have no value where the sum has one, refuse, or print a constant as written. None of them is a wrong value.

New with the zero test (CAS 040), each a refusal or 1.6.0's line, never a wrong value:

- **Refused, though X's coefficient isn't 0.** The coefficient reads 0 at every one of the six points where it has a value: 0 on a region holding them all (`(ABS(A+40)-(A+40))×X=1`, 0 for A≥-40, whose 1.6.0 answer held for A<-40); a factor built to vanish at the points; a part under about 1E-31 of its terms (`(SIN(A)^2+COS(A)^2-1+10^-40)×X=1`); or read at one point only, 0 there (`(4×A-147)×SQRT(A-30)×X=1`, which has a value only at A = 147/4 among the points).
- **A 0 coefficient no point reads prints 1.6.0's line,** SOLVED with a row that never holds: functions the bound doesn't read (`(IP(A)+FP(A)-A)×X=1`, `(10^LOG(A)-A)×X=1`), and a domain holding no point (`(2×LN(A-60)-LN((A-60)^2))×X=1`).
- **canon doesn't multiply out (A-B)×(A+B) where a sum holds it:** `(A^2-(A-B)×(A+B)-B^2)×X=1` printed SOLVED at 1.6.0, and 1.7.0 refuses it only because the point test reads its 0. canon's form is unchanged.

Carried from 1.6.0, each example run again on this tag's command line, where it prints as at 1.6.0. With letters as coefficients (CAS 038 and 039):

- **canon's forms, not a student's:** `(X+A)÷B=C` is `X=B×(C-A÷B); B≠0`, `A×X+B=C` is `X=-(B-C)÷A; A≠0`, `F=9÷5×C+32` for C is `C=5×F÷9-160÷9`, and steps' BOTH SIDES for `V=I×R` is `-I×R+V=0`.
- **A row in the expanded coefficient's form:** `A=(B+C)×H÷2` for H has `-B÷2-C÷2≠0`, and `(A×B-A)×X=B` has `-A+A×B≠0`.
- **A row that always holds, or one left unsolved:** `(SQRT(A)+1)×(X+1)=B` has `SQRT(A)+1≠0`, `(SQRT(A)-1)×X=1` has `SQRT(A)-1≠0` (A≠1), `ACOSH(A)×X=1` has `A≥1; ACOSH(A)≠0` (A>1), and `(A^3-1)×X=1` has `A^3-1≠0`.
- **Refused, NOT_NUMERIC as 1.5.0 refuses them:** var dropping out (`A×(X-1)=A×X`), degree 2 or more with a letter (`X^2=A`), var inside a function or a denominator (`A×SIN(X)=B`, `A÷X=1`), a condition no row writes (ASIN's ABS(u)≤1, ATANH's ABS(u)<1), model 33's variable i, a constant with neither a letter nor π (`SIN(1)×X=1`), and an answer where the typed text has no value (`X×(X-A)÷(X-A)=A`). pw's first refusal decides whether letters are read at all: `X^2÷X×A=1` is NOT_POLYNOMIAL, and `A×X^2÷X=1` is `SOLVED: X=1÷A; A≠0`. Past six conditions, NO_ROOM (`A×B×C×D×E×F×G×X=1`).

Carried from 1.5.0, printing as at 1.6.0:

- **The typed domain, stricter than it needs to be** (CAS 035):
  - limit's letter conditions at the edge: `SQRT(Y)^2×X` at 1 prints `Y; Y>0`, though Y = 0 has a value, and `ASIN(Y)×X` at 1 prints `ASIN(Y); ABS(Y)<1`, though |Y| = 1 has one;
  - `SQRT(X^2×Y)^2` at 0 gives `0; Y>0`, though Y = 0 has a value;
  - limit's degenerate divisor tries k = 0 only for k linear in one letter: `(SIN((Y-Z)×X)+1)÷(SIN((Y-Z)×X)+1)` at 0 prints `1; Y-Z≠0`, though at Y = Z it is 1 everywhere, and `Y×Z-1` and `Y^2-1` (`1; Y≠-1; Y≠1`) the same.
- **The typed domain, refused where it could be answered** (CAS 035):
  - an X×Y-form limit at ±∞ is UNDECIDED: its sign is Y's, and no row is given (`SQRT(X×Y)÷SQRT(X×Y)` at ∞);
  - a divisor 0 for every X at a letter's value that casim can't rule out is UNDECIDED: `(Y×SIN(X)-SIN(Y×X))÷(Y×SIN(X)-SIN(Y×X))` at 0, whose divisor is 0 for every X at Y = ±1;
  - rows count toward the 191 bytes, so some lines with them are TOO_LONG: `K×(1+Z×G)^K` from 1 to ∞, where G is u written as `SQRT(u)^2`, `(u^(1÷4))^4`, `EXP(LN(u))` or `ALOG(LOG(u))` and u is Y+1, Y-2 or W-1, and `(K-1)×(F(Z)×Y)^K÷F(Z)` from 1 to N for ACOSH, ASINH and ATANH, and LN from 2.
- **Older checks beside the typed rows** (found by 1.4.0's mutant search; 1.3.0 prints the same):
  - which refusal a constant with no value gets beside a pole on the interval: `X^(-2)×ACOS(3)` over 0..1 is IMPROPER, where UNDEFINED would be just as lawful (CAS 009's interval check answers first);
  - limit rows from the answer's own walk and f's (CAS 023 sites 6 and 7) that restate a typed row (`K×π+4≠0` beside `K×π÷2+2≠0`) or can't fail (`EXP(K^2)≠0`), and 69 limit lines found TOO_LONG where the line without site 6's rows fits.
- **A quotient that canon cancels, or whose divisor falls in K, is still refused.** `X^(2×K)÷X^K` is NO_CLOSED_FORM: canon reads it as `X^K`, which has a value at X = 0, where its terms are 0÷0. So is `X^K÷Y^(-K)`: its divisor has no value at Y = 0, while canon's `(X×Y)^K` has one. Answering them needs a row that `(X÷Y)^K`-style twins don't print (`X≠0`, `Y≠0`). So are `Y^K÷Y^K` (where `Y^K×Y^(-K)` prints `N; Y≠0`) and a divisor in K inside an exponent: `(-8)^(1÷(K-1))` from 0 is NO_CLOSED_FORM, though the term at K = 1 has no value (UNDEFINED would say why).
- **A divisor of degree 2 or more in K that canon cancels** is read by no CAS 037 rule. The shapes found are refused (`((K^2-1)×K)÷(K^2-1)` from 0 to N is NO_CLOSED_FORM) or UNDEFINED already.
- **UNDEFINED for a whole sum to N whose divisor is 0 mid-range** (CAS 037's (C), as for a divisor canon keeps): `((K+1)^2-1)÷((K+1)-1)` from -2 to N is UNDEFINED, though the sum has a value for N = -2 and -1, as `1÷(K-2)` from 0 to N is UNDEFINED, though the sum has one for N = 0 and 1.
- **A refusal that changes kind with the arena.** CAS 037's rows take arena, so a line too long at every arena can run out of room first at the calculator's: `(1+Z×(Y-1)^2)^(-K)` from -2 to N is NO_ROOM at 8,800 B and TOO_LONG at 16,384 B, where 1.4.0 was TOO_LONG at both.
- **The `÷` spelling reaches the rows of the `×` one.** A quotient prints the answer its canon spelling did in 1.1.0, rows for nothing and missing values included. At a letter's 0, a quotient's terms whose power of the letter is 0 or less have values (0^0 = 1), and there the letter's row excludes the sum's value. `X^K÷Y^K` from 0 to N prints `-1÷(X÷Y-1)+(X÷Y)^(N+1)÷(X÷Y-1); Y≠0; X÷Y≠1`, though at Y = 0 and N = 0 the sum is 1, as `X^K×Y^(-K)` printed in 1.1.0. `X^(K-1)÷Y^(K+2)` from 1 to N prints `X≠0`, though at X = 0 the sum is 1÷Y^3 for every N ≥ 1. Where the factor is one of CAS 030's functions of a letter, its row excludes the point now: `Y^K÷SIN(Z)^K` from 0 to N prints `SIN(Z)≠0`, though at Z = π and N = 0 the sum is 1 (rows for nothing, below). They're the class below, where the shift that clears a rational ratio's row is refused past a letter at a negative power.
- **A note that doesn't fit.** Where the line with the note would pass solve's 191 B, solve prints the line as 1.0.1 did, with no note, never TOO_LONG for it and with no root dropped. `(7×X-2)×(3×X^2-9×X-7)×(8×X^2+3×X-2)×(8×X-3)×(X^2-3×X+8)×(8×X^2+6×X-1)×(1×X^2+9×X+3)` prints its ten real roots in 184 B as `SOLVED: …`, and `, 2 CPLX` would make it 192 B. With `--complex` the same input is TOO_LONG. In a family of degree-12 polynomials built to reach the cap, 59 of 1,416 noted answers give way at one seed and 69 of 1,403 at another; none in the golden corpus does.
- **The complex roots of a factor of degree 3 or more** aren't counted, since solve doesn't find them, with the bit or without it: `X^4+1` is `PARTIAL, DEGREE 4 LEFT`.
- **Rows for nothing.** Each is excluded where the sum has a value, while the answer has none there:
  - CAS 036's rows, at the empty sum: `X^(-K)` from 1 to N prints `X≠0`, while at X = 0 and N = 0 the sum is 0;
  - from a LO of 0, CAS 036's row also excludes N = 0, where the one term is u^0 = 1: `X^K÷(Y+1)^K` from 0 to N prints `Y≠-1`, though at Y = -1 and N = 0 the sum is 1. CAS 037's rows do the same at N = LO where the exponent is 0 there: `Y^K×Y^(-K)` from 0 to N prints `N+1; Y≠0`, though at Y = 0 and N = 0 the sum is 1;
  - a CAS 037 row read from an exponent that falls toward negative excludes its point before it gets there: `Y^(2-K)×Y^K` from 0 to N prints `Y^2+N×Y^2; Y≠0`, though at Y = 0 the sum is 0 for N up to 2 (1.4.0 printed no row, and a value from N = 3, where the term is 0^(-1)×0^3);
  - a function factor's row to N, at an N before the first term that divides by the factor (CAS 030, as a letter's row): `(Y÷SIN(Z))^K` from 1 to N prints `SIN(Z)≠0`, which excludes Z = π at N = 0, where the sum is 0, as its twin's `X≠0` excludes X = 0. For every N from 1 the sum has no value there, and the row is the one it needs;
  - a divisor's row to N, at the empty sum: `(X÷Y)^K÷X` from 1 to N prints `X≠0`, which excludes X = 0 at N = 0, where the sum is 0. For every N from 1 each term divides by 0 there, and the row is the one it needs;
  - a row that excludes no point: casim doesn't know a polynomial's real zeros here, so to ∞ a divisor inside the ratio that has none gets its row all the same (CAS 033): `(Z÷(Y^2+1)+1)^K` from 0 to ∞ prints `Y^2+1≠0`, true everywhere;
  - past an irrational constant, to N, the shift that clears a rational ratio's row is refused: `X^(K-1)×(SQRT(2)×Y)^K` from 1 to N prints `…; X≠0; X×Y×SQRT(2)≠1`, though at X = 0 the sum is SQRT(2)×Y for every N from 1, and `X^(K-1)×(2×Y)^K` prints no `X≠0`. To ∞ it prints none.
- **A redundant row the rule doesn't reach.** Where the row beside it holds another letter under a division or inside a function with no value somewhere (TAN, LN, LOG, ASIN, ACOS, ACOSH, ATANH), the letter's row stays though it may exclude nothing more: `(1+TAN(Y)×Z)^K` from 0 to N keeps `Z≠0` beside `Z×TAN(Y)+1≠1`. A give-way row beside a hard letter's row prints both: `K×(SQRT(Z)×Y)^K÷SQRT(Z)` from 0 to N prints `Z≠0; Z≥0`, which `Z>0` says together.
- **A row less plain than the one it replaces.** Where a function's row says a letter's, the letter's goes: `(Y÷ASIN(Z))^K` from 1 to N prints `ABS(Z)≤1; ASIN(Z)≠0` in place of 1.4.0's `Z≠0`.
- **A missing value.** Each has no value at a point where the sum has one, and no row excludes that point. What CAS 036 leaves:
  - a line within its rows of the cap. The rows give way together, so a line with room for one keeps none: `K×Y^K÷TAN(Z)` from 1 to N is 170 B, with room for `TAN(Z)≠0` alone (182 B) but not with `COS(Z)≠0` too (194 B). `(K-1)×(COS(Z)×Y)^K÷COS(Z)` from 0 to N prints its 181 B line as 1.2.0 did, with no value at Z = π÷2 for N = -1, where the sum is 0;
  - an exponent linear in K that isn't K: `SIN(2×Z)^(2×K-1)×Y^K` from 0 to N has no value at Z = π÷2 for N = -1, where the sum is 0. The same holds from -2 and -1, and for 3×K-1;
  - the closed form's own hole, where the typed summand has none: `X^(K+2)×Y^(-2×K)` from -2 to N has no value at X = 0 for N = -3, where the sum is 0, and `X^(K+1)×Y^(-K)` from -1 likewise. `(Y÷TAN(Z))^(K+1)` from -1 to N has no value at Y = 0 for N = -2, and so do ACOS, ACOSH and TAN(W) in its place.
- A letter-free constant that is exactly 0 but isn't seen to be is printed as written:
  - in a finite sum's terms, Σ 1÷c^K for K = 1 to 3, where the line divides by c and so shows the hole;
  - in a factor 1÷c pulled out of a limit, SIN(X)÷(c×X) at 0.

  Here c = SIN(1)^2+COS(1)^2-1. simplify prints 1÷c the same way.
- The least arena a case needs isn't monotone in the arena it's given: at rc6, two bench cases needed less at 16,384 B than at 12,288 B, with the same answer. Not yet traced.

Also not on 1.0's list: trigonometric identities, matrices, letters as coefficients past degree 1, and implied products (`2X`).
