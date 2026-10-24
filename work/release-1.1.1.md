Casimir 1.1.1: sum reads a quotient of powers in K as one ratio. `X^K÷Y^K` from 1 to N was NO_CLOSED_FORM in 1.1.0. It now prints `-X÷(X-Y)+(X÷Y)^(N+1)÷(X÷Y-1); Y≠0; X÷Y≠1`, the answer 1.1.0 already gave for `X^K×Y^(-K)`, which is how canon reads the quotient, and for `(X÷Y)^K`. 1.1.1's code is CAS 029's, accepted at 33b265d, with the version text changed. src/casim.h's interface is 1.0's, and no answer 1.1.0 gave changes. Only sums it refused move: most now answer, and a constant whose sign casim can't decide gives UNDECIDED in place of NO_CLOSED_FORM.

## What's new since 1.1.0

- **A quotient of powers in K, read as one ratio** (CAS 029). Each answer is, byte for byte, 1.1.0's answer for the summand as simplify writes it, K from 1 to N unless said:
  - `2^K÷X^K` is `2÷(X-2)+(2÷X)^(N+1)÷2÷(1÷X-1÷2); X≠2`, as `X^(-K)×2^K` was;
  - `X^(K+1)÷Y^K` is `X×(X÷Y)^(N+1)÷(X÷Y-1)-X^2÷(X-Y); Y≠0; X÷Y≠1`;
  - `K×X^K÷Y^K` from 0 to ∞ is `-X÷2÷(X-Y÷2-X^2÷2÷Y); Y≠0; ABS(X÷Y)<1`;
  - `(X^K+1)÷Y^K` from 0 to N is `-1÷(1÷Y-1)-1÷(X÷Y-1)+(1÷Y)^(N+1)÷(1÷Y-1)+(X÷Y)^(N+1)÷(X÷Y-1); Y≠0; Y≠1; X÷Y≠1`;
  - `SIN(Z)^K÷Y^K` is `SIN(Z)÷(Y-SIN(Z))+(SIN(Z)÷Y)^(N+1)÷(SIN(Z)÷Y-1); Y≠0; SIN(Z)÷Y≠1`.
- **How:** a typed divisor's zeros must survive canon's reading, or the sum refuses (CAS 017). A divisor's power of b, with b free of K, now counts as kept where canon's power of b falls in K, since the ratio then holds b at a negative power: where b = 0 the answer has no value, or its row `b≠0` excludes it.
- **A constant's power in a divisor is read too.** `1÷(-2)^K` is `-2×(-1÷2)^(N+1)÷3-1÷3`, and `1÷(-10^(-9))^K` is `-(-1000000000)^(N+1)÷1000000001-1000000000÷1000000001`, where 1.1.0 refused both. A constant whose sign casim can't decide gives UNDECIDED, as for any ratio (CAS 023): `1÷(SIN(1)^2+COS(1)^2-1)^K`, whose constant is exactly 0.
- **What stays refused:** `X^(2×K)÷X^K`, `Y^K÷Y^K` and `X^K÷Y^(-K)` are NO_CLOSED_FORM, as in 1.1.0 (Not yet, below).
- **What stays as it was, byte for byte:** every answer 1.1.0 gave. The golden corpus moves nowhere, and the hidden sets of earlier units are 1.1.0's byte for byte. The one exception is 52 runs of CAS 022's, each a constant's power in a divisor that 1.1.0 refused (above): 49 now say UNDECIDED, 1 NO_ROOM, and 2 answer, exactly.
- **Measured:** on the ESP32-S3 at 16,384 B, the five new bench rows take at most 3,848 B of arena and 2,592 B of stack.

**The guarantee is unchanged:** an exact answer, or a status that says why not. Never an approximation presented as an answer.

## The known wrong answers

None.

## How it's tested (at this tag)

