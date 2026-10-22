Casimir 1.0.0-rc6, a pre-release: exact computer algebra in portable C11, written for the STU-32 calculator (an ESP32-S3) and usable anywhere C runs. It's MIT licensed and allocates nothing: the caller supplies an output buffer and an arena.

Everything on 0.9.0's list for 1.0 is in. One wrong answer is known (below); it's fixed next, as CAS 025, in 1.0.0-rc7. 1.0.0 itself waits until the calculator runs the last release candidate: its firmware repinned to it and passing on the device. 0.9.0 stays the latest release until then.

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

**The guarantee is unchanged:** an exact answer, or a status that says why not. Never an approximation presented as an answer.

## The known wrong answer

`casim sum 'K×(SQRT(2)×X×Y)^K÷Y' K 0 ∞ RAD` prints

    -X×SQRT(2)÷2÷(X×Y×SQRT(2)-X^2×Y^2-1÷2); ABS(X×Y×SQRT(2))<1

with no `Y≠0`. The term at K = 0 is 0×Y^(-1), which has no value at Y = 0, so neither has the sum, while the answer does. With a rational constant the row is there: `K×(X×Y)^K÷Y` gives `…; Y≠0; ABS(X×Y)<1`. An irrational constant takes a route that misses the row. CAS 025 fixes it for 1.0.0-rc7. That unit also covers the bug's siblings: other irrational constants (π among them), other zero terms that divide, and sums to N as well as to ∞.

## How it's tested (at this tag)

- `make test`: 20 programs with 118,421 checks. They read every answer back through an independent evaluator (the calculator firmware's core) and compare values.
- The golden corpus, 10,822 calls compared byte for byte.
- Frame and symbol gates: no frame over 1 KiB, no `pow` linked.
- Value checks beside the corpus judge Taylor polynomials, limits, integrals and sums against independent constructions: power series over fractions, numeric derivatives at 90 digits, and sums added term by term at 60 digits.
- CI on every push: `make check` under gcc and clang, `make test`, CMake under gcc and clang, WebAssembly, and the ESP-IDF component for the ESP32-S3.
- 414 planted mutants, each caught by the tests, run against this tag's code on 2026-10-22 (work/rc6-mutants.txt).
- Hidden sets kept by the project's reviewer, independent of this repository, rerun at each unit's acceptance.
- On the ESP32-S3, every one of the bench's 1,347 cases is byte-identical to the host:
  - with a 16,384 B arena, the size the calculator will run it with;
  - worst stack 11,360 B read with interrupts masked, plus a 384 B interrupt allowance, within a 12 KiB task;
  - nesting deeper than 44 levels is refused (NO_ROOM);
  - about 240 KB of Casimir's code at -O2.

## Not yet, after 1.0

These print a row the answer doesn't need, refuse, or print a constant as written. None of them is a wrong value.

- A sum to ∞ from a negative bound can keep a row for nothing: `(K+1)×X^(K+1)×Y^K` from -1 to ∞ prints `X^2×Y÷(X×Y-2×X^2×Y^2+X^3×Y^3); X≠0; Y≠0; ABS(X×Y)<1`, though the sum is 0 at X = 0.
- `(X÷Y)^K÷X` from 1 to N prints `X≠0`, though the sum has a value at X = 0.
- `X^K÷Y^K` is NO_CLOSED_FORM, while `(X÷Y)^K` answers.
- A letter-free constant that is exactly 0 but isn't seen to be is printed as written:
  - in a finite sum's terms, Σ 1÷c^K for K = 1 to 3;
  - in a factor 1÷c pulled out of a limit, SIN(X)÷(c×X) at 0.

  Here c = SIN(1)^2+COS(1)^2-1. simplify prints 1÷c the same way.
- The least arena a case needs isn't monotone in the arena it's given: two bench cases need less at 16,384 B than at 12,288 B, with the same answer. Not yet traced.

Also not on 1.0's list: trigonometric identities, matrices, letters as coefficients in solve, and implied products (`2X`).
