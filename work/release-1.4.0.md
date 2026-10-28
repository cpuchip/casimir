Casimir 1.4.0: the wrong answers found after 1.3.0 are fixed. sum, defint and limit read the input's domain as typed, so none of them prints a value where the input has none, and a row whose other letters sit inside a function with a value everywhere now says the letter's row. 1.4.0's code is CAS 034's and CAS 035's, accepted at 4c0523b and a6836d0, with the version text changed. src/casim.h's interface is 1.0's, but answers' bytes change, which is why this is 1.4.0 and not 1.3.1: a moved answer has the same value wherever both lines have one, and the rows and refusals it gains exclude only points where the input has no value, with one named cost (below).

## What's new since 1.3.0

- **The input's domain, as typed** (CAS 035, the fix). canon's domain rule lets it define a result where the input has none (u÷u is 1, EXP(LN(u)) is u, SQRT(u)^2 is u). A walk over the typed input now lists each part's condition: a divisor not 0, LN's and LOG's argument above 0, a root's not below 0, ASIN's and ACOS's argument within ±1, ACOSH's at least 1, ATANH's within ±1, TAN's cosine not 0. Each op reads the list:
  - a sum prints a row for each condition in the other letters, unless a row already says it or the answer shows the hole itself: `(Y÷Y)^K` from 1 to N is `N; Y≠0` (1.3.0: `N`), and `(Z×EXP(LN(Y))+1)^K` from 0 to ∞ is `-1÷Y÷Z; Y>0; ABS(Y×Z+1)<1`. Summed term by term, the input is read with each K put in, so an exponent in K counts: `Y^K×Y^(-K)` from -1 to 1 is `3; Y≠0`;
  - defint is IMPROPER where the typed integrand has no real value on the interval (`SQRT(X)^2` over -2..-1, 1.3.0: `-3÷2`), and BAD_ARG where another letter's value decides it (`X×Y÷Y` over 0..1, 1.3.0: `1÷2`);
  - limit is UNDEFINED on a side with no real value near the point: `EXP(LN(X))` at -1 (1.3.0: `-1`), `SQRT(X)` at -1 (1.3.0: `SQRT(-1)`), and `SQRT(X)` at 0 two-sided, while from above it is still `0`. A condition in another letter is CAS 023's row: `X×Y÷Y` at 1 is `1; Y≠0`;
  - limit, a divisor 0 for every X at a letter's value: `SIN(K×X)÷SIN(K×X)` at 0 is `1; K≠0` (1.3.0: `1`), since at K = 0 it has no value anywhere. Where casim can't show that no other letter's value does the same, the limit is UNDECIDED;
  - the closed form's cancelled divisor is a typed divisor too: `K×(X×Y)^K÷Y` from 1 to ∞ gains `Y≠0`.
