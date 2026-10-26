Casimir 1.0.0: exact computer algebra in portable C11, written for the STU-32 calculator (an ESP32-S3) and usable anywhere C runs. It's MIT licensed and allocates nothing: the caller supplies an output buffer and an arena.

Everything on 0.9.0's list for 1.0 is in, and no wrong answer is known. 1.0.0's code is 1.0.0-rc7's with the version text changed. rc7 fixed rc6's known wrong answer with its siblings (CAS 025), and a second class of the same kind found while fixing it (CAS 026). The calculator's firmware now runs it (below).

## What's new since 0.9.0

- **partfrac**: partial fractions over the rationals. `(X^4+1)÷(X^3+1)` is `X+2÷(3×(X+1))-(2×X-1)÷(3×(X^2-X+1))`.
- **integrate**, three new routes, each answer checked before it prints:
  - rational functions through their partial fractions, with LN and ATAN terms (`(2×X+3)÷(X^2+X)` is `3×LN(ABS(X))-LN(ABS(X+1))`);
  - substitution;
  - by parts (`X×EXP(X)` is `-EXP(X)+X×EXP(X)`).
- **taylor**: a Taylor polynomial to order 12 about a point, exact. `LN(X)` about 1 to order 3 is `(X-1)-(X-1)^2÷2+(X-1)^3÷3`.
- **limit**: at a point from either side or both, or at ∞ and -∞, exact. `(1+3÷X)^X` at ∞ is `EXP(3)`.
- **sum**: a sum in closed form, to N or to ∞; or exactly, term by term, between whole bounds at most 64 apart.
  - Closed forms come from a polynomial times a geometric term, by an antidifference checked exactly, and from rational functions whose partial fractions telescope.
  - `K×2^K` from 1 to N is `-2^(N+1)+N×2^(N+1)+2`, and `1÷(K×(K+1))` from 1 to ∞ is `1`.
