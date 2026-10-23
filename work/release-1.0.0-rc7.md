Casimir 1.0.0-rc7, a pre-release: exact computer algebra in portable C11, written for the STU-32 calculator (an ESP32-S3) and usable anywhere C runs. It's MIT licensed and allocates nothing: the caller supplies an output buffer and an arena.

Everything on 0.9.0's list for 1.0 is in, and no wrong answer is known. rc7 fixes rc6's known wrong answer with its siblings (CAS 025), and a second class of the same kind found while fixing it (CAS 026). 1.0.0 itself waits until the calculator runs the last release candidate: its firmware repinned to it and passing on the device. 0.9.0 stays the latest release until then.

## What's new since rc6

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

## How it's tested (at this tag)

- `make test`: 20 programs with 118,421 checks. They read every answer back through an independent evaluator (the calculator firmware's core) and compare values.
- The golden corpus, 10,822 calls compared byte for byte.
- Frame and symbol gates: no frame over 1 KiB, no `pow` linked, and every function's frame against the recorded table.
- Value checks beside the corpus judge Taylor polynomials, limits, integrals and sums against independent constructions: power series over fractions, numeric derivatives at 90 digits, and sums added term by term at 60 digits. A sum's rows are judged at each letter's 0 and at each factor's own zeros (SIN at 0 and π, COS at π÷2, LN at 1), with the factor put to 0 exactly.
- CI on every push: `make check` under gcc and clang, `make test`, CMake under gcc and clang, WebAssembly, and the ESP-IDF component for the ESP32-S3.
- 428 planted mutants, each caught by the tests, run against this tag's code on 2026-10-22 and 23 (work/rc7-mutants.txt).
- Hidden sets kept by the project's reviewer, independent of this repository, rerun at each unit's acceptance.
- On the ESP32-S3, every one of the bench's 1,359 cases is byte-identical to the host:
  - with a 16,384 B arena, the size the calculator will run it with;
  - worst stack 11,360 B read with interrupts masked, plus a 400 B interrupt allowance (the most measured over the masked reading), within a 12 KiB task;
  - nesting deeper than 44 levels is refused (NO_ROOM);
  - about 241 KB of Casimir's code at -O2.

## Not yet, after 1.0

These print a row the answer doesn't need, have no value where the sum has one, refuse, or print a constant as written. None of them is a wrong value.

- **Rows for nothing.** Each is excluded where the sum has a value, while the answer has none there:
  - a sum to ∞ from a negative bound: `(K+1)×X^(K+1)×Y^K` from -1 to ∞ prints `X^2×Y÷(X×Y-2×X^2×Y^2+X^3×Y^3); X≠0; Y≠0; ABS(X×Y)<1`, though the sum is 0 at X = 0;
  - `(X÷Y)^K÷X` from 1 to N prints `X≠0`, though the sum has a value at X = 0;
  - past an irrational constant, a letter in a function or a letter at a negative power, the shift that clears a rational ratio's row is refused. `(K-1)×(SQRT(2)×X×Y)^K÷X` from 1 to ∞ keeps `X≠0`, while its rational twin `(K-1)×(X×Y)^K÷X` doesn't, and `K×(SIN(Z)×Y)^K÷SIN(Z)` from 1 to ∞ keeps `Z≠0`.
- **A missing value.** Summed to N from a negative lo, an answer can have no value at a letter's 0 for N ≤ −2, where the sum has one. `X^(K+2)×Y^(-2×K)` from -2 to N prints `X^2×(X÷Y^2)^(N+1)÷(X÷Y^2-1)-Y^4÷(X÷Y^2-1); Y≠0; X÷Y^2≠1`. At X = 0 and N = -2 the sum is Y^4, while X^2×(X÷Y^2)^(-1) has no value.
- `X^K÷Y^K` is NO_CLOSED_FORM, while `(X÷Y)^K` answers.
- A letter-free constant that is exactly 0 but isn't seen to be is printed as written:
  - in a finite sum's terms, Σ 1÷c^K for K = 1 to 3;
  - in a factor 1÷c pulled out of a limit, SIN(X)÷(c×X) at 0.

  Here c = SIN(1)^2+COS(1)^2-1. simplify prints 1÷c the same way.
- The least arena a case needs isn't monotone in the arena it's given: at rc6, two bench cases needed less at 16,384 B than at 12,288 B, with the same answer. Not yet traced.

Also not on 1.0's list: trigonometric identities, matrices, letters as coefficients in solve, and implied products (`2X`).