- **A term-by-term sum's rows count toward the 191 bytes.** Where they take the line past it, the closed form answers instead: `(ACOS(Z)^2×Y)^K÷ACOS(Z)` from 1 to 12 gains `ACOS(Z)≠0` (the input has no value at Z = 1, where 1.3.0's line is 0), and its 12 terms with the row take 198 bytes, so it prints `-Y×ACOS(Z)÷(Y×ACOS(Z)^2-1)+Y^13×ACOS(Z)^25÷(Y×ACOS(Z)^2-1); ACOS(Z)≠0; Y×ACOS(Z)^2≠1`.
- **A row whose other letters are inside a function with a value everywhere says the letter's row** (CAS 034). CAS 032 let a kept row say `L≠0` only where its other letters were in a sum, a product or a whole power. ABS, SIN, COS, EXP, ALOG, SINH, COSH, TANH, ATAN and ASINH of those now count as well: `(1+ABS(Y)×Z)^K` from 0 to N prints `Z×ABS(Y)+1≠1` without 1.3.0's `Z≠0`, and `(1+Z×Y)^K` from 0 to ∞ is `-1÷Y÷Z; ABS(Y×Z+1)<1` (1.3.0: `-1÷Y÷Z; Y≠0; Z≠0; ABS(Y×Z+1)<1`).
- **A sum to N below LO-1** (documented, no code change). A closed form to N holds from N = LO-1, the empty sum, up. Below that it is the formula carried on, which agreed with the subtracting convention wherever both have a value in every case measured (work/sum-below-lo.tsv).
- **The named cost.** To fit the flash budget, limit tries a letter's value only for a letter or its power. A divisor's k that is linear in a letter gets its row untried, so `(SIN((Y-1)×X)+1)÷(SIN((Y-1)×X)+1)` at 0 prints `1; Y≠1`, though at Y = 1 the input is 1 everywhere. The row is never wrong, only stricter than it needs to be. `(SIN((Y-1)×X)+Y)÷(SIN((Y-1)×X)+Y)` does the same.
- **What moves:** CAS 034 moves 934 of a family's 2,784 calls, 3 documented examples and none of the golden corpus. CAS 035 moves 37 of the golden corpus's 10,822 lines and their 37 under →DEC (34 to a refusal, 3 gaining a row), and 26 documented examples, each by a row. Of the earlier units' hidden sets, CAS 034 moves 116 runs in CAS 032's set and 2 in CAS 033's, each by its rule; at CAS 035's acceptance the only lines its judge failed were the ACOS sum's closed form (above), which the reviewer accepted by hand.
- **Measured:** on the ESP32-S3 at 16,384 B, the 13 new bench rows take at most 7,272 B of arena and 5,808 B of stack (a sum of a 40-deep nest of `1÷(`, 59 ms), and the worst stack is unchanged. Against 1.3.0's code with CAS 034, CAS 035 changes no bench case's status; 51 cases need more arena (at most 1,976 B more, a 43-deep nest's limit at 4,864 B) and 14 more stack (at most 464 B more).

**The guarantee is unchanged:** an exact answer, or a status that says why not. Never an approximation presented as an answer.

## The known wrong answers

None. Those found after 1.3.0 (canon's domain rule reaching sum, defint and limit, and limit reading SQRT outside its real domain) print the refusals and rows above.

## How it's tested (at this tag)

- `make test`: 20 programs with 118,449 checks. They read every answer back through an independent evaluator (the calculator firmware's core) and compare values.
- The golden corpus, 10,822 calls compared byte for byte: CAS 034 moves none, and CAS 035 moves 37, each listed with its before (tests/golden/moved/cas-035.txt). A check judges each moved line apart from casim: the typed input, valued by an independent evaluator, has no value somewhere the line no longer gives one.
- Frame and symbol gates: no frame over 1 KiB, no `pow` linked, and every function's frame against the recorded table. No frame moved from 1.3.0, on the host or the S3. CAS 035's 29 new functions take at most 384 B on the S3 (td_defint).
- Value checks beside the corpus judge Taylor polynomials, limits, integrals and sums against independent constructions. New in 1.4.0: sums whose letter's row a row holding a total function may say (CAS 034), judged by the rule and by its sentence; and sums whose typed term has no value at the low bound, judged by a strict evaluator that gives no value to a divisor 0 to 40 digits or to LN(0).
- CI on every push: `make check` under gcc and clang, `make test`, CMake under gcc and clang, WebAssembly, and the ESP-IDF component for the ESP32-S3.
- Mutants (scripts/mutants.py): 519 deliberate faults, and at this tag's tree `make test` catches 514 (work/v1.4.0-mutants.txt). At the version commit it caught 509: nine faults in older checks, each caught at CAS 034's tree, survived because CAS 035's typed rows print the same line. A search of generated inputs, each mutant against the build, found the lines where five of those checks alone decide, and 6090b8c pins them (rows only). Five survive, each named in work/v1.4.0-mutants.txt with what masks it: cancel-rewind-keeps-cache, as at 1.3.0; cond-site2-lhopital and ser-root-of-negative, which no searched line separates from the build; div-letter-off, which drops only rows the kept rows imply; and ser-ln-no-value, which differs from the library build on no line. With the series route forced alone it differs on one, where 1.4.0 refuses a line the mutant gets right. Some pins hold what 1.3.0 printed, and no ruling stands behind them:
  - `K^X` at 0.5 is `SQRT(K); K>0`. At K = 0 the typed text has a value (0^X is 0 for X>0, and the limit is 0, which is SQRT(0)), so `K≥0` would be exact; the stricter row is lawful at that edge.
  - The three defint pins keep IMPROPER where the integrand also holds a constant with no value (`X^(-2)×ACOS(3)` over 0..1). UNDEFINED would be just as lawful, and arguably the plainer word. defint's interval check is redundant to the output except for this choice of word.
- Hidden sets kept by the project's reviewer, independent of this repository, rerun at each unit's acceptance. At CAS 034's: its own, 744 runs, none failing; at CAS 035's: its own, 1,074 rows at 8,800, 16,384 and 65,536 B, none failing; and the earlier units' sets.
- On the ESP32-S3, every one of the bench's 1,412 cases is byte-identical to the host:
  - with a 16,384 B arena, the size the calculator runs it with;
  - worst stack 11,360 B read with interrupts masked, plus a 384 B interrupt allowance (two frames, or the most measured over the masked reading where that is larger), within a 12 KiB task;
  - nesting deeper than 44 levels is refused (NO_ROOM);
  - about 262 KB of Casimir's code at -O2 (262,036 B, 13,674 more than 1.3.0's), 108 B under the 262,144 B budget.

## Not yet, after 1.4.0

These print a line without the note it could have, print a row the answer doesn't need, have no value where the sum has one, refuse, or print a constant as written. None of them is a wrong value.

- **The typed domain, stricter than it needs to be** (CAS 035):
  - limit's letter conditions at the edge: `SQRT(Y)^2×X` at 1 prints `Y; Y>0`, though Y = 0 has a value, and `ASIN(Y)×X` at 1 prints `ASIN(Y); ABS(Y)<1`, though |Y| = 1 has one;
  - `SQRT(X^2×Y)^2` at 0 gives `0; Y>0`, though Y = 0 has a value;
  - a divisor's k linear in a letter gets its row untried (the named cost, above).
- **The typed domain, refused where it could be answered** (CAS 035):
  - an X×Y-form limit at ±∞ is UNDECIDED: its sign is Y's, and no row is given (`SQRT(X×Y)÷SQRT(X×Y)` at ∞);
  - a divisor 0 for every X at a letter's value that casim can't rule out is UNDECIDED: `(Y×SIN(X)-SIN(Y×X))÷(Y×SIN(X)-SIN(Y×X))` at 0, whose divisor is 0 for every X at Y = ±1;
  - rows count toward the 191 bytes, so some lines with them are TOO_LONG: `K×(1+Z×F(u))^K` from 1 to ∞ with u Y+1, Y-2 or W-1, and `(K-1)×(F(Z)×Y)^K÷F(Z)` from 1 to N for ACOSH, ASINH and ATANH, and LN from 2.
- **Older checks beside the typed rows** (found by 1.4.0's mutant search; 1.3.0 prints the same):
  - which refusal a constant with no value gets beside a pole on the interval: `X^(-2)×ACOS(3)` over 0..1 is IMPROPER, where UNDEFINED would be just as lawful (CAS 009's interval check answers first);
  - limit rows from the answer's own walk and f's (CAS 023 sites 6 and 7) that restate a typed row (`K×π+4≠0` beside `K×π÷2+2≠0`) or can't fail (`EXP(K^2)≠0`), and 69 limit lines found TOO_LONG where the line without site 6's rows fits.
- **A quotient that canon cancels, or whose divisor falls in K, is still refused.** `X^(2×K)÷X^K` is NO_CLOSED_FORM: canon reads it as `X^K`, which has a value at X = 0, where its terms are 0÷0. So is `X^K÷Y^(-K)`: its divisor has no value at Y = 0, while canon's `(X×Y)^K` has one. Answering them needs a row that `(X÷Y)^K`-style twins don't print (`X≠0`, `Y≠0`).
- **The `÷` spelling reaches the rows of the `×` one.** A quotient prints the answer its canon spelling did in 1.1.0, rows for nothing and missing values included. At a letter's 0, a quotient's terms whose power of the letter is 0 or less have values (0^0 = 1), and there the letter's row excludes the sum's value. `X^K÷Y^K` from 0 to N prints `-1÷(X÷Y-1)+(X÷Y)^(N+1)÷(X÷Y-1); Y≠0; X÷Y≠1`, though at Y = 0 and N = 0 the sum is 1, as `X^K×Y^(-K)` printed in 1.1.0. `X^(K-1)÷Y^(K+2)` from 1 to N prints `X≠0`, though at X = 0 the sum is 1÷Y^3 for every N ≥ 1. Where the factor is one of CAS 030's functions of a letter, its row excludes the point now: `Y^K÷SIN(Z)^K` from 0 to N prints `SIN(Z)≠0`, though at Z = π and N = 0 the sum is 1 (rows for nothing, below). They're the class below, where the shift that clears a rational ratio's row is refused past a letter at a negative power.
- **A note that doesn't fit.** Where the line with the note would pass solve's 191 B, solve prints the line as 1.0.1 did, with no note, never TOO_LONG for it and with no root dropped. `(7×X-2)×(3×X^2-9×X-7)×(8×X^2+3×X-2)×(8×X-3)×(X^2-3×X+8)×(8×X^2+6×X-1)×(1×X^2+9×X+3)` prints its ten real roots in 184 B as `SOLVED: …`, and `, 2 CPLX` would make it 192 B. With `--complex` the same input is TOO_LONG. In a family of degree-12 polynomials built to reach the cap, 59 of 1,416 noted answers give way at one seed and 69 of 1,403 at another; none in the golden corpus does.
- **The complex roots of a factor of degree 3 or more** aren't counted, since solve doesn't find them, with the bit or without it: `X^4+1` is `PARTIAL, DEGREE 4 LEFT`.
- **Rows for nothing.** Each is excluded where the sum has a value, while the answer has none there:
  - a function factor's row to N, at an N before the first term that divides by the factor (CAS 030, as a letter's row): `(Y÷SIN(Z))^K` from 1 to N prints `SIN(Z)≠0`, which excludes Z = π at N = 0, where the sum is 0, as its twin's `X≠0` excludes X = 0. For every N from 1 the sum has no value there, and the row is the one it needs;
  - `(X÷Y)^K÷X` from 1 to N prints `X≠0`, though the sum has a value at X = 0;
  - a row that excludes no point: casim doesn't know a polynomial's real zeros here, so to ∞ a divisor inside the ratio that has none gets its row all the same (CAS 033): `(Z÷(Y^2+1)+1)^K` from 0 to ∞ prints `Y^2+1≠0`, true everywhere;
  - past an irrational constant, to N, the shift that clears a rational ratio's row is refused, and the cancel doesn't reach a power of N. `(K-1)×(SQRT(2)×X×Y)^K÷X` from 1 to N prints `…; X≠0; X×Y×SQRT(2)≠1`, though the sum is 0 at X = 0. To ∞ the cancel clears it.
- **A redundant row the rule doesn't reach.** Where the row beside it holds another letter under a division or inside a function with no value somewhere (TAN, LN, LOG, ASIN, ACOS, ACOSH, ATANH), the letter's row stays though it may exclude nothing more: `(1+TAN(Y)×Z)^K` from 0 to N keeps `Z≠0` beside `Z×TAN(Y)+1≠1`.
- **A missing value.** Each has no value at a point where the sum has one, and no row excludes that point. Michael's card cas-missing-scope rules rows for these, giving way at the cap, for a later unit (CAS 036):
  - at the empty sum, at a letter's 0, from a lo of 1 or more. `(1÷Y)^K` from 1 to N prints `1÷(Y-1)+(1÷Y)^(N+1)÷(1÷Y-1); Y≠1`. At Y = 0 and N = 0 the sum is 0, the empty sum, while the answer has no value. The same holds for `Y^(-K)`, `(3÷Y)^K` and `(1÷X)^K`;
  - at the empty sum, where a factor outside the ratio has no value. `1÷X` from 1 to N prints `N÷X`, which has no value at X = 0 and N = 0, where the sum is 0. The same holds for `Y^K÷X`, and for `K×Y^K÷SIN(Z)` at Z = π;
  - at the empty sum, where a function in the summand has no real value: the answer holds the function, so it shows the hole at every N but the empty sum, and no row is added. `(1+Z×SQRT(Y))^K` from 0 to N has no value at Y = -1 for N = -1, where the sum is 0;
  - summed to N from a negative lo, at a letter's 0 for N ≤ −2. `X^(K+2)×Y^(-2×K)` from -2 to N prints `X^2×(X÷Y^2)^(N+1)÷(X÷Y^2-1)-Y^4÷(X÷Y^2-1); Y≠0; X÷Y^2≠1`. At X = 0 and N = -2 the sum is Y^4, while X^2×(X÷Y^2)^(-1) has no value;
  - at a zero of a function factor other than its letter's 0, where the same sum with the factor read as a letter prints that letter's row for nothing. What stays:
    - a line within its row of the cap: where the line with the row would pass 191 B, the row gives way. `(K-1)×(COS(Z)×Y)^K÷COS(Z)` from 0 to N prints its 181 B line as 1.2.0 did, with no value at Z = π÷2 for N = -1, where the sum is 0. Its SIN twin is the same at Z = π;
    - a polynomial factor, whose real zeros casim doesn't know here. `X^K÷(Y-1)^K` from 1 to N prints `X÷(Y-X×Y÷(Y-1)+X÷(Y-1)-1)+(X÷(Y-1))^(N+1)÷(X÷(Y-1)-1); X÷(Y-1)≠1`, with no value at Y = 1 for N = 0, where the sum is 0;
    - a function of anything but a letter. `(Y÷SIN(2×Z))^K` from 1 to N prints `-Y÷(Y-SIN(2×Z))+(Y÷SIN(2×Z))^(N+1)÷(Y÷SIN(2×Z)-1); Z≠0; Y÷SIN(2×Z)≠1`, with no value at Z = π÷2 for N = 0, where the sum is 0.
- A letter-free constant that is exactly 0 but isn't seen to be is printed as written:
  - in a finite sum's terms, Σ 1÷c^K for K = 1 to 3, where the line divides by c and so shows the hole;
  - in a factor 1÷c pulled out of a limit, SIN(X)÷(c×X) at 0.

  Here c = SIN(1)^2+COS(1)^2-1. simplify prints 1÷c the same way.
- The least arena a case needs isn't monotone in the arena it's given: at rc6, two bench cases needed less at 16,384 B than at 12,288 B, with the same answer. Not yet traced.

Also not on 1.0's list: trigonometric identities, matrices, letters as coefficients in solve, and implied products (`2X`).
