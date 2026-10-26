Casimir 1.2.0: sum's closed form loses the factor each term shares with its denominator, so an answer that was 0÷0 at a letter's 0 or a factor's zero now has the sum's value there. `(K+1)×SIN(Z)^(K+1)×Y^K` from -1 to ∞ printed `Y×SIN(Z)^2÷(Y×SIN(Z)-2×Y^2×SIN(Z)^2+Y^3×SIN(Z)^3); Y≠0; Z≠0; ABS(Y×SIN(Z))<1` in 1.1.2, which is 0÷0 at Z = π, where every term is 0. It now prints `-SIN(Z)÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2); Y≠0; ABS(Y×SIN(Z))<1`, which is 0 there. 1.2.0's code is CAS 031's, accepted at 3bc0b11, with the version text changed. src/casim.h's interface is 1.0's, but answers' bytes change, which is why this is 1.2.0 and not 1.1.3, as for 1.1.0: a moved answer has the same value wherever both lines have one.

## What's new since 1.1.2

- **The common factor, cancelled** (CAS 031). K from lo to hi as said:
  - `(K+1)×X^(K+1)×Y^K` from -1 to ∞ is `-X÷2÷(X×Y-X^2×Y^2÷2-1÷2); Y≠0; ABS(X×Y)<1`, which is 0 at X = 0, as the sum is. 1.1.2 printed `X≠0` there, a row for nothing;
  - `(K-1)×(SQRT(2)×X×Y)^K÷X` from 1 to ∞ is `-X×Y^2÷(X×Y×SQRT(2)-X^2×Y^2-1÷2); ABS(X×Y×SQRT(2))<1`, and `K×(EXP(Z)×X)^K÷X` from 1 to ∞ is `-X×EXP(Z)^2÷2÷(X×EXP(Z)-X^2×EXP(Z)^2÷2-1÷2)-EXP(Z)÷(X×EXP(Z)-1); ABS(X×EXP(Z))<1`, each without 1.1.2's `X≠0`;
  - `K×(SQRT(5)÷3×Y×SIN(Z))^K÷SIN(Z)` from 1 to ∞ is `-Y×SQRT(5)÷(Y×SIN(Z)×SQRT(5)-3)-5×Y^2×SIN(Z)÷6÷(Y×SIN(Z)×SQRT(5)-5×Y^2×SIN(Z)^2÷6-3÷2); ABS(Y×SIN(Z)×SQRT(5)÷3)<1`, which is Y×SQRT(5)÷3 at Z = π, as the sum is. 1.1.2 had no value there;
  - `K×EXP(Z)^(K-1)` from 1 to N, TOO_LONG in 1.1.2, fits: `-1÷(EXP(Z)-1)-N×EXP(Z)^(N+1)÷(EXP(Z)-EXP(Z)^2)-EXP(Z)÷2÷(EXP(Z)-EXP(Z)^2÷2-1÷2)-EXP(Z)^(N+1)÷(EXP(Z)-EXP(Z)^2)+EXP(Z)^(N+1)÷2÷(EXP(Z)-EXP(Z)^2÷2-1÷2); Z≠0; EXP(Z)≠1`.
- **How:** each term n×P^(-1) of the closed form, P a sum, loses the monomial n and P share: for each base n holds at a positive whole power, its least whole power over P's terms. The rows are made after, from the new answer, by the rules as they were. So a letter's row that only the 0÷0 called for goes, and where the answer now has a value at a factor's zero and a term divides by the factor, the factor's row stands: `(1+K)×(SIN(Z)×X×Y)^K` from -1 to ∞ prints `SIN(Z)≠0` in place of 1.1.2's `Z≠0`, and `(1+K)×(ABS(Z)×X×Y)^K` prints `ABS(Z)≠0`, which excludes the same point.
- **It gives way.** Where the cancel runs out of room, or where the cancelled line with its rows would pass 191 B, the line is 1.1.2's. Canon's spelling of a cancelled form can be the longer: `(K+1)×(Z-1)^K×Y^K` from -1 to ∞ prints its 188 B line as 1.1.2 did, never TOO_LONG for the cancel.
- **What moves:** of the 640 function-factor sums the earlier units probed, 31 reword at both arenas, every one shorter or brought under the cap; of 2,640 more, 70 calls. 12 of the documented examples move. The golden corpus moves nowhere. Of the earlier units' hidden sets, 128 runs move (CAS 024's 14, 025's 50, 026's 14, 027's 30, 029's 4, 030's 18), each to a line with the same value wherever both have one; the rest are 1.1.2's byte for byte.
- **Measured:** on the ESP32-S3 at 16,384 B, the five new bench rows take at most 4,992 B of arena and 2,960 B of stack.

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

