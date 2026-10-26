Casimir 1.1.2: sum's answer to N excludes a function factor's zero by its row, as its answer for a letter does. `(Y÷SIN(Z))^K` from 1 to N had no value at Z = π in 1.1.1, for N = 0, where the sum is 0, and no row said so. It now prints `-Y÷(Y-SIN(Z))+(Y÷SIN(Z))^(N+1)÷(Y÷SIN(Z)-1); Z≠0; SIN(Z)≠0; Y÷SIN(Z)≠1`, as its twin `(Y÷X)^K` prints `X≠0`. 1.1.2's code is CAS 030's, accepted at 8462e77, with the version text changed. src/casim.h's interface is 1.0's, and no answer's value changes: a line gains one row, or stays as 1.1.1 printed it, byte for byte.

## What's new since 1.1.1

- **A function factor's row to N** (CAS 030). Each line is 1.1.1's plus one row, the factor's own, K from 1 to N unless said:
  - `TAN(Z)^(2×K-1)×Y^K` from 0 to N is `1÷(TAN(Z)-Y×TAN(Z)^3)-(Y×TAN(Z)^2)^(N+1)÷(TAN(Z)-Y×TAN(Z)^3); Z≠0; TAN(Z)≠0; Y×TAN(Z)^2≠1`;
  - `(Y÷LOG(Z))^K` is `-Y÷(Y-LOG(Z))+(Y÷LOG(Z))^(N+1)÷(Y÷LOG(Z)-1); Z≠0; LOG(Z)≠0; Y÷LOG(Z)≠1`;
  - `(Y÷ACOS(Z))^K` from 0 to N is `-1÷(Y÷ACOS(Z)-1)+(Y÷ACOS(Z))^(N+1)÷(Y÷ACOS(Z)-1); ACOS(Z)≠0; Y÷ACOS(Z)≠1`;
  - in GRAD, `(Y÷COS(Z))^K` is `-Y÷(Y-COS(Z))+(Y÷COS(Z))^(N+1)÷(Y÷COS(Z)-1); COS(Z)≠0; Y÷COS(Z)≠1`. The row is the factor's own text in RAD, DEG and GRAD alike, since the test puts the factor to 0, not Z to a zero; GRAD's zero of COS is 100.
- **How:** a factor of the ratio gets its row where a term from lo divides by it (CAS 026), unless the answer itself has no value at the factor's zero. To N the sum has a value there at the empty sum, or at an N before the first term that divides, so that point was missing. Now, to N (an upper bound neither ∞ nor a whole number), SIN, COS, TAN, LN, LOG, ACOS and ACOSH of a letter keep their row there too, as a letter's row stands where the answer has no value at 0. ASIN, ATAN, SINH, TANH, ASINH and ATANH are 0 only where their letter is, and the letter's own row (`Z≠0`) already excludes that.
- **It gives way at the cap.** Where the line with the row would pass 191 B, the line is printed without it, as 1.1.0's note for solve gives way, never TOO_LONG for the row (Not yet, below).
- **What stays as it was, byte for byte:** every other answer 1.1.1 gave, and every value. The golden corpus moves nowhere. Of the earlier units' hidden sets, 36 runs gain the row (CAS 027's 20, CAS 029's 16), and the rest are 1.1.1's byte for byte.
- **Measured:** on the ESP32-S3 at 16,384 B, the five new bench rows take at most 4,312 B of arena and 2,608 B of stack.

**The guarantee is unchanged:** an exact answer, or a status that says why not. Never an approximation presented as an answer.

## The known wrong answers

None.