- `make test`: 20 programs with 118,439 checks. They read every answer back through an independent evaluator (the calculator firmware's core) and compare values.
- The golden corpus, 10,822 calls compared byte for byte: CAS 029 moves none.
- Frame and symbol gates: no frame over 1 KiB, no `pow` linked, and every function's frame against the recorded table. No frame moved from 1.1.0. The two new functions, sm_fall and sm_held, take 80 B and 48 B on the host, and 64 B and 32 B on the S3.
- Value checks beside the corpus judge Taylor polynomials, limits, integrals and sums against independent constructions: power series over fractions, numeric derivatives at 90 digits, and sums added term by term at 60 digits. New in 1.1.1: seeded quotients (80 a run, and 14 held), each checked byte for byte against the answer for simplify's spelling, and by value against its typed terms wherever it has a value, with each letter at 0 in turn; one in eight is a shape that must stay refused.
- CI on every push: `make check` under gcc and clang, `make test`, CMake under gcc and clang, WebAssembly, and the ESP-IDF component for the ESP32-S3.
- [to come: the 450 planted mutants, run at this tag's tree after the version commit, as for 1.1.0]
- Hidden sets kept by the project's reviewer, independent of this repository, rerun at each unit's acceptance. At CAS 029's: its own, 700 runs, judged against the typed sum, the ratio written as one power, and 1.1.0's answer for simplify's spelling; CAS 028's; CAS 005's 3,000 equations; CAS 020's to 027's; and CAS 014's, 015's and 018's.
- On the ESP32-S3, every one of the bench's 1,374 cases is byte-identical to the host:
  - with a 16,384 B arena, the size the calculator runs it with;
  - worst stack 11,360 B read with interrupts masked, plus a 384 B interrupt allowance (two frames, or the most measured over the masked reading where that is larger; another run of this code's base measured 400), within a 12 KiB task;
  - nesting deeper than 44 levels is refused (NO_ROOM);
  - about 243 KB of Casimir's code at -O2 (242,965 B, 433 more than 1.1.0's).

## Not yet, after 1.1.1

These print a line without the note it could have, print a row the answer doesn't need, have no value where the sum has one, refuse, or print a constant as written. None of them is a wrong value.

- **A quotient that canon cancels, or whose divisor falls in K, is still refused.** `X^(2×K)÷X^K` is NO_CLOSED_FORM: canon reads it as `X^K`, which has a value at X = 0, where its terms are 0÷0. So is `X^K÷Y^(-K)`: its divisor has no value at Y = 0, while canon's `(X×Y)^K` has one. Answering them needs a row that `(X÷Y)^K`-style twins don't print (`X≠0`, `Y≠0`).
- **The `÷` spelling reaches the rows of the `×` one.** A quotient prints the answer its canon spelling did in 1.1.0, rows for nothing and missing values included. At a letter's 0, a quotient's terms whose power of the letter is 0 or less have values (0^0 = 1), and there the letter's row excludes the sum's value. `X^K÷Y^K` from 0 to N prints `-1÷(X÷Y-1)+(X÷Y)^(N+1)÷(X÷Y-1); Y≠0; X÷Y≠1`, though at Y = 0 and N = 0 the sum is 1, as `X^K×Y^(-K)` printed in 1.1.0. `X^(K-1)÷Y^(K+2)` from 1 to N prints `X≠0`, though at X = 0 the sum is 1÷Y^3 for every N ≥ 1. Where the factor is a function, the answer has no value there: `Y^K÷SIN(Z)^K` from 0 to N at Z = π and N = 0, where the sum is 1. They're the class below, where the shift that clears a rational ratio's row is refused past a letter at a negative power.

- **A note that doesn't fit.** Where the line with the note would pass solve's 191 B, solve prints the line as 1.0.1 did, with no note, never TOO_LONG for it and with no root dropped. `(7×X-2)×(3×X^2-9×X-7)×(8×X^2+3×X-2)×(8×X-3)×(X^2-3×X+8)×(8×X^2+6×X-1)×(1×X^2+9×X+3)` prints its ten real roots in 184 B as `SOLVED: …`, and `, 2 CPLX` would make it 192 B. With `--complex` the same input is TOO_LONG. In a family of degree-12 polynomials built to reach the cap, 59 of 1,416 noted answers give way at one seed and 69 of 1,403 at another; none in the golden corpus does.
- **The complex roots of a factor of degree 3 or more** aren't counted, since solve doesn't find them, with the bit or without it: `X^4+1` is `PARTIAL, DEGREE 4 LEFT`.

- **Rows for nothing.** Each is excluded where the sum has a value, while the answer has none there:
  - a sum to ∞ from a negative bound: `(K+1)×X^(K+1)×Y^K` from -1 to ∞ prints `X^2×Y÷(X×Y-2×X^2×Y^2+X^3×Y^3); X≠0; Y≠0; ABS(X×Y)<1`, though the sum is 0 at X = 0;
  - `(X÷Y)^K÷X` from 1 to N prints `X≠0`, though the sum has a value at X = 0;
  - past an irrational constant, a factor that is never 0 or a letter at a negative power, the shift that clears a rational ratio's row is refused. `(K-1)×(SQRT(2)×X×Y)^K÷X` from 1 to ∞ keeps `X≠0`, while its rational twin `(K-1)×(X×Y)^K÷X` doesn't, and `K×(EXP(Z)×X)^K÷X` from 1 to ∞ keeps `X≠0`. (Past a letter in a function the shift is made now, above.)
- **A redundant row.** The letter's row can be printed beside a row that already excludes the letter's 0. `K×COS(Z)^(K-1)` from 1 to N prints `Z≠0` beside `COS(Z)≠1`, and COS(0) is 1. It excludes nothing the other row doesn't.
- **A missing value.** Each has no value at a point where the sum has one, and no row excludes that point:
  - at the empty sum, at a letter's 0, from a lo of 1 or more (found after 1.1.0's release, by CAS 029's probe). `(1÷Y)^K` from 1 to N prints `1÷(Y-1)+(1÷Y)^(N+1)÷(1÷Y-1); Y≠1`. At Y = 0 and N = 0 the sum is 0, the empty sum, while the answer has no value. A ratio holding the letter at a negative power gets no row for its 0 from a lo of 1 (abacus #8320 (b)), since every term from 1 has no value there; the empty sum still has one. The same holds for `Y^(-K)`, `(3÷Y)^K` and `(1÷X)^K`;
  - summed to N from a negative lo, at a letter's 0 for N ≤ −2. `X^(K+2)×Y^(-2×K)` from -2 to N prints `X^2×(X÷Y^2)^(N+1)÷(X÷Y^2-1)-Y^4÷(X÷Y^2-1); Y≠0; X÷Y^2≠1`. At X = 0 and N = -2 the sum is Y^4, while X^2×(X÷Y^2)^(-1) has no value;
  - at a zero of a function factor other than its letter's 0 (SIN and TAN at π, COS at π÷2, LN at 1), now only where the same sum with the factor read as a letter, its rational twin, prints that letter's row for nothing. Of the 640 probe sums, 1.0.0 had this at 62 and 1.0.1 has it at 24 (the first shape below); the other two shapes were found beyond those sets:
    - to N, at an N before the first term that divides by the factor, where the sum has a value (the empty sum, or a sum of terms that don't divide) and the terms after it divide. `(Y÷SIN(Z))^K` from 1 to N prints `-Y÷(Y-SIN(Z))+(Y÷SIN(Z))^(N+1)÷(Y÷SIN(Z)-1); Z≠0; Y÷SIN(Z)≠1`, with no value at Z = π for N = 0, where the sum is 0. Its twin `(Y÷X)^K` prints `X≠0`, which excludes X = 0 at N = 0 too. Over the probe sets there are 24 of these: with F one of SIN(Z) and COS(Z), `K×F^(K-1)` from -1 and 0, `(F×Y)^K` from -1, `(Y÷F)^K` from -1, 0, 1 and 2, and `(K-1)×(F×Y)^K÷F` from 0; with F one of SIN(Z), COS(Z), TAN(Z) and LN(Z), `F^(2×K-1)×Y^K` and `F^(3×K-1)×Y^K` from 0;
    - to ∞ from a negative lo, where the term at lo is 0: `(K+1)×SIN(Z)^(K+1)×Y^K` from -1 to ∞ prints `Y×SIN(Z)^2÷(Y×SIN(Z)-2×Y^2×SIN(Z)^2+Y^3×SIN(Z)^3); Y≠0; Z≠0; ABS(Y×SIN(Z))<1`, which is 0÷0 at Z = π, where every term is 0. It's the function-factor face of the first row for nothing above. A grid of such sums found 12: `(K+1)×F^(K+1)×Y^K` and `(K+1)×F^(K+2)×Y^K` from -1, and `(K+2)×F^(K+2)×Y^K` from -2, with F one of SIN(Z), COS(Z), TAN(Z) and LN(Z);
    - past a constant beside the factor: `K×(SQRT(5)÷3×Y×SIN(Z))^K÷SIN(Z)` from 1 to ∞ prints `Y×SIN(Z)×SQRT(5)÷3÷(SIN(Z)-Y×SIN(Z)^2×SQRT(5)÷3)-5×Y^2×SIN(Z)÷6÷(Y×SIN(Z)×SQRT(5)-5×Y^2×SIN(Z)^2÷6-3÷2); Z≠0; ABS(Y×SIN(Z)×SQRT(5)÷3)<1`, with no value at Z = π, where the sum is Y×SQRT(5)÷3. The constant stops the shift (above), and its twin keeps `X≠0`. The same holds for COS, TAN and LN.
- A letter-free constant that is exactly 0 but isn't seen to be is printed as written:
  - in a finite sum's terms, Σ 1÷c^K for K = 1 to 3;
  - in a factor 1÷c pulled out of a limit, SIN(X)÷(c×X) at 0.

  Here c = SIN(1)^2+COS(1)^2-1. simplify prints 1÷c the same way.
- The least arena a case needs isn't monotone in the arena it's given: at rc6, two bench cases needed less at 16,384 B than at 12,288 B, with the same answer. Not yet traced.

Also not on 1.0's list: trigonometric identities, matrices, letters as coefficients in solve, and implied products (`2X`).