- **Conditions.** sum, taylor and limit answers carry the rows that say where they hold. `X^K` from 0 to N is `-1÷(X-1)+X^(N+1)÷(X-1); X≠1`.
- **Complex roots**: set `CASIM_COMPLEX` (the CLI's `--complex`), and solve prints a quadratic factor's conjugate pair: `X^2+2×X+5` gives `SOLVED: X=-1-2×0i1; X=-1+2×0i1`. It's off by default, and with it off solve prints real roots only, as before.
- **Fractions.** A number Casimir makes prints as a fraction (`X÷4+1÷6`). A typed decimal stays a decimal. →DEC prints decimals (`CASIM_PRINT_DECIMAL`).
- **steps** for partfrac, and for integrate's new routes (partial fractions, substitution, by parts).
- **New statuses**:
  - UNDECIDED: a limit, Taylor coefficient or sum that it can't decide. That includes a constant whose sign or zero a carried error bound can't settle; it is never a guess.
  - NO_CLOSED_FORM: a sum it finds no closed form for.
  - DECIMAL_FORM: an answer printed as →DEC would, because its fractions didn't fit and it holds a decimal.
  - UNDEFINED now covers a pole in every operation.
- **A sum's rows wherever a term divides, whatever else the ratio holds** (CAS 025). The rows are read past an irrational constant, a letter inside a function, or a letter at a negative power.
  - `K×(SQRT(2)×X×Y)^K÷Y` from 0 to ∞ is `-X×SQRT(2)÷2÷(X×Y×SQRT(2)-X^2×Y^2-1÷2); Y≠0; ABS(X×Y×SQRT(2))<1`. Its K = 0 term is 0×Y^(-1), which has no value at Y = 0.
  - A letter at a negative power keeps its row: `(X÷Y)^K÷Y` from 0 to ∞ is `-1÷(X-Y); Y≠0; ABS(X÷Y)<1`. ABS(X÷Y)<1 has no value at Y = 0, so it can't exclude that point; Y≠0 does.
- **A row on a function factor** (CAS 026). Where a term divides by a factor of the ratio that holds a letter, the answer gets that factor's row.
  - `K×(SIN(Z)×Y)^K÷SIN(Z)` from 0 to ∞ is `-Y÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2); SIN(Z)≠0; ABS(Y×SIN(Z))<1`, excluding every Z where SIN(Z) is 0.
  - The same holds for COS, TAN, LN, SQRT and the other roots, and ABS. `(Y÷SQRT(Z))^K` from 1 to ∞ is `-Y÷(Y-SQRT(Z)); SQRT(Z)≠0; ABS(Y÷SQRT(Z))<1`.
  - A factor that is never 0 gets no row: `K×(EXP(Z)×Y)^K÷EXP(Z)` from 0 to ∞ is `-Y÷2÷(Y×EXP(Z)-Y^2×EXP(Z)^2÷2-1÷2); ABS(Y×EXP(Z))<1`.
  - A root at a power that merges into its letter gets the letter's row: `K×SQRT(Z)^(2×K-1)×Y^K` from 0 to ∞ is `-Y×SQRT(Z)÷2÷(Y×Z-Y^2×Z^2÷2-1÷2); Z≠0; ABS(Y×Z)<1`.
- **Term by term, as simplify reads it.** Added term by term (whole bounds at most 64 apart), each term is the typed text simplified, so it cancels as simplify does: 0÷Y is 0, X÷X is 1. So `K×Y^K÷Y` from 0 to 3 is `2×Y+3×Y^2+1`, with no row, while from 0 to ∞ it is `Y÷(Y-2×Y^2+Y^3); Y≠0; ABS(Y)<1`. A closed form reads each term with its powers merged and nothing cancelled after, so its K = 0 term is 0×Y^(-1). Don't take one route's rows for the other's.

**The guarantee is unchanged:** an exact answer, or a status that says why not. Never an approximation presented as an answer.

## The known wrong answers

None. rc6's, `K×(SQRT(2)×X×Y)^K÷Y` from 0 to ∞ with no `Y≠0`, prints its row now (above).

**Found after this release, on 25 Oct 2026.** Summed from 0 to ∞, `(Z÷Y+1)^K` prints `-Y÷Z; Z≠0; ABS(Z÷Y+1)<1`. Every term from K = 1 has no value at Y = 0, so neither has the sum, while the answer does: it reads 0 there, and no row excludes the point (a row with no value there doesn't exclude it). `(Z÷(Y-1)+1)^K` at Y = 1, `(Z÷Y^2+1)^K` at Y = 0, and `(Z÷SIN(Y)+1)^K` wherever SIN(Y) is 0 do the same to ∞. Summed to N, none of them has a value where the sum has none. The project's reviewer found it while testing a later unit. The fix is CAS 033, in the release after 1.2.0, which adds the rows.

**Found after this release, on 26 Oct 2026.** canon may drop a part of the input that has no value at some points, since its domain rule lets it define a result where the input has none (u÷u is 1, u-u and 0×u are 0, u^0 is 1, EXP(LN(u)) and ALOG(LOG(u)) are u, SQRT(u)^2 is u). sum, defint and limit then print a value where the input as typed has none:
- Summed from 1 to N, `(Y÷Y)^K` prints `N`. At Y = 0 every term is 0÷0, so the sum has no value, while `N` has one. Summed from 0 to ∞, `(Z×LN(Y)÷LN(Y))^K` prints `-1÷(Z-1); ABS(Z)<1`, which has a value at Y = 1, where every term holds LN(1)÷LN(1), and at every Y ≤ 0, where LN(Y) has none.
- Summed from 0 to ∞, `(Z×EXP(LN(Y))+1)^K` prints `-1÷Y÷Z; Y≠0; Z≠0; ABS(Y×Z+1)<1`. At Y = -1 and Z = 1÷2 every term holds LN(-1), which has no real value, so the sum has none, while the answer is 2 there and each row holds. With ALOG(LOG(Y)), SQRT(Y)^2 or SQRT(Y)×SQRT(Y) in place of EXP(LN(Y)) it prints the same line.
- `defint "SQRT(X)^2" X -2 -1` prints `-3÷2`, though SQRT(X) has no real value on the interval (`SQRT(X)` over it is IMPROPER). `SQRT(X)×SQRT(X)` and `(X^(1÷4))^4` print the same, and `0×SQRT(X)` prints `0`.
- `limit "EXP(LN(X))" X -1 ''` prints `-1`, though LN(X) has no real value near -1.
- With another letter, a 0÷0 reaches limit and defint as well: `defint "X×Y÷Y" X 0 1` prints `1÷2` and `limit "X×Y÷Y" X 1 ''` prints `1`, each with a value at Y = 0, where the input has none.
- Where the sum's closed form cancels a divisor of the summand: summed from 1 to ∞, `K×(X×Y)^K÷Y` prints `-X÷(X×Y-1)-X^2×Y÷2÷(X×Y-X^2×Y^2÷2-1÷2); ABS(X×Y)<1`. At Y = 0 the term at K = 1 is 0÷0, so the sum has no value, while the answer is X.

limit also reads a function outside its real domain as typed, with no canon involved: `limit "SQRT(X)" X -1 ''` prints `SQRT(-1)`, and from below 0 it prints `0`, though SQRT(X) has no real value near -1 or below 0.

simplify promises no more than the domain rule (docs/canonical-form.md), and expand, collect, subst, derive and integrate answer through canon under the same rule. taylor and the polynomial ops read what was typed: `taylor "EXP(LN(X))" X -1 2` prints `UNDEFINED`, and solve, factor, cancel, partfrac and pdiv refuse these forms (NOT_POLYNOMIAL). 1.0.0 to 1.3.0 print every line above. The project's reviewer found it while testing a later unit, and a fix is being made (CAS 035).

## How it's tested (at this tag)

- `make test`: 20 programs with 118,421 checks. They read every answer back through an independent evaluator (the calculator firmware's core) and compare values.
- The golden corpus, 10,822 calls compared byte for byte.
- Frame and symbol gates: no frame over 1 KiB, no `pow` linked, and every function's frame against the recorded table.
- Value checks beside the corpus judge Taylor polynomials, limits, integrals and sums against independent constructions: power series over fractions, numeric derivatives at 90 digits, and sums added term by term at 60 digits. A sum's rows are judged at each letter's 0 and at each factor's own zeros (SIN at 0 and π, COS at π÷2, LN at 1), with the factor put to 0 exactly.
- CI on every push: `make check` under gcc and clang, `make test`, CMake under gcc and clang, WebAssembly, and the ESP-IDF component for the ESP32-S3.
- 428 planted mutants, each caught by the tests: 428 of 428 at rc7's tree (work/rc7-mutants.txt), whose src differs from 1.0.0's only in the version text.
- Hidden sets kept by the project's reviewer, independent of this repository, rerun at each unit's acceptance.
- On the ESP32-S3, every one of the bench's 1,359 cases is byte-identical to the host:
  - with a 16,384 B arena, the size the calculator runs it with;
  - worst stack 11,360 B read with interrupts masked, plus a 400 B interrupt allowance (the most measured over the masked reading), within a 12 KiB task;
  - nesting deeper than 44 levels is refused (NO_ROOM);
  - about 241 KB of Casimir's code at -O2.
- The calculator's firmware runs it: stu32-firmware 3b6512b, pinned to rc7's tag (e5f798b) with a 16,384 B arena, passes its stand-in run on the ESP32-S3 (ALL MET), with interrupts live and IDF's 300 ms watchdog.

## Not yet, after 1.0

These print a row the answer doesn't need, have no value where the sum has one, refuse, or print a constant as written. None of them is a wrong value.

- **Rows for nothing.** Each is excluded where the sum has a value, while the answer has none there:
  - a sum to ∞ from a negative bound: `(K+1)×X^(K+1)×Y^K` from -1 to ∞ prints `X^2×Y÷(X×Y-2×X^2×Y^2+X^3×Y^3); X≠0; Y≠0; ABS(X×Y)<1`, though the sum is 0 at X = 0;
  - `(X÷Y)^K÷X` from 1 to N prints `X≠0`, though the sum has a value at X = 0;
  - past an irrational constant, a letter in a function or a letter at a negative power, the shift that clears a rational ratio's row is refused. `(K-1)×(SQRT(2)×X×Y)^K÷X` from 1 to ∞ keeps `X≠0`, while its rational twin `(K-1)×(X×Y)^K÷X` doesn't, and `K×(SIN(Z)×Y)^K÷SIN(Z)` from 1 to ∞ keeps `Z≠0`.
- **A missing value.** Each has no value at a point where the sum has one, and no row excludes that point:
  - summed to N from a negative lo, at a letter's 0 for N ≤ −2. `X^(K+2)×Y^(-2×K)` from -2 to N prints `X^2×(X÷Y^2)^(N+1)÷(X÷Y^2-1)-Y^4÷(X÷Y^2-1); Y≠0; X÷Y^2≠1`. At X = 0 and N = -2 the sum is Y^4, while X^2×(X÷Y^2)^(-1) has no value;
  - at a zero of a function factor other than its letter's 0: SIN and TAN at π, COS at π÷2, LN at 1. The row the answer prints, `Z≠0`, is the letter's. `K×(SIN(Z)×Y)^K÷SIN(Z)` from 1 to ∞ prints `Y×SIN(Z)÷(SIN(Z)-Y×SIN(Z)^2)-Y^2×SIN(Z)÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2); Z≠0; ABS(Y×SIN(Z))<1`, which has no value at Z = π, where the sum is Y. Summed to N it is the same: `SIN(Z)^(2×K-1)×Y^K` from 1 to N prints `-Y×SIN(Z)÷(Y×SIN(Z)^2-1)-(Y×SIN(Z)^2)^(N+1)÷(SIN(Z)-Y×SIN(Z)^3); Z≠0; Y×SIN(Z)^2≠1`, which has no value at Z = π for every N ≥ 0, where the sum is 0, while its rational twin `X^(2×K-1)×Y^K` doesn't divide by X. It's the same family as the refused shift past a function factor (above).
- `X^K÷Y^K` is NO_CLOSED_FORM, while `(X÷Y)^K` answers.
- A letter-free constant that is exactly 0 but isn't seen to be is printed as written:
  - in a finite sum's terms, Σ 1÷c^K for K = 1 to 3;
  - in a factor 1÷c pulled out of a limit, SIN(X)÷(c×X) at 0.

  Here c = SIN(1)^2+COS(1)^2-1. simplify prints 1÷c the same way.
- The least arena a case needs isn't monotone in the arena it's given: at rc6, two bench cases needed less at 16,384 B than at 12,288 B, with the same answer. Not yet traced.

Also not on 1.0's list: trigonometric identities, matrices, letters as coefficients in solve, and implied products (`2X`).