- `make test`: 20 programs with 118,439 checks. They read every answer back through an independent evaluator (the calculator firmware's core) and compare values.
- The golden corpus, 10,822 calls compared byte for byte: CAS 031 moves none.
- Frame and symbol gates: no frame over 1 KiB, no `pow` linked, and every function's frame against the recorded table. No frame moved from 1.1.2. The new functions take 240 B (sm_cancel), 304 B (sm_fits) and 96 B (sm_pw) on the host, and 176, 176 and 64 B on the S3.
- Value checks beside the corpus judge Taylor polynomials, limits, integrals and sums against independent constructions: power series over fractions, numeric derivatives at 90 digits, and sums added term by term at 60 digits. New in 1.2.0: sums of the shape CAS 031 fixes (to ∞ from a negative lo, the term at lo 0), 60 a run (SIN, COS, TAN and SIN(2×Z) in each angle mode) and 45 held, judged at the factor's zeros, where no value is a failure.
- CI on every push: `make check` under gcc and clang, `make test`, CMake under gcc and clang, WebAssembly, and the ESP-IDF component for the ESP32-S3.
- 464 planted mutants, each a fault the checks must catch: 463 are caught at this tag's tree (work/v1.2.0-mutants.txt), seven of the eight new on CAS 031's cancel among them. The eighth, cancel-rewind-keeps-cache, is named: no input is known where it differs from the build.
- Hidden sets kept by the project's reviewer, independent of this repository, rerun at each unit's acceptance. At CAS 031's: its own, 794 runs at two arenas, judged by value at the letters' and the factors' zeros, by the rows, and by the give-way at the cap; and the earlier units' sets.
- On the ESP32-S3, every one of the bench's 1,384 cases is byte-identical to the host:
  - with a 16,384 B arena, the size the calculator runs it with;
  - worst stack 11,360 B read with interrupts masked, plus a 400 B interrupt allowance (two frames, or the most measured over the masked reading where that is larger), within a 12 KiB task;
  - nesting deeper than 44 levels is refused (NO_ROOM);
  - about 246 KB of Casimir's code at -O2 (246,335 B, 3,113 more than 1.1.2's).

## Not yet, after 1.2.0

These print a line without the note it could have, print a row the answer doesn't need, have no value where the sum has one, refuse, or print a constant as written. None of them is a wrong value.

- **A quotient that canon cancels, or whose divisor falls in K, is still refused.** `X^(2×K)÷X^K` is NO_CLOSED_FORM: canon reads it as `X^K`, which has a value at X = 0, where its terms are 0÷0. So is `X^K÷Y^(-K)`: its divisor has no value at Y = 0, while canon's `(X×Y)^K` has one. Answering them needs a row that `(X÷Y)^K`-style twins don't print (`X≠0`, `Y≠0`).
- **The `÷` spelling reaches the rows of the `×` one.** A quotient prints the answer its canon spelling did in 1.1.0, rows for nothing and missing values included. At a letter's 0, a quotient's terms whose power of the letter is 0 or less have values (0^0 = 1), and there the letter's row excludes the sum's value. `X^K÷Y^K` from 0 to N prints `-1÷(X÷Y-1)+(X÷Y)^(N+1)÷(X÷Y-1); Y≠0; X÷Y≠1`, though at Y = 0 and N = 0 the sum is 1, as `X^K×Y^(-K)` printed in 1.1.0. `X^(K-1)÷Y^(K+2)` from 1 to N prints `X≠0`, though at X = 0 the sum is 1÷Y^3 for every N ≥ 1. Where the factor is one of CAS 030's functions of a letter, its row excludes the point now: `Y^K÷SIN(Z)^K` from 0 to N prints `SIN(Z)≠0`, though at Z = π and N = 0 the sum is 1 (rows for nothing, below). They're the class below, where the shift that clears a rational ratio's row is refused past a letter at a negative power.

- **A note that doesn't fit.** Where the line with the note would pass solve's 191 B, solve prints the line as 1.0.1 did, with no note, never TOO_LONG for it and with no root dropped. `(7×X-2)×(3×X^2-9×X-7)×(8×X^2+3×X-2)×(8×X-3)×(X^2-3×X+8)×(8×X^2+6×X-1)×(1×X^2+9×X+3)` prints its ten real roots in 184 B as `SOLVED: …`, and `, 2 CPLX` would make it 192 B. With `--complex` the same input is TOO_LONG. In a family of degree-12 polynomials built to reach the cap, 59 of 1,416 noted answers give way at one seed and 69 of 1,403 at another; none in the golden corpus does.
- **The complex roots of a factor of degree 3 or more** aren't counted, since solve doesn't find them, with the bit or without it: `X^4+1` is `PARTIAL, DEGREE 4 LEFT`.