**Found after this release, on 25 Oct 2026.** Summed from 0 to ∞, `(Z÷Y+1)^K` prints `-Y÷Z; Z≠0; ABS(Z÷Y+1)<1`. Every term from K = 1 has no value at Y = 0, so neither has the sum, while the answer does: it reads 0 there, and no row excludes the point (a row with no value there doesn't exclude it). `(Z÷(Y-1)+1)^K` at Y = 1, `(Z÷Y^2+1)^K` at Y = 0, and `(Z÷SIN(Y)+1)^K` wherever SIN(Y) is 0 do the same to ∞. Summed to N, none of them has a value where the sum has none. The project's reviewer found it while testing a later unit. The fix is CAS 033, in the release after 1.2.0, which adds the rows.

## How it's tested (at this tag)

- `make test`: 20 programs with 118,439 checks. They read every answer back through an independent evaluator (the calculator firmware's core) and compare values.
- The golden corpus, 10,822 calls compared byte for byte: CAS 030 moves none.
- Frame and symbol gates: no frame over 1 KiB, no `pow` linked, and every function's frame against the recorded table. No frame moved from 1.1.1. The new function, sm_rput, takes 80 B on the host and 48 B on the S3.
- Value checks beside the corpus judge Taylor polynomials, limits, integrals and sums against independent constructions: power series over fractions, numeric derivatives at 90 digits, and sums added term by term at 60 digits. New in 1.1.2: a function factor's family in each angle mode (80 a run, and 18 held), judged at the mode's own zeros, where a missing value of the shape CAS 030 rows is a failure unless its row would pass the cap; and TAN and the inverse functions read in the mode.
- CI on every push: `make check` under gcc and clang, `make test`, CMake under gcc and clang, WebAssembly, and the ESP-IDF component for the ESP32-S3.
- 456 planted mutants, each a fault the checks must catch: all 456 are caught at this tag's tree (work/v1.1.2-mutants.txt), CAS 030's six among them.
- Hidden sets kept by the project's reviewer, independent of this repository, rerun at each unit's acceptance. At CAS 030's: its own, 1,040 runs in RAD, DEG and GRAD, judged by the byte shape (1.1.1's line plus exactly one row, the factor's own, within 191 B), at the mode's own zeros, and by value; and the earlier units' sets.
- On the ESP32-S3, every one of the bench's 1,379 cases is byte-identical to the host:
  - with a 16,384 B arena, the size the calculator runs it with;
  - worst stack 11,360 B read with interrupts masked, plus a 384 B interrupt allowance (two frames, or the most measured over the masked reading where that is larger), within a 12 KiB task;
  - nesting deeper than 44 levels is refused (NO_ROOM);
  - about 243 KB of Casimir's code at -O2 (243,222 B, 257 more than 1.1.1's).

## Not yet, after 1.1.2

These print a line without the note it could have, print a row the answer doesn't need, have no value where the sum has one, refuse, or print a constant as written. None of them is a wrong value.

- **A quotient that canon cancels, or whose divisor falls in K, is still refused.** `X^(2×K)÷X^K` is NO_CLOSED_FORM: canon reads it as `X^K`, which has a value at X = 0, where its terms are 0÷0. So is `X^K÷Y^(-K)`: its divisor has no value at Y = 0, while canon's `(X×Y)^K` has one. Answering them needs a row that `(X÷Y)^K`-style twins don't print (`X≠0`, `Y≠0`).
- **The `÷` spelling reaches the rows of the `×` one.** A quotient prints the answer its canon spelling did in 1.1.0, rows for nothing and missing values included. At a letter's 0, a quotient's terms whose power of the letter is 0 or less have values (0^0 = 1), and there the letter's row excludes the sum's value. `X^K÷Y^K` from 0 to N prints `-1÷(X÷Y-1)+(X÷Y)^(N+1)÷(X÷Y-1); Y≠0; X÷Y≠1`, though at Y = 0 and N = 0 the sum is 1, as `X^K×Y^(-K)` printed in 1.1.0. `X^(K-1)÷Y^(K+2)` from 1 to N prints `X≠0`, though at X = 0 the sum is 1÷Y^3 for every N ≥ 1. Where the factor is one of CAS 030's functions of a letter, its row excludes the point now: `Y^K÷SIN(Z)^K` from 0 to N prints `SIN(Z)≠0`, though at Z = π and N = 0 the sum is 1 (rows for nothing, below). They're the class below, where the shift that clears a rational ratio's row is refused past a letter at a negative power.

- **A note that doesn't fit.** Where the line with the note would pass solve's 191 B, solve prints the line as 1.0.1 did, with no note, never TOO_LONG for it and with no root dropped. `(7×X-2)×(3×X^2-9×X-7)×(8×X^2+3×X-2)×(8×X-3)×(X^2-3×X+8)×(8×X^2+6×X-1)×(1×X^2+9×X+3)` prints its ten real roots in 184 B as `SOLVED: …`, and `, 2 CPLX` would make it 192 B. With `--complex` the same input is TOO_LONG. In a family of degree-12 polynomials built to reach the cap, 59 of 1,416 noted answers give way at one seed and 69 of 1,403 at another; none in the golden corpus does.
- **The complex roots of a factor of degree 3 or more** aren't counted, since solve doesn't find them, with the bit or without it: `X^4+1` is `PARTIAL, DEGREE 4 LEFT`.

- **Rows for nothing.** Each is excluded where the sum has a value, while the answer has none there:
  - a function factor's row to N, at an N before the first term that divides by the factor (CAS 030, as a letter's row): `(Y÷SIN(Z))^K` from 1 to N prints `SIN(Z)≠0`, which excludes Z = π at N = 0, where the sum is 0, as its twin's `X≠0` excludes X = 0. For every N from 1 the sum has no value there, and the row is the one it needs;
  - a sum to ∞ from a negative bound: `(K+1)×X^(K+1)×Y^K` from -1 to ∞ prints `X^2×Y÷(X×Y-2×X^2×Y^2+X^3×Y^3); X≠0; Y≠0; ABS(X×Y)<1`, though the sum is 0 at X = 0;
  - `(X÷Y)^K÷X` from 1 to N prints `X≠0`, though the sum has a value at X = 0;
  - past an irrational constant, a factor that is never 0 or a letter at a negative power, the shift that clears a rational ratio's row is refused. `(K-1)×(SQRT(2)×X×Y)^K÷X` from 1 to ∞ keeps `X≠0`, while its rational twin `(K-1)×(X×Y)^K÷X` doesn't, and `K×(EXP(Z)×X)^K÷X` from 1 to ∞ keeps `X≠0`. (Past a letter in a function the shift is made now, above.)
