Casimir 1.12.0: defint answers integrands that read as SIN and COS polynomials through LN(EXP(u)), where 1.11.0 refused them as improper. `COS(LN(EXP(X+1)))` from 1 to 2 is `-SIN(2)+SIN(3)` and `LN(EXP(SIN(X)))` from 0 to 2 is `-COS(2)+1`, where 1.11.0 printed IMPROPER.

## What's new since 1.11.0

- **A typed condition that always holds is no longer a refusal** (CAS 045). Typed `LN(u)` asks u > 0 and `SQRT(u)` asks u ≥ 0 over the interval, and 1.11.0 could bound u there only when u is linear in the variable, so `LN(EXP(X+1))` was refused as if at a pole. Where u is EXP, ALOG or COSH of anything, or a positive number to a power that isn't a number (`2^X`, `2^K`), u > 0 wherever it has a value, and defint now reads it so. `LN(EXP(SIN(X)))^3` from 0 to π is `4÷3`, `LOG(ALOG(SIN(X)))` from 0 to π is `2`, and in DEG `LN(EXP(SIN(X)))` from 0 to 90 is `180÷π`.
  - The same for a letter: `LN(EXP(K))×SIN(X)` from 0 to 1 is `K-K×COS(1)` and `SQRT(EXP(K))^2×SIN(X)` is `EXP(K)-EXP(K)×COS(1)`, where 1.11.0 said BAD_ARG, since EXP(K) > 0 for every real K. This also holds in the table's path: `LN(EXP(K))×X` from 0 to 1 is `K÷2`.
  - The true refusals beside them stay: `SQRT(K)^2×SIN(X)` is BAD_ARG (K < 0 has no value, and the answer doesn't show it), `SQRT(-1)×X` and `LN(-2)×SIN(X)` are UNDEFINED, and a pole is IMPROPER as before.
- **What moves**, against 1.11.0:
  - the golden corpus: none of its 10,822 lines;
  - the documented examples: none of 1.11.0's commands;
  - the developer tests: none of the earlier checks;
  - the reviewer's hidden sets of the earlier units: 0 moved of 34,956, and CAS 028's 568;
  - defint's refusals named above: IMPROPER or BAD_ARG on an integrand whose typed condition always holds, now answered. 1.11.0's survivor search found four of them in its own lines (`COS(LN(EXP(X+1)))` from 1 to 2 among them), and in that corpus they are the only lines that move.
- **Measured:** S3 code 299,032 bytes (137 more than 1.11.0's 298,895), under decision 105's 331,776 B line (Michael's, 30 KiB on decision 94's 301,056 B), with 32,744 B left. Each unit still brings its measured cost and its scope to Michael: the room isn't a budget. No accepted frame moved, on the host or the S3 (CAS 045's first shape moved one on the S3, and was changed). On the ESP32-S3 the bench passes at this tag, and its worst stack is unchanged (11,360 B masked).

**The guarantee is unchanged:** an exact answer, or a status that says why not. Never an approximation presented as an answer.

## The known wrong answers

None. CAS 045's hidden set found no wrong value, and every answer this unit adds agreed with a quadrature.

## How it's tested (at this tag)

- `make test`: 20 programs with 122,278 checks. They read every answer back through an independent evaluator (the calculator firmware's core) and compare values. New in 1.12.0: CAS 045's 25 defints, each answer against the core's quadrature, and the refusals beside them.
- The golden corpus, 10,822 calls compared byte for byte; none moved.
- Frame and symbol gates: no frame over 1 KiB, no `pow` linked, and every function's frame against the recorded table; none moved.
- Value checks beside the corpus judge Taylor polynomials, limits, integrals, sums and solve's and integrate's steps against independent constructions.
- The docs: every `casim` row of docs/language.md and the units' example files is run, each again with the series route compiled out, and every backticked answer in docs/language.md's prose must be in one of them or in the golden corpus.
- CI on every push: `make check` under gcc and clang, `make test`, CMake under gcc and clang, WebAssembly, and the ESP-IDF component for the ESP32-S3.
- Mutants (scripts/mutants.py): 652 deliberate faults, and at this tag's tree `make test` catches 644 (work/v1.12.0-mutants.txt). The eight that survive are 1.11.0's eight, and searched again at this tree each moves exactly the lines it moved at 1.11.0's. CAS 045's 4 are caught, among them the reviewer's own, which `make test`'s golden corpus catches at five lines. In the same search the build's own lines moved only where CAS 045 answers: the four definite integrals 1.11.0 refused IMPROPER, each right against a quadrature.
- Hidden sets kept by the project's reviewer, independent of this repository, rerun at each unit's acceptance. At CAS 045's: its own, 141 rows run at 2,048, 8,800 and 16,384 B (423 runs), none failing, and the earlier units'.
- On the ESP32-S3, at this tag's tree (e7b2c51), every one of the bench's 1,521 cases is byte-identical to the host, and the steps of the 304 derivatives, integrals, solves and partial fractions page as the host's:
  - with a 16,384 B arena, the size the calculator runs it with;
  - worst stack 11,360 B read with interrupts masked, plus a 400 B interrupt allowance (two frames, 384 B, or the most measured over the masked reading where that is larger: 400 B in this run, where 1.11.0's measured 336), 11,760 of a 12 KiB task; CAS 044's five defint cases, which run CAS 045's typed check, reach 2,032 B as at 1.11.0;
  - nesting deeper than 44 levels is refused (NO_ROOM);
  - about 299 KB of Casimir's code at -O2 (299,032 B), 32,744 B under decision 105's 331,776 B line.

## Not yet, after 1.12.0

These print a line without the note it could have, print a row the answer doesn't need, have no value where the sum has one, refuse, or print a constant as written. None of them is a wrong value.

New with CAS 045, each a refusal, as at 1.11.0:

- **LN(EXP(u)) and SQRT(EXP(u))^2 outside CAS 042's form stay NOT_INTEGRABLE:** `LN(EXP(X))` from -1 to 1, `SQRT(EXP(X))^2` from 0 to 1 and `COS(LN(SQRT(EXP(X))^2))` from 0 to 1. integrate's twin has no antiderivative for them through CAS 042's form, and the table reads them as typed.
- **LN(EXP(u)) with u a product or a sum, times another factor, is NOT_INTEGRABLE** in integrate and defint alike: `2×LN(EXP(SIN(X)^2×COS(X)))` from 0 to 1, though `LN(EXP(SIN(X)^2×COS(X)))` is `SIN(1)^3÷3` and `2×SIN(X)^2×COS(X)` is `2×SIN(1)^3÷3`. So are `-LN(EXP(SIN(X+1)×COS(X+1)^2))`, `COS(X)×LN(EXP(SIN(X)^2×COS(X)))` and `2×LN(EXP(SIN(X)+COS(X)))`. In the canonical integrand u's factors or terms merge with the factor beside it, and CAS 017's rule 2 looks for u as a whole part of that form, as it does for every typed LN. With u a single function or power (`2×LN(EXP(SIN(X)^2))`), or with the factor added rather than multiplied, it answers.
- **A sum of a polynomial and a SIN and COS product is NOT_INTEGRABLE in defint, though integrate answers it:** `SIN(X)^3×COS(X)+X` from -1 to 2 (integrate: `X^2÷2+SIN(X)^4÷4`), as at 1.11.0. defint takes CAS 006's table and CAS 042's form, not integrate's other paths (as for `X×SIN(X)^2` below).
- **A condition on an argument that isn't linear, and that CAS 045 doesn't decide, is still refused IMPROPER**, as at 1.11.0 (ruled to stay until a row integrate can answer reaches it).

Carried from 1.11.0, with definite SIN and COS integrals (CAS 044), each right in value, printed in canon's form, and the same at this tag:

- **An odd function at a negative bound keeps its sign inside:** `SIN(X)^2` from -1 to 2 is `SIN(-2)÷4-SIN(4)÷4+3÷2`.
- **A radical's power isn't combined with its root:** `SIN(X)^3` from 0 to π÷6 is `-SQRT(3)÷2+3^(3÷2)÷24+2÷3`, which is 2÷3-3×SQRT(3)÷8.
- **A 0 by symmetry over bounds off the table prints as its terms,** since COS(-1) isn't taken to COS(1): `SIN(X)^3` from -1 to 1 is `COS(-1)-COS(1)-COS(-1)^3÷3+COS(1)^3÷3`, exactly 0. Over the table's bounds the 0 is seen: from -π to π it is `0`.
- **Substitution and parts don't reach defint:** `X×SIN(X)^2` from 0 to π is NOT_INTEGRABLE, as the integrand outside CAS 042's form is.
- ~~**LN(EXP(u)) with X inside u is refused IMPROPER on a finite integrand,** where the right refusal is NOT_INTEGRABLE~~ Fixed by CAS 045, and the right result was the answer, not NOT_INTEGRABLE: 1.11.0's line here was wrong on that point. `COS(LN(EXP(X+1)))` from 1 to 2 is `-SIN(2)+SIN(3)`.

Carried from 1.10.0. With x^n = c under the bit (CAS 043), each the line without the bit, never a wrong value; CAS 044 and CAS 045 reach defint only, so these were not run again:

- **Every root doesn't fit the line for most n past 6:** `X^7=1` is `SOLVED, 6 CPLX: X=1` and `X^9=64` `SOLVED, 8 CPLX: X=4^(1÷3)`, with the bit or without it. Of x^n = c for n 2 to 12 over 14 values of c in three modes (462 lines), 199 give every root and 263 the line without the bit.
- **Only a pure power of the variable:** `(X+1)^3=2` is `PARTIAL, DEGREE 3 LEFT` under the bit, as without it. Letters stay as at 1.9.0 (`X^3=A` is `SOLVED: X=A^(1÷3); A≥0` with the bit).

With SIN and COS integrals (CAS 042), each a refusal, never a wrong value, as at 1.10.0 (CAS 044 and 045 change defint's path, not integrate's):

- **A sum with letter or π coefficients runs out of room** at the calculator's 16,384 B more often than one with numbers, since each such coefficient is kept as an expression: `A×SIN(X)^8` needs 17,808 B, `A×SIN(X)^6+π×COS(X)^4` 20,704 B.
- **The degree is 24 in COS(θ) over the whole integrand,** so `SIN(24×X)×SIN(X)` and `SIN(X)^25` are refused though each argument is within 24 times θ.
- **expand doesn't cancel a rational coefficient in letters once π is multiplied in** (π÷(π+A^2×π) beside 1÷(A^2+1)). integrate's check works around it by checking the answer as built; the cause is expand's.
- **Refused, as outside the unit:** a letter in an argument (`SIN(A×X)^2`), arguments with no common angle (`SIN(X)×SIN(X+1)`, `SIN(X)^2+SIN(SQRT(2)×X)^2`), a power of SIN or COS times X (`X×SIN(X)^2`), a quotient (`SIN(X)^2÷COS(X)`), and powers of TAN (`TAN(X)^2`). Their definite integrals are CAS 044's (above).
- **Where substitution and parts run out of room first,** NO_ROOM stands when this path doesn't fit either: `COS(3×X)^4×SIN(2×X)` is NO_ROOM at 8,800 B and answers at 16,384 B.

With quadratics and pure powers (CAS 041), as at 1.10.0, each a refusal or a row, never a wrong value:

- **A D below 0 everywhere is refused, not answered.** `X^2+X+A^2+1=0`, whose D is -4×A^2-3, is NOT_NUMERIC, where NO REAL ROOTS would be true. No value is taken from digits for letters.
- **A row the 22 points can't witness refuses.** A letter's sign at the 16 added points is its position mod 4, so E shares A's sign at each of them: `X^2=-A×E` is NOT_NUMERIC, though `X^2=-A×B` is `SOLVED: X=SQRT(-A×B); X=-SQRT(-A×B); -A×B≥0`.
- **Square content left inside the root.** Only numbers' squares and single letters' even powers come out: `X^2=(A-1)^2×B` is `SOLVED: X=SQRT(B-2×A×B+A^2×B); X=-SQRT(B-2×A×B+A^2×B); B-2×A×B+A^2×B≥0`.
- **An odd root with letters asks r≥0,** though a negative r has a real odd root: `X^3=A` prints `A≥0`.
- **An even root of a perfect power is left as a power:** `X^4=A^4` is `SOLVED: X=(A^4)^(1÷4); X=-(A^4)^(1÷4)`, where ABS(A) is the plain form.
- **A row for nothing:** `X^2=A×SQRT(A)` prints `A^(3÷2)≥0` beside `A≥0`.
- **Refused, as outside the unit:** a pure power of a linear u (`(X+1)^3=A` is NOT_NUMERIC), a letter in an exponent (`A=P×(1+R)^N` for R), and degree 3 or more with letters but a pure power. With `--complex`, a letter quadratic prints as without it.

Carried from 1.7.0, not run again: CAS 042 reaches integrate only, CAS 043 solve under the complex bit only and CAS 044 and 045 defint only, and none of these is any of them. New with the zero test (CAS 040), each a refusal or 1.6.0's line, never a wrong value:

- **Refused, though X's coefficient isn't 0.** The coefficient reads 0 at every one of the six points where it has a value: 0 on a region holding them all (`(ABS(A+40)-(A+40))×X=1`, 0 for A≥-40, whose 1.6.0 answer held for A<-40); a factor built to vanish at the points; a part under about 1E-31 of its terms (`(SIN(A)^2+COS(A)^2-1+10^-40)×X=1`); or read at one point only, 0 there (`(4×A-147)×SQRT(A-30)×X=1`, which has a value only at A = 147/4 among the points).
- **A 0 coefficient no point reads prints 1.6.0's line,** SOLVED with a row that never holds: functions the bound doesn't read (`(IP(A)+FP(A)-A)×X=1`, `(10^LOG(A)-A)×X=1`), and a domain holding no point (`(2×LN(A-60)-LN((A-60)^2))×X=1`).
- **canon doesn't multiply out (A-B)×(A+B) where a sum holds it:** `(A^2-(A-B)×(A+B)-B^2)×X=1` printed SOLVED at 1.6.0, and 1.7.0 refuses it only because the point test reads its 0. canon's form is unchanged.

With letters as coefficients (CAS 038 and 039):

- **canon's forms, not a student's:** `(X+A)÷B=C` is `X=B×(C-A÷B); B≠0`, `A×X+B=C` is `X=-(B-C)÷A; A≠0`, `F=9÷5×C+32` for C is `C=5×F÷9-160÷9`, and steps' BOTH SIDES for `V=I×R` is `-I×R+V=0`.
- **A row in the expanded coefficient's form:** `A=(B+C)×H÷2` for H has `-B÷2-C÷2≠0`, and `(A×B-A)×X=B` has `-A+A×B≠0`.
- **A row that always holds, or one left unsolved:** `(SQRT(A)+1)×(X+1)=B` has `SQRT(A)+1≠0`, `(SQRT(A)-1)×X=1` has `SQRT(A)-1≠0` (A≠1), `ACOSH(A)×X=1` has `A≥1; ACOSH(A)≠0` (A>1), and `(A^3-1)×X=1` has `A^3-1≠0`.
- **Refused, NOT_NUMERIC as 1.5.0 refuses them:** var dropping out (`A×(X-1)=A×X`), degree 3 or more with a letter but a pure power (`A×X^3+X=1`), var inside a function or a denominator (`A×SIN(X)=B`, `A÷X=1`), a condition no row writes (ASIN's ABS(u)≤1, ATANH's ABS(u)<1), model 33's variable i, a constant with neither a letter nor π (`SIN(1)×X=1`), and an answer where the typed text has no value (`X×(X-A)÷(X-A)=A`). pw's first refusal decides whether letters are read at all: `X^2÷X×A=1` is NOT_POLYNOMIAL, and `A×X^2÷X=1` is `SOLVED: X=1÷A; A≠0`. Past six conditions, NO_ROOM (`A×B×C×D×E×F×G×X=1`).

Carried from 1.5.0, not run again for the same reason:

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
- **The complex roots of a factor of degree 3 or more** aren't counted, since solve doesn't find them, with the bit or without it: `X^4+X+1` is `PARTIAL, DEGREE 4 LEFT`. (1.7.0's example, `X^4+1`, is a pure power and is `NO REAL ROOTS` now.)
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

Also not on 1.0's list: trigonometric identities (but integrate's SIN and COS polynomials), matrices, letters as coefficients past degree 2 (but pure powers), and implied products (`2X`).