- **Rows for nothing.** Each is excluded where the sum has a value, while the answer has none there:
  - a function factor's row to N, at an N before the first term that divides by the factor (CAS 030, as a letter's row): `(Y÷SIN(Z))^K` from 1 to N prints `SIN(Z)≠0`, which excludes Z = π at N = 0, where the sum is 0, as its twin's `X≠0` excludes X = 0. For every N from 1 the sum has no value there, and the row is the one it needs;
  - `(X÷Y)^K÷X` from 1 to N prints `X≠0`, though the sum has a value at X = 0;
  - past an irrational constant, to N, the shift that clears a rational ratio's row is refused, and the cancel (above) doesn't reach a power of N. `(K-1)×(SQRT(2)×X×Y)^K÷X` from 1 to N, TOO_LONG in 1.1.2, now prints `-N×(X×Y×SQRT(2))^(N+1)÷(X-X^2×Y×SQRT(2))-X×Y^2÷(X×Y×SQRT(2)-X^2×Y^2-1÷2)+Y×(X×Y×SQRT(2))^(N+1)×SQRT(2)÷2÷(X×Y×SQRT(2)-X^2×Y^2-1÷2); X≠0; X×Y×SQRT(2)≠1`, though the sum is 0 at X = 0. To ∞ the cancel clears it.
- **A redundant row.** The letter's row can be printed beside a row that already excludes the letter's 0. `K×COS(Z)^(K-1)` from 1 to N prints `Z≠0` beside `COS(Z)≠1`, and COS(0) is 1. It excludes nothing the other row doesn't.
- **A missing value.** Each has no value at a point where the sum has one, and no row excludes that point:
  - at the empty sum, at a letter's 0, from a lo of 1 or more (found after 1.1.0's release, by CAS 029's probe). `(1÷Y)^K` from 1 to N prints `1÷(Y-1)+(1÷Y)^(N+1)÷(1÷Y-1); Y≠1`. At Y = 0 and N = 0 the sum is 0, the empty sum, while the answer has no value. A ratio holding the letter at a negative power gets no row for its 0 from a lo of 1 (abacus #8320 (b)), since every term from 1 has no value there; the empty sum still has one. The same holds for `Y^(-K)`, `(3÷Y)^K` and `(1÷X)^K`;
  - at the empty sum, where a factor outside the ratio has no value (found after 1.1.1's version, by CAS 030's probe; as at 1.1.0). `1÷X` from 1 to N prints `N÷X`, which has no value at X = 0 and N = 0, where the sum is 0. Every term has no value there, and no row excludes the empty sum. The same holds for `Y^K÷X`, and for `K×Y^K÷SIN(Z)` at Z = π;
  - summed to N from a negative lo, at a letter's 0 for N ≤ −2. `X^(K+2)×Y^(-2×K)` from -2 to N prints `X^2×(X÷Y^2)^(N+1)÷(X÷Y^2-1)-Y^4÷(X÷Y^2-1); Y≠0; X÷Y^2≠1`. At X = 0 and N = -2 the sum is Y^4, while X^2×(X÷Y^2)^(-1) has no value;
  - at a zero of a function factor other than its letter's 0, where the same sum with the factor read as a letter, its rational twin, prints that letter's row for nothing. To N, at an N before the first term that divides by the factor, CAS 030 gives SIN, COS, TAN, LN, LOG, ACOS and ACOSH of a letter their row (above); of the 640 probe sums, 4 of 1.1.1's 24 are left, each at the cap. What stays:
    - a line within its row of the cap: where the line with the row would pass 191 B, the row gives way. `K×SIN(Z)^(K-1)` from -1 to N prints its 184 B line as 1.1.1 and 1.1.2 did, with no value at Z = π for N = -2, where the sum is 0, and the row would take it to 196 B. A family of 2,640 such sums has 13;
    - a polynomial factor, whose real zeros casim doesn't know here (Z^2+1 has none). `X^K÷(Y-1)^K` from 1 to N prints `X÷(Y-X×Y÷(Y-1)+X÷(Y-1)-1)+(X÷(Y-1))^(N+1)÷(X÷(Y-1)-1); X÷(Y-1)≠1`, with no value at Y = 1 for N = 0, where the sum is 0;
    - a function of anything but a letter. `(Y÷SIN(2×Z))^K` from 1 to N prints `-Y÷(Y-SIN(2×Z))+(Y÷SIN(2×Z))^(N+1)÷(Y÷SIN(2×Z)-1); Z≠0; Y÷SIN(2×Z)≠1`, with no value at Z = π÷2 for N = 0, where the sum is 0;
- A letter-free constant that is exactly 0 but isn't seen to be is printed as written:
  - in a finite sum's terms, Σ 1÷c^K for K = 1 to 3;
  - in a factor 1÷c pulled out of a limit, SIN(X)÷(c×X) at 0.

  Here c = SIN(1)^2+COS(1)^2-1. simplify prints 1÷c the same way.
- The least arena a case needs isn't monotone in the arena it's given: at rc6, two bench cases needed less at 16,384 B than at 12,288 B, with the same answer. Not yet traced.

Also not on 1.0's list: trigonometric identities, matrices, letters as coefficients in solve, and implied products (`2X`).