- **A redundant row.** The letter's row can be printed beside a row that already excludes the letter's 0. `K×COS(Z)^(K-1)` from 1 to N prints `Z≠0` beside `COS(Z)≠1`, and COS(0) is 1. It excludes nothing the other row doesn't.
- **A missing value.** Each has no value at a point where the sum has one, and no row excludes that point:
  - at the empty sum, at a letter's 0, from a lo of 1 or more (found after 1.1.0's release, by CAS 029's probe). `(1÷Y)^K` from 1 to N prints `1÷(Y-1)+(1÷Y)^(N+1)÷(1÷Y-1); Y≠1`. At Y = 0 and N = 0 the sum is 0, the empty sum, while the answer has no value. A ratio holding the letter at a negative power gets no row for its 0 from a lo of 1 (abacus #8320 (b)), since every term from 1 has no value there; the empty sum still has one. The same holds for `Y^(-K)`, `(3÷Y)^K` and `(1÷X)^K`;
  - at the empty sum, where a factor outside the ratio has no value (found after 1.1.1's version, by CAS 030's probe; as at 1.1.0). `1÷X` from 1 to N prints `N÷X`, which has no value at X = 0 and N = 0, where the sum is 0. Every term has no value there, and no row excludes the empty sum. The same holds for `Y^K÷X`, and for `K×Y^K÷SIN(Z)` at Z = π;
  - summed to N from a negative lo, at a letter's 0 for N ≤ −2. `X^(K+2)×Y^(-2×K)` from -2 to N prints `X^2×(X÷Y^2)^(N+1)÷(X÷Y^2-1)-Y^4÷(X÷Y^2-1); Y≠0; X÷Y^2≠1`. At X = 0 and N = -2 the sum is Y^4, while X^2×(X÷Y^2)^(-1) has no value;
  - at a zero of a function factor other than its letter's 0, where the same sum with the factor read as a letter, its rational twin, prints that letter's row for nothing. To N, at an N before the first term that divides by the factor, CAS 030 gives SIN, COS, TAN, LN, LOG, ACOS and ACOSH of a letter their row (above); of the 640 probe sums, 4 of 1.1.1's 24 are left, each at the cap. What stays:
    - a line within its row of the cap: where the line with the row would pass 191 B, the row gives way. `K×SIN(Z)^(K-1)` from -1 to N prints its 184 B line as 1.1.1 did, with no value at Z = π for N = -2, where the sum is 0, and the row would take it to 196 B. A family of 2,640 such sums has 13;
    - a polynomial factor, whose real zeros casim doesn't know here (Z^2+1 has none). `X^K÷(Y-1)^K` from 1 to N prints `X÷(Y-X×Y÷(Y-1)+X÷(Y-1)-1)+(X÷(Y-1))^(N+1)÷(X÷(Y-1)-1); X÷(Y-1)≠1`, with no value at Y = 1 for N = 0, where the sum is 0;
    - a function of anything but a letter. `(Y÷SIN(2×Z))^K` from 1 to N prints `-Y÷(Y-SIN(2×Z))+(Y÷SIN(2×Z))^(N+1)÷(Y÷SIN(2×Z)-1); Z≠0; Y÷SIN(2×Z)≠1`, with no value at Z = π÷2 for N = 0, where the sum is 0;
    - to ∞ from a negative lo, where the term at lo is 0: `(K+1)×SIN(Z)^(K+1)×Y^K` from -1 to ∞ prints `Y×SIN(Z)^2÷(Y×SIN(Z)-2×Y^2×SIN(Z)^2+Y^3×SIN(Z)^3); Y≠0; Z≠0; ABS(Y×SIN(Z))<1`, which is 0÷0 at Z = π, where every term is 0. It's the function-factor face of the first row for nothing above. A grid of such sums found 12: `(K+1)×F^(K+1)×Y^K` and `(K+1)×F^(K+2)×Y^K` from -1, and `(K+2)×F^(K+2)×Y^K` from -2, with F one of SIN(Z), COS(Z), TAN(Z) and LN(Z);
    - past a constant beside the factor: `K×(SQRT(5)÷3×Y×SIN(Z))^K÷SIN(Z)` from 1 to ∞ prints `Y×SIN(Z)×SQRT(5)÷3÷(SIN(Z)-Y×SIN(Z)^2×SQRT(5)÷3)-5×Y^2×SIN(Z)÷6÷(Y×SIN(Z)×SQRT(5)-5×Y^2×SIN(Z)^2÷6-3÷2); Z≠0; ABS(Y×SIN(Z)×SQRT(5)÷3)<1`, with no value at Z = π, where the sum is Y×SQRT(5)÷3. The constant stops the shift (above), and its twin keeps `X≠0`. The same holds for COS, TAN and LN.
- A letter-free constant that is exactly 0 but isn't seen to be is printed as written:
  - in a finite sum's terms, Σ 1÷c^K for K = 1 to 3;
  - in a factor 1÷c pulled out of a limit, SIN(X)÷(c×X) at 0.

  Here c = SIN(1)^2+COS(1)^2-1. simplify prints 1÷c the same way.
- The least arena a case needs isn't monotone in the arena it's given: at rc6, two bench cases needed less at 16,384 B than at 12,288 B, with the same answer. Not yet traced.

Also not on 1.0's list: trigonometric identities, matrices, letters as coefficients in solve, and implied products (`2X`).
