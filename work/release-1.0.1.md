Casimir 1.0.1: a fast follow to 1.0.0. In most of the cases where 1.0.0's closed form for a sum had no value at a zero of a function factor of its ratio, 1.0.1's has the sum's value there. 1.0.0 printed no wrong value in those cases: its answers had no value at those points, and its notes named them. 1.0.1's code is CAS 027's, accepted at cfdb18f, with the version text changed. src/casim.h's interface is 1.0.0's.

## What's new since 1.0.0

- **The shift past a function factor** (CAS 027). A sum q(K)×B^K whose B holds a function of a letter, SIN(Z), COS(Z), TAN(Z), LN(Z), SQRT(Z), ABS(Z) or Z^(1÷3), is shifted past it as past a letter. Where the shift clears the factor, the answer no longer divides by it.
  - `K×(SIN(Z)×Y)^K÷SIN(Z)` from 1 to ∞ is `-Y÷(Y×SIN(Z)-1)-Y^2×SIN(Z)÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2); ABS(Y×SIN(Z))<1`. That is Y at Z = π, as the sum is. 1.0.0 printed a form that divides by SIN(Z), with no value at π, and `Z≠0`.
  - Summed to N it is the same: `SIN(Z)^(2×K-1)×Y^K` from 1 to N is `Y×SIN(Z)×(Y×SIN(Z)^2)^N÷(Y×SIN(Z)^2-1)-Y×SIN(Z)÷(Y×SIN(Z)^2-1); Y×SIN(Z)^2≠1`, which is 0 at Z = π for every N, as the sum is.
  - At the letter's own 0, from a negative lo: `(SQRT(Z)×Y)^K×SQRT(Z)` from -1 to N is `1÷(Y-Y^2×SQRT(Z))-(Y×SQRT(Z))^(N+2)÷(Y-Y^2×SQRT(Z)); Y≠0; Y×SQRT(Z)≠1`, which is 0 at Z = 0 and N = -2, the empty sum. 1.0.0's form had no value there.
- **What comes with it:** rows for nothing dropped, and sums that were TOO_LONG answered.
  - `K×(SIN(Z)×Y)^K÷Y` from 1 to ∞ is `-Y×SIN(Z)^2÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2)-SIN(Z)÷(Y×SIN(Z)-1); ABS(Y×SIN(Z))<1`, with no `Y≠0`: the sum is SIN(Z) at Y = 0, and so is the answer.
  - `K×SIN(Z)^(K-1)` from 1 to N was TOO_LONG. It's now `-1÷(SIN(Z)-1)+N×SIN(Z)^N÷(SIN(Z)-1)-SIN(Z)÷2÷(SIN(Z)-SIN(Z)^2÷2-1÷2)+SIN(Z)^N÷(SIN(Z)-1)+SIN(Z)^(N+1)÷2÷(SIN(Z)-SIN(Z)^2÷2-1÷2); SIN(Z)≠1`.
- **What stays as it was, byte for byte:**
  - Where the shift wouldn't clear the factor, the answer is 1.0.0's. From 0, the first term of `K×SIN(Z)^(K-1)` is 0×SIN(Z)^(-1), with no value where SIN(Z) is 0, so `K×SIN(Z)^(K-1)` from 0 to ∞ is still `-1÷2÷(SIN(Z)-SIN(Z)^2÷2-1÷2); SIN(Z)≠0; ABS(SIN(Z))<1`.
  - A factor that is never 0 (EXP, COSH, 2^Z) and a constant (SQRT(2)) still stop the shift. `K×(EXP(Z)×X)^K÷X` from 1 to ∞ keeps `X≠0` (below).
- **Measured** over CAS 026's two probe sets, 640 sums: seven factors in eight shapes from -1 to 2, and eight factors at powers 2 and 3 from 0 and 1, each to N and to ∞. The sums with a missing value at a factor's zero, 65 at 1.0.0, are 24. 105 sums print something new:
  - 41 gain a value;
  - 42 drop a row for nothing;
  - 14 that were TOO_LONG answer;
  - 8 change form only.

  At 9,758 points, at the factors' zeros and off them, each answer's value is the sum's.

**The guarantee is unchanged:** an exact answer, or a status that says why not. Never an approximation presented as an answer.

## The known wrong answers

None.

**Found after this release, on 25 Oct 2026.** Summed from 0 to ∞, `(Z÷Y+1)^K` prints `-Y÷Z; Z≠0; ABS(Z÷Y+1)<1`. Every term from K = 1 has no value at Y = 0, so neither has the sum, while the answer does: it reads 0 there, and no row excludes the point (a row with no value there doesn't exclude it). `(Z÷(Y-1)+1)^K` at Y = 1, `(Z÷Y^2+1)^K` at Y = 0, and `(Z÷SIN(Y)+1)^K` wherever SIN(Y) is 0 do the same to ∞. Summed to N, none of them has a value where the sum has none. The project's reviewer found it while testing a later unit. The fix is CAS 033, in the release after 1.2.0, which adds the rows.

**Found after this release, on 26 Oct 2026.** canon may drop a part of the input that has no value at some points, since its domain rule lets it define a result where the input has none (u÷u is 1, u-u and 0×u are 0, u^0 is 1, EXP(LN(u)) and ALOG(LOG(u)) are u, SQRT(u)^2 is u). sum, defint and limit then print a value where the input as typed has none:
- Summed from 1 to N, `(Y÷Y)^K` prints `N`. At Y = 0 every term is 0÷0, so the sum has no value, while `N` has one. Summed from 0 to ∞, `(Z×LN(Y)÷LN(Y))^K` prints `-1÷(Z-1); ABS(Z)<1`, which has a value at Y = 1, where every term holds LN(1)÷LN(1), and at every Y ≤ 0, where LN(Y) has none.
- Summed from 0 to ∞, `(Z×EXP(LN(Y))+1)^K` prints `-1÷Y÷Z; Y≠0; Z≠0; ABS(Y×Z+1)<1`. At Y = -1 and Z = 1÷2 every term holds LN(-1), which has no real value, so the sum has none, while the answer is 2 there and each row holds. With ALOG(LOG(Y)), SQRT(Y)^2 or SQRT(Y)×SQRT(Y) in place of EXP(LN(Y)) it prints the same line.
- `defint "SQRT(X)^2" X -2 -1` prints `-3÷2`, though SQRT(X) has no real value on the interval (`SQRT(X)` over it is IMPROPER). `SQRT(X)×SQRT(X)` and `(X^(1÷4))^4` print the same, and `0×SQRT(X)` prints `0`.
- `limit "EXP(LN(X))" X -1 ''` prints `-1`, though LN(X) has no real value near -1.
- With another letter, a 0÷0 reaches limit and defint as well: `defint "X×Y÷Y" X 0 1` prints `1÷2` and `limit "X×Y÷Y" X 1 ''` prints `1`, each with a value at Y = 0, where the input has none.
- Where the sum's closed form cancels a divisor of the summand: summed from 1 to ∞, `K×(X×Y)^K÷Y` prints `-X÷(X×Y-1)-X^2×Y÷2÷(X×Y-X^2×Y^2÷2-1÷2); ABS(X×Y)<1`. At Y = 0 the term at K = 1 is 0÷0, so the sum has no value, while the answer is X. From 1.0.1, `K×(SIN(Z)×Y)^K÷SIN(Z)` does the same where SIN(Z) is 0, and `(K-1)×(TAN(Z)×Y)^K÷TAN(Z)` where TAN(Z) is 0 (1.0.0 printed both with no value there).

limit also reads a function outside its real domain as typed, with no canon involved: `limit "SQRT(X)" X -1 ''` prints `SQRT(-1)`, and from below 0 it prints `0`, though SQRT(X) has no real value near -1 or below 0.

simplify promises no more than the domain rule (docs/canonical-form.md), and expand, collect, subst, derive and integrate answer through canon under the same rule. taylor and the polynomial ops read what was typed: `taylor "EXP(LN(X))" X -1 2` prints `UNDEFINED`, and solve, factor, cancel, partfrac and pdiv refuse these forms (NOT_POLYNOMIAL). 1.0.0 to 1.3.0 print every line above. The project's reviewer found it while testing a later unit, and a fix is being made (CAS 035).

## How it's tested (at this tag)

- `make test`: 20 programs with 118,421 checks. They read every answer back through an independent evaluator (the calculator firmware's core) and compare values.
- The golden corpus, 10,822 calls compared byte for byte.
- Frame and symbol gates: no frame over 1 KiB, no `pow` linked, and every function's frame against the recorded table. No frame moved from 1.0.0; the one new function, sm_atom, takes 144 B on the host and 96 B on the S3.
- Value checks beside the corpus judge Taylor polynomials, limits, integrals and sums against independent constructions: power series over fractions, numeric derivatives at 90 digits, and sums added term by term at 60 digits. A sum's rows are judged at each letter's 0 and at each factor's own zeros (SIN at 0 and π, COS at π÷2, LN at 1), with the factor put to 0 exactly. New in 1.0.1: at each such zero where the sum has a value, the answer must have one, and it must be the sum's.
- CI on every push: `make check` under gcc and clang, `make test`, CMake under gcc and clang, WebAssembly, and the ESP-IDF component for the ESP32-S3.
- 435 planted mutants, each caught by the tests. All 435 were run at this tag's tree and caught, 435 of 435 (work/v1.0.1-mutants.txt): rc7's 428 and the seven new on CAS 027's code.
- Hidden sets kept by the project's reviewer, independent of this repository, rerun at each unit's acceptance. At CAS 027's: its own, and CAS 014's to 026's.
- On the ESP32-S3, every one of the bench's 1,364 cases is byte-identical to the host:
  - with a 16,384 B arena, the size the calculator runs it with;
  - worst stack 11,360 B read with interrupts masked, plus a 400 B interrupt allowance (the most measured over the masked reading), within a 12 KiB task;
  - nesting deeper than 44 levels is refused (NO_ROOM);
  - about 242 KB of Casimir's code at -O2 (241,853 B, 772 more than 1.0.0's).

## Not yet, after 1.0.1

These print a row the answer doesn't need, have no value where the sum has one, refuse, or print a constant as written. None of them is a wrong value.

- **Rows for nothing.** Each is excluded where the sum has a value, while the answer has none there:
  - a sum to ∞ from a negative bound: `(K+1)×X^(K+1)×Y^K` from -1 to ∞ prints `X^2×Y÷(X×Y-2×X^2×Y^2+X^3×Y^3); X≠0; Y≠0; ABS(X×Y)<1`, though the sum is 0 at X = 0;
  - `(X÷Y)^K÷X` from 1 to N prints `X≠0`, though the sum has a value at X = 0;
  - past an irrational constant, a factor that is never 0 or a letter at a negative power, the shift that clears a rational ratio's row is refused. `(K-1)×(SQRT(2)×X×Y)^K÷X` from 1 to ∞ keeps `X≠0`, while its rational twin `(K-1)×(X×Y)^K÷X` doesn't, and `K×(EXP(Z)×X)^K÷X` from 1 to ∞ keeps `X≠0`. (Past a letter in a function the shift is made now, above.)
- **A redundant row.** The letter's row can be printed beside a row that already excludes the letter's 0. `K×COS(Z)^(K-1)` from 1 to N prints `Z≠0` beside `COS(Z)≠1`, and COS(0) is 1. It excludes nothing the other row doesn't.
- **A missing value.** Each has no value at a point where the sum has one, and no row excludes that point:
  - summed to N from a negative lo, at a letter's 0 for N ≤ −2. `X^(K+2)×Y^(-2×K)` from -2 to N prints `X^2×(X÷Y^2)^(N+1)÷(X÷Y^2-1)-Y^4÷(X÷Y^2-1); Y≠0; X÷Y^2≠1`. At X = 0 and N = -2 the sum is Y^4, while X^2×(X÷Y^2)^(-1) has no value;
  - at a zero of a function factor other than its letter's 0 (SIN and TAN at π, COS at π÷2, LN at 1), now only where the same sum with the factor read as a letter, its rational twin, prints that letter's row for nothing. Of the 640 probe sums, 1.0.0 had this at 62 and 1.0.1 has it at 24 (the first shape below); the other two shapes were found beyond those sets:
    - to N, at an N before the first term that divides by the factor, where the sum has a value (the empty sum, or a sum of terms that don't divide) and the terms after it divide. `(Y÷SIN(Z))^K` from 1 to N prints `-Y÷(Y-SIN(Z))+(Y÷SIN(Z))^(N+1)÷(Y÷SIN(Z)-1); Z≠0; Y÷SIN(Z)≠1`, with no value at Z = π for N = 0, where the sum is 0. Its twin `(Y÷X)^K` prints `X≠0`, which excludes X = 0 at N = 0 too. Over the probe sets there are 24 of these: with F one of SIN(Z) and COS(Z), `K×F^(K-1)` from -1 and 0, `(F×Y)^K` from -1, `(Y÷F)^K` from -1, 0, 1 and 2, and `(K-1)×(F×Y)^K÷F` from 0; with F one of SIN(Z), COS(Z), TAN(Z) and LN(Z), `F^(2×K-1)×Y^K` and `F^(3×K-1)×Y^K` from 0;
    - to ∞ from a negative lo, where the term at lo is 0: `(K+1)×SIN(Z)^(K+1)×Y^K` from -1 to ∞ prints `Y×SIN(Z)^2÷(Y×SIN(Z)-2×Y^2×SIN(Z)^2+Y^3×SIN(Z)^3); Y≠0; Z≠0; ABS(Y×SIN(Z))<1`, which is 0÷0 at Z = π, where every term is 0. It's the function-factor face of the first row for nothing above. A grid of such sums found 12: `(K+1)×F^(K+1)×Y^K` and `(K+1)×F^(K+2)×Y^K` from -1, and `(K+2)×F^(K+2)×Y^K` from -2, with F one of SIN(Z), COS(Z), TAN(Z) and LN(Z);
    - past a constant beside the factor: `K×(SQRT(5)÷3×Y×SIN(Z))^K÷SIN(Z)` from 1 to ∞ prints `Y×SIN(Z)×SQRT(5)÷3÷(SIN(Z)-Y×SIN(Z)^2×SQRT(5)÷3)-5×Y^2×SIN(Z)÷6÷(Y×SIN(Z)×SQRT(5)-5×Y^2×SIN(Z)^2÷6-3÷2); Z≠0; ABS(Y×SIN(Z)×SQRT(5)÷3)<1`, with no value at Z = π, where the sum is Y×SQRT(5)÷3. The constant stops the shift (above), and its twin keeps `X≠0`. The same holds for COS, TAN and LN.
- `X^K÷Y^K` is NO_CLOSED_FORM, while `(X÷Y)^K` answers.
- A letter-free constant that is exactly 0 but isn't seen to be is printed as written:
  - in a finite sum's terms, Σ 1÷c^K for K = 1 to 3;
  - in a factor 1÷c pulled out of a limit, SIN(X)÷(c×X) at 0.

  Here c = SIN(1)^2+COS(1)^2-1. simplify prints 1÷c the same way.
- The least arena a case needs isn't monotone in the arena it's given: at rc6, two bench cases needed less at 16,384 B than at 12,288 B, with the same answer. Not yet traced.

Also not on 1.0's list: trigonometric identities, matrices, letters as coefficients in solve, and implied products (`2X`).
